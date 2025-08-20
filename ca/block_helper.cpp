#include "block_helper.h"


#include "ca/test.h"
#include "ca/checker.h"
#include "ca/algorithm.h"
#include "ca/block_http_callback.h"
#include "ca/transaction_cache.h"
#include "ca/double_spend_cache.h"
#include "ca/sync_block.h"
#include "ca/resend_reconnect_node.h"

#include "common.pb.h"
#include "common/task_pool.h"
#include "common/global_data.h"

#include "utils/account_manager.h"
#include "utils/magic_singleton.h"
#include "utils/bench_mark.h"
#include "utils/contract_utils.h"

#include "db/db_api.h"
#include "net/interface.h"
#include "include/scope_guard.h"
#include "ca/evm/evm_manager.h"

static global::ca::SaveType g_syncType = global::ca::SaveType::Unknow;

BlockHelper::BlockHelper() : missing_prehash(false){}

int getUtxoAndFindNode(uint32_t num, uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &node_ids_to_send)
{
    return SyncBlock::getSyncNodeSimplified(num, chainHeight, pledgeAddr, node_ids_to_send);
}

int sendBlockByUtxoRequest(const std::string &utxo)
{
    if(!MagicSingleton<BlockHelper>::GetInstance()->getWhetherRunSendBlockByUtxoRequest())
    {
        DEBUGLOG("rollbackPreviousBlocks is running");
        return 0;
    }
    MagicSingleton<BlockHelper>::GetInstance()->setWhetherRunSendBlockByUtxoRequest(false);

    ON_SCOPE_EXIT{
        MagicSingleton<BlockHelper>::GetInstance()->popMissUtxo();
        MagicSingleton<BlockHelper>::GetInstance()->setWhetherRunSendBlockByUtxoRequest(true);
    };

    DEBUGLOG("begin get missing block utxo {}",utxo);
    std::vector<std::string> node_ids_to_send;

    uint64_t chainHeight = 0;
    if(!BlockHelper::getChainHeight(chainHeight))
    {
        return -1;
    }
    uint64_t nodeSelfHeight = 0;
    std::vector<std::string> pledgeAddr;
    DBReader dbReader;
    {
        auto status = dbReader.getBlockTop(nodeSelfHeight);
        if (DBStatus::DB_SUCCESS != status)
        {
            return -2;
        }

        std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();

        for (const auto &node : nodelist)
        {
            int ret = VerifyBonusAddr(node.address);

            int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
            if (stakeTime > 0 && ret == 0)
            {
                pledgeAddr.push_back(node.address);
            }
        }
    }
    
    if (getUtxoAndFindNode(global::ca::MIN_SYNC_QUAL_NODES, chainHeight, pledgeAddr, node_ids_to_send) != 0)
    {
        ERRORLOG("get sync node fail");
        return -4;
    }


    std::string msgId;
    size_t sendNum = node_ids_to_send.size();
    if (!dataMgrPtr.CreateWait(30, sendNum * 0.8, msgId))
    {
        return -5;
    }
    std::string selfNodeId = MagicSingleton<PeerNode>::GetInstance()->GetSelfId();
    for (auto &nodeId : node_ids_to_send)
    {
        GetBlockByUtxoReq req;
        req.set_addr(selfNodeId);
        req.set_utxo(utxo);
        req.set_msg_id(msgId);
        if(!dataMgrPtr.AddResNode(msgId, nodeId))
        {
            return -6;
        }
        NetSendMessage<GetBlockByUtxoReq>(nodeId, req, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    }

    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        if(!SyncBlock::checkByzantineFault(sendNum, retDatas.size()))
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
            return -7;
        }
    }
    GetBlockByUtxoAck ack;
    std::string blockRaw = "";
    for(auto iter = retDatas.begin(); iter != retDatas.end(); iter++)
    {
        ack.Clear();
        if (!ack.ParseFromString(*iter))
        {
            continue;
        }
        if(iter == retDatas.begin())
        {
            blockRaw = ack.block_raw();
        }
        else
        {
            if( blockRaw != ack.block_raw())
            {
                ERRORLOG("get different block");
                return -8;
            }
        }
    }

    if(blockRaw == "")
    {
        ERRORLOG("blockRaw is empty!");
        return -9;
    }

    CBlock block;
    if(!block.ParseFromString(blockRaw))
    {
        ERRORLOG("blockRaw parse fail!");
        return -10;
    }

    std::string strHeader;
    if (DBStatus::DB_SUCCESS == dbReader.getBlockByBlockHash(block.hash(), strHeader)) 
    {
        DEBUGLOG("sendBlockByUtxoRequest error in blockHash:{} , now run rollbackPreviousBlocks to find utxo: {}",block.hash(), utxo);
        MagicSingleton<SyncBlock>::GetInstance()->ThreadStop();
        int ret = MagicSingleton<BlockHelper>::GetInstance()->rollbackPreviousBlocks(utxo, nodeSelfHeight, block.hash());
        MagicSingleton<SyncBlock>::GetInstance()->ThreadStart(true);
        if(ret != 0)
        {
            ERRORLOG("rollbackPreviousBlocks fail, fail num: {}", ret);
            return -11;
        }
    }


    MagicSingleton<BlockHelper>::GetInstance()->AddMissingBlock(block);
    
    return 0;
}

int BlockHelper::rollbackPreviousBlocks(const std::string utxo, uint64_t shelfHeight, const std::string blockHash)
{

    DEBUGLOG("running rollbackPreviousBlocks");
    DBReader dbReader;
    uint64_t chainHeight = 0;
    if(!MagicSingleton<BlockHelper>::GetInstance()->getChainHeight(chainHeight))
    {
        ERRORLOG("getChainHeight error -1");
        return -1;
    }
    if(chainHeight < shelfHeight + 50)
    {
        ERRORLOG("chainHeight > shelfHeight  -2");
        return -2;
    }
    for(int i = shelfHeight / 100 * 100; i > 0; --i)
    {
        std::vector<std::string> selfBlockHashes_;
        if (DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(i, i, selfBlockHashes_))
        {
            ERRORLOG("getBlockHashesByBlockHeight error -3");
            return -3;
        }

        CBlock tempBlock;
        for(const auto& self_block_hashe_renamed_1: selfBlockHashes_)
        { 
            std::string strblock;
            auto res = dbReader.getBlockByBlockHash(self_block_hashe_renamed_1, strblock);
            if (DBStatus::DB_SUCCESS != res)
            {
                ERRORLOG("getBlockByBlockHash failed -4");
                return -4;
            }

            if(!tempBlock.ParseFromString(strblock))
            {
                ERRORLOG("blockRaw parse fail! -5");
                return -5;
            }

            for(const auto& tx : tempBlock.txs())
            {     
                for(const auto& txUtxo : tx.utxos())
                {
                    for(const auto& vin: txUtxo.vin())
                    {
                        for(const auto& prevOutput: vin.prevout())
                        {
                            if(prevOutput.hash() == utxo && tempBlock.hash() != blockHash)
                            {
                                DEBUGLOG("SetFastSync height: {}", tempBlock.height());
                                SyncBlock::SetFastSync(tempBlock.height());
                                return 0;
                            }
                        } 
                    }
                }
            }
        }
    }

    return -6;
}



int sendBlockByHashRequest(const std::map<std::string, bool> &missingHashs)
{
    DEBUGLOG("sendBlockByHashRequest Start");
    std::vector<std::string> node_ids_to_send;

    uint64_t chainHeight = 0;
    if(!BlockHelper::getChainHeight(chainHeight))
    {
        return -1;
    }
    uint64_t nodeSelfHeight = 0;
    std::vector<std::string> pledgeAddr;
    {
        DBReader dbReader;
        auto status = dbReader.getBlockTop(nodeSelfHeight);
        if (DBStatus::DB_SUCCESS != status)
        {
            return -2;
        }
        std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
        
        for (const auto &node : nodelist)
        {
            int ret = VerifyBonusAddr(node.address);

            int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
            if (stakeTime > 0 && ret == 0)
            {
                pledgeAddr.push_back(node.address);
            }
        }
    }
    
    if (getUtxoAndFindNode(global::ca::MIN_SYNC_QUAL_NODES, chainHeight, pledgeAddr, node_ids_to_send) != 0)
    {
        ERRORLOG("get sync node fail");
        return -4;
    }


    std::string msgId;
    size_t sendNum = node_ids_to_send.size();
    if (!dataMgrPtr.CreateWait(30, sendNum * 0.8, msgId))
    {
        return -5;
    }
    GetBlockByHashReq req;
    for(auto &it : missingHashs)
    {
        auto missingHash = req.add_missinghashs();
        missingHash->set_hash(it.first);
        missingHash->set_tx_or_block(it.second);
    }

    std::string selfNodeId = MagicSingleton<PeerNode>::GetInstance()->GetSelfId();
    req.set_addr(selfNodeId);
    req.set_msg_id(msgId);

    for (auto &nodeId : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, nodeId))
        {
            return -6;
        }
        NetSendMessage<GetBlockByHashReq>(nodeId, req, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    }

    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        if(!SyncBlock::checkByzantineFault(sendNum, retDatas.size()))
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
            return -7;
        }
    }

    GetBlockByHashAck ack;
    uint32_t successCounter = 0;
    std::map<std::string, std::pair<std::string, uint32_t>> blockHashesToSeek;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        successCounter++;
        for (auto &block : ack.blocks())
        {
            if (blockHashesToSeek.end() == blockHashesToSeek.find(block.hash()))
            {
                blockHashesToSeek[block.hash()].first = std::move(block.block_raw());
                blockHashesToSeek[block.hash()].second = 1;
            }
            blockHashesToSeek[block.hash()].second++;
        }
    }

    uint32_t verifyNum = successCounter / 5 * 4;
    std::vector<std::pair<CBlock,std::string>> seekBlocks;
    for(const auto& it : blockHashesToSeek)
    {
        if(it.second.second > verifyNum)
        {
            CBlock block;
            if(!block.ParseFromString(it.second.first))
            {
                ERRORLOG("blockRaw parse fail!");
                return -8;
            }
            seekBlocks.push_back({block, it.first});
        }
    }

    MagicSingleton<TaskPool>::GetInstance()->CommitSyncBlockJob(std::bind(&BlockHelper::add_seek_block, MagicSingleton<BlockHelper>::GetInstance().get(), seekBlocks));
    return 0;
}

int seekBlockByContractPreHashRequest(const std::string &blockHashToSeek, std::string& contractBlockString)
{
    DEBUGLOG("seekBlockByContractPreHashRequest Start");
    std::vector<std::string> node_ids_to_send;

    uint64_t chainHeight = 0;
    if(!BlockHelper::getChainHeight(chainHeight))
    {
        return -1;
    }
    uint64_t nodeSelfHeight = 0;
    std::vector<std::string> pledgeAddr;
    {
        DBReader dbReader;
        auto status = dbReader.getBlockTop(nodeSelfHeight);
        if (DBStatus::DB_SUCCESS != status)
        {
            return -2;
        }

        std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();

        for (const auto &node : nodelist)
        {
            int ret = VerifyBonusAddr(node.address);

            int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
            if (stakeTime > 0 && ret == 0)
            {
                pledgeAddr.push_back(node.address);
            }
        }
    }
    
    if (getUtxoAndFindNode(global::ca::MIN_SYNC_QUAL_NODES, chainHeight, pledgeAddr, node_ids_to_send) != 0)
    {
        ERRORLOG("get sync node fail");
        return -4;
    }


    std::string msgId;
    size_t sendNum = node_ids_to_send.size();
    if (!dataMgrPtr.CreateWait(2, sendNum / 2, msgId))
    {
        return -5;
    }
    GetBlockByHashReq req;
    auto missingHash = req.add_missinghashs();
    missingHash->set_hash(blockHashToSeek);
    missingHash->set_tx_or_block(false);
    
    std::string selfNodeId = MagicSingleton<PeerNode>::GetInstance()->GetSelfId();
    req.set_addr(selfNodeId);
    req.set_msg_id(msgId);

    for (auto &node_id : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, node_id))
        {
            return -6;
        }
        NetSendMessage<GetBlockByHashReq>(node_id, req, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    }

    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        if(!SyncBlock::checkByzantineFault(sendNum, retDatas.size()))
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
            return -7;
        }
    }

    GetBlockByHashAck ack;
    std::map<std::string, std::pair<std::string, uint32_t>> blockHashesToSeek;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        for (auto &iter : ack.blocks())
        {
            contractBlockString = iter.block_raw();
            return 0;
        }
    }

    return -8;
}



int BlockHelper::verifyFlowedBlockStatus(const CBlock& block, BlockStatus* blockStatus , BlockMsg *msg)
{
    if( block.version() != global::ca::kCurrentBlockVersion)
	{
		return -1;
	}
    bool isVerify = true;
    auto selfAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(GenerateAddr(block.sign(0).pub()) == selfAddr)
    {
        isVerify = false;
    }
    uint64_t blockHeight = block.height();
    
	std::string blockHash = block.hash();
	if(blockHash.empty())
    {
        return -2;
    }
    
    DBReadWriter dbWriter;
	uint64_t nodeHeight = 0;
    if (DBStatus::DB_SUCCESS != dbWriter.getBlockTop(nodeHeight))
    {
        return -3;
    }

    if ( (nodeHeight  > 9) && (nodeHeight - 9 > blockHeight))
	{
        ERRORLOG("VerifyHeight fail!!,blockHeight:{}, nodeHeight:{}, isVerify:{}",blockHeight, nodeHeight, isVerify);
		return -4;
	}
	else if (nodeHeight + 1 < blockHeight)
	{
        ERRORLOG("VerifyHeight fail!!,blockHeight:{}, nodeHeight:{}, isVerify:{}",blockHeight, nodeHeight, isVerify);
		return -5;
	}

    uint64_t chainHeight = 0;
    if(!getChainHeight(chainHeight))
    {
        return -6;
    }

    //Increase the height and time of the block within a certain height without judgment
    if(chainHeight > global::ca::MIN_UNSTAKE_HEIGHT)
    {
        uint64_t currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        const static uint64_t kStabilityTime = 60 * 1000000;
        if(blockHeight < (chainHeight - 10) && currentTime - block.time() > kStabilityTime)
        {
            DEBUGLOG("broadcast block overtime , block height{}, block hash{}",blockHeight,blockHash);
            return -7;
        }
    }
    
	std::string tempHeader;

	DBStatus status = dbWriter.getBlockByBlockHash(blockHash, tempHeader);
	if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
	{
		ERRORLOG("get block not success or not found ");
		return -8;
	}

	if (tempHeader.size() != 0)
	{
		return -9;
	}

	std::string prev_header_str;
	status = dbWriter.getBlockByBlockHash(block.prevhash(), prev_header_str);
	if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
	{
		ERRORLOG("get block not success or not found ");
		return -10;
	}

	if (prev_header_str.size() == 0)
	{
        return -11;
	}
    std::vector<CTransaction> double_spent_transactions;
    Checker::CheckConflict(block, double_spent_transactions);
    if(!double_spent_transactions.empty())
    {
        if(blockStatus != NULL)
        {
            for(const auto& tx : double_spent_transactions)
            {
                auto txStatus = blockStatus->add_txstatus();
                txStatus->set_txhash(tx.hash());
                txStatus->set_status(global::ca::DoubleSpend::SingleBlock);
            }
            
        }
        std::ostringstream filestream;
        PrintBlock(block,true,filestream);

        std::string testStr = filestream.str();
        DEBUGLOG("double_spent_transactions block --> {}", testStr);
        return -12;
    }
    auto startT5 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    if(block.sign_size() >= 1)
    {
        DEBUGLOG("verifying block {} , isVerify:{}, addr:{}", blockHash.substr(0, 6), isVerify, GenerateAddr(block.sign(0).pub()));
    }
	auto ret = ca_algorithm::VerifyBlock(block, false, true, isVerify, blockStatus,msg);

	if (0 != ret)
	{
		ERRORLOG("verify block fail ret:{}:{}:{}", ret, blockHeight, blockHash);
		return -13;
	}
    
    if(!isVerify){
        auto endT5 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        auto t5 = endT5 - startT5;
        MagicSingleton<Benchmark>::GetInstance()->setByBlockHash(block.hash(), &t5, 3);
    }
    return 0;
}

int BlockHelper::SaveBlock(const CBlock& block, global::ca::SaveType saveType, global::ca::blockMean computeMeanValue)
{    
    DBReadWriter* dbWriterInstance = new DBReadWriter();
    ON_SCOPE_EXIT{
        if (dbWriterInstance != nullptr)
        {
            delete dbWriterInstance;
            dbWriterInstance = nullptr;
        }
        if (saveType == global::ca::SaveType::Broadcast)
        {
            DEBUGLOG("SAVETEST hash: {} , BlockHelper::SaveBlock end: {}", block.hash(), MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
        }
    };


    uint64_t nodeHeight = 0;
    if (DBStatus::DB_SUCCESS != dbWriterInstance->getBlockTop(nodeHeight))
    {
         ERRORLOG("getBlockTop error!");
        return -3;
    }

    if (block.height() >= nodeHeight + 2)
    {
        ERRORLOG("block.height:{} >= (nodeHeight + 2):{}", block.height(), nodeHeight + 2);
        return -4;
    }

    int ret = 0;
    std::string blockRaw;
    std::string blockHash = block.hash();
    ret = dbWriterInstance->getBlockByBlockHash(block.hash(), blockRaw);
    if (DBStatus::DB_SUCCESS == ret)
    {
        INFOLOG("BlockHelper block {} already in saved , skip",block.hash().substr(0, 6));
        return 0;
    }

    ret = PreSaveProcess(block, saveType, computeMeanValue);
    if (ret < 0)
    {
        ERRORLOG("PreSaveProcess ret : {}", ret);
        return -5;
    }
    DEBUGLOG("PreSaveProcess doubleSpendCheck ret:{}", ret);
    
    resetMissingPrehash();
    uint64_t blockHeight = block.height();
    ret = ca_algorithm::SaveBlock(*dbWriterInstance, block, saveType, computeMeanValue);
    if (0 != ret)
    {
        ERRORLOG("save block ret:{}:{}:{}", ret, blockHeight, blockHash);
        if(saveType == global::ca::SaveType::SyncNormal || saveType == global::ca::SaveType::SyncFromZero)
        {
            DEBUGLOG("run new sync, start height: {}", blockHeight);
            SyncBlock::set_new_sync_height(blockHeight);
        }
        if (missing_prehash)
        {
            resetMissingPrehash();
            DEBUGLOG("run new sync, start height: {}", blockHeight - 1);
            SyncBlock::set_new_sync_height(blockHeight - 1);
            return -6;
        }
        if(!missingUtxos.empty())
        {
            getMissingBlock();
            return -7;
        }
        return -8;
    }
    if(DBStatus::DB_SUCCESS != dbWriterInstance->transactionCommit())
    {     
        ERRORLOG("Transaction commit fail");
        return -9;   
    }

    for(auto iter = contractBlocks.begin(); iter != contractBlocks.end();)
    {
        DEBUGLOG("delete double block hash:{}", block.hash());
        if(block.hash() == iter->second.second.hash())
        {
            contractBlocks.erase(iter++);
        }
        else
        {
            ++iter;
        }
    }


    MagicSingleton<doubleSpendCache>::GetInstance()->Detection(block);
    MagicSingleton<ResendReconnectNode>::GetInstance()->RemoveResendBlock(block.hash());

    INFOLOG("save block ret:{}:{}:{}", ret, blockHeight, blockHash);
    auto startTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    PostSaveProcess(block);
    auto endTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp(); 
    commitCostAfter += (endTime - startTime);
    committedPostCount++;

    return 0;
}

bool BlockHelper::VerifyHeight(const CBlock& block, uint64_t ownblockHeight)
{
    DBReader dbReader;

	unsigned int preheight = 0;
	if (DBStatus::DB_SUCCESS != dbReader.getBlockHeightByBlockHash(block.prevhash(), preheight))
	{
		ERRORLOG("get block height failed,block.prehash() = {} ,block.hash() = {}, preheight = {} " ,block.prevhash(),block.hash(),preheight);
		return false;
	}

	if(ownblockHeight > (preheight + 5))
	{
		return false;
	}
	return true;
}

void BlockHelper::processPostMembershipCancellation(const CBlock &block)
{
    for (int i = 0; i < block.txs_size(); i++)
    {
        CTransaction tx = block.txs(i);
        if (GetTransactionType(tx) != kTransactionTypeTx)
        {
            continue;
        }

        global::ca::TxType txType;
        txType = (global::ca::TxType)tx.txtype();

        if (global::ca::TxType::kTxTypeUnstake_ == txType || global::ca::TxType::kTxTypeUndelegate == txType)
        {
            DBReadWriter dbWriter;
            std::vector<std::string> blockHashs;
            uint64_t blockHeight = block.height();
            if (DBStatus::DB_SUCCESS != dbWriter.getBlockHashsByBlockHeight(blockHeight, blockHashs))
            {
                ERRORLOG("fail to get block hash at height {}", blockHeight);
                continue;
            }
            std::vector<std::string> blocks;
            if (DBStatus::DB_SUCCESS != dbWriter.getBlocksByBlockHash(blockHashs, blocks))
            {
                ERRORLOG("fail to get block at height {}", blockHeight);
                continue;
            }
            
            for (auto &blockRaw : blocks)
            {                                                                               
                CBlock heightBlock;                
                if (!heightBlock.ParseFromString(blockRaw))
                {
                    ERRORLOG("block parse fail!");
                    continue;
                }
                if(heightBlock.hash() == block.hash())
                {
                    continue;
                }
                for (int i = 0; i < heightBlock.txs_size(); i++)
                {
                    CTransaction height_tx = heightBlock.txs(i);
                    bool shouldUseAgent = TxHelper::requiresAgent(tx);

                    for (int i = (shouldUseAgent ? 0 : 1); i < block.sign_size(); ++i)
                    {
                        std::string signAddr = GenerateAddr(block.sign(i).pub());
                       for(const auto& utxo : tx.utxos())
                        {
                            if(std::find(utxo.owner().begin(), utxo.owner().end(), signAddr) != utxo.owner().end())
                            {
                                int ret = RollbackBlock(heightBlock.hash(), heightBlock.height());
                                if (ret != 0)
                                {
                                    ERRORLOG("rollback hash {} fail, ret: ", heightBlock.hash(), ret);
                                }
                            }     
                        }                   
                    }
                }

            }
        }
    }
}

std::pair<DOUBLE_SPEND_TYPE, CBlock> BlockHelper::dealDoubleSpend(const CBlock& block, const CTransaction& tx, const std::string& missingUtxo)
{
    uint64_t blockHeight = block.height();
    std::string blockHash = block.hash();

    DBReader dbReader;
    uint64_t nodeHeight = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(nodeHeight))
    {
        ERRORLOG("getBlockTop fail!!!");
        return {DOUBLE_SPEND_TYPE::err,{}};
    }
    
    std::set<std::string> setOwner;
	for(const auto& utxo : tx.utxos())
	{
		for(const auto& owner : utxo.owner())
		{
			setOwner.insert(owner);
		}
	}

    std::vector<std::string> blockHashes;
    if(blockHeight > nodeHeight)
    {
        DEBUGLOG("blockHeight:({}) > nodeHeight:({})", blockHeight, nodeHeight);
        return {DOUBLE_SPEND_TYPE::err,{}};
    }
    if (DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(blockHeight, nodeHeight, blockHashes))
    {
        ERRORLOG("getBlockHashesByBlockHeight fail!!!");
        return {DOUBLE_SPEND_TYPE::err,{}};
    }
    std::vector<std::string> blocks;
    if (DBStatus::DB_SUCCESS != dbReader.getBlocksByBlockHash(blockHashes, blocks))
    {
        ERRORLOG("getBlocksByBlockHash fail!!!");
        return {DOUBLE_SPEND_TYPE::err,{}};
    }

    for (auto &PBlockStr : blocks)
    {
        CBlock PBlock;
        if(PBlock.ParseFromString(PBlockStr))
        {
            for(const auto& PTx : PBlock.txs())
            {
                if((global::ca::TxType)PTx.txtype() != global::ca::TxType::TX_TYPE_TX)
                {
                    continue;                              
                }
                for (const auto &utxo : PTx.utxos()){
                    for (auto &PVin : utxo.vin())
                    {
                        std::string inputAddress = GenerateAddr(PVin.vinsign().pub());
                        if (setOwner.find(inputAddress) != setOwner.end())
                        {
                            for (auto &PPrevout : PVin.prevout())
                            {
                                std::string PUtxo = PPrevout.hash();
                                if (missingUtxo == PUtxo)
                                {
                                    DEBUGLOG("DoubleSpend, blockHeight:{}, PBlock.height:{} , block_time:{}, PBlock.time:{}, blockHash:{}, PBlockHash:{}", blockHeight, PBlock.height(), block.time(), PBlock.time(), block.hash().substr(0, 6), PBlock.hash().substr(0, 6));
                                    // same height doublespend
                                    if ((blockHeight == PBlock.height() && block.time() >= PBlock.time()) || blockHeight > PBlock.height())
                                    {
                                        DEBUGLOG("doubleSpendBlocks.insert(blockHash):{}", blockHash.substr(0, 6));
                                        doubleSpendBlocks.insert({blockHash, block});
                                        checkDoubleBlooming({DOUBLE_SPEND_TYPE::isDoubleSpend, std::move(PBlock)}, block);
                                        return {DOUBLE_SPEND_TYPE::isDoubleSpend, std::move(PBlock)};
                                    }
                                    else
                                    {
                                        DEBUGLOG("doubleSpendBlocks PBlock roll oldBackHash:{} at newBlockHash:{}", PBlock.hash().substr(0, 6), blockHash.substr(0, 6));
                                        auto ret = RollbackBlock(PBlock.hash(), PBlock.height());
                                        if (ret != 0)
                                        {
                                            ERRORLOG("PBlock rollback hash {} fail, ret:{}", PBlock.hash(), ret);
                                            return {DOUBLE_SPEND_TYPE::err, {}};
                                        }
                                        checkDoubleBlooming({DOUBLE_SPEND_TYPE::previousDoubleSpend, std::move(PBlock)}, block);
                                        return {DOUBLE_SPEND_TYPE::previousDoubleSpend, std::move(PBlock)};
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    DEBUGLOG("PBlock Not found doubleSpendBlocks.insert(blockHash):{}", blockHash);
    doubleSpendBlocks.insert({blockHash, block});
    return {DOUBLE_SPEND_TYPE::isInvalidDoubleSpend,{}};
}

int BlockHelper::PreSaveProcess(const CBlock& block, global::ca::SaveType saveType, global::ca::blockMean computeMeanValue)
{
    uint64_t blockHeight = block.height();
    std::string blockHash = block.hash();
    if(saveType == global::ca::SaveType::SyncNormal)
    {
        DEBUGLOG("verifying block {}", blockHash.substr(0, 6));
        resetMissingPrehash();
        auto ret = ca_algorithm::VerifyBlock(block, true, false, true, nullptr, nullptr, saveType);
        if (0 != ret)
        {
            ERRORLOG("verify block ret:{}:{}:{}", ret, blockHeight, blockHash);
            if (missing_prehash)
            {
                resetMissingPrehash();
                DEBUGLOG("run new sync, start height: {}", blockHeight - 1);
                SyncBlock::set_new_sync_height(blockHeight - 1);
                return -1;
            }
            if(!missingUtxos.empty())
            {
                getMissingBlock();
                return -2;
            }
            return -3;
        }
    }
    else if(saveType == global::ca::SaveType::Broadcast)
    {
        DBReader dbReader;
        uint64_t nodeHeight = 0;
        if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(nodeHeight))
        {
            return -4;
        }
        if(computeMeanValue == global::ca::blockMean::Normal && blockHeight + 50 < nodeHeight)
        {
            DEBUGLOG("blockHeight + 50 < nodeHeight");
            return -5;
        }

        if(ca_algorithm::verifyPreSaveBlock_(block) < 0)
        {
            ERRORLOG("Verify PreSave Block fail!");
            return -6;
        }
        
        for (auto& tx : block.txs())
        {
            if (GetTransactionType(tx) != kTransactionTypeTx)
            {
                continue;
            }
            std::string missingUtxo;
            int ret = ca_algorithm::doubleSpendCheck(tx, false, &missingUtxo);
            if (0 != ret)
            {
                if(ret == -5 || ret == -7 || ret == -8 && !missingUtxo.empty())
                {
                    std::string blockHash;
                    if(dbReader.getBlockHashByTransactionHash(missingUtxo, blockHash) == DBStatus::DB_SUCCESS)
                    {
                        DEBUGLOG("doubleSpendCheck fail!! <utxo>: {}, ", missingUtxo);
                        auto double_spend_data = dealDoubleSpend(block, tx , missingUtxo);
                        if(double_spend_data.first == DOUBLE_SPEND_TYPE::isDoubleSpend)
                        {
                            return -7;
                        }
                        else if(double_spend_data.first == DOUBLE_SPEND_TYPE::previousDoubleSpend)
                        {
                            DEBUGLOG("previousDoubleSpend,blockHash:{}, txHash:{}", block.hash().substr(0,6), tx.hash().substr(0,10));
                            continue;
                        }
                        else if(double_spend_data.first == DOUBLE_SPEND_TYPE::isInvalidDoubleSpend)
                        {
                            return -8;
                        }
                        else
                        {
                            return -9;
                        }
                    }
                    else
                    {
                        DEBUGLOG("not found!! <utxo>: {}, ", missingUtxo);
                        uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
                        std::unique_lock<std::mutex> locker(seekMutex);
                        _missingBlocks.insert({missingUtxo, nowTime, 1});
                    }
                }

                auto found = hashPendingBlocks.find(block.hash());
                if(found == hashPendingBlocks.end())
                {
                    hashPendingBlocks[block.hash()] = {MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp(), block};
                }
                
                DEBUGLOG("doubleSpendCheck fail!! block height:{}, hash:{}, ret: {}, ", block.height(), block.hash().substr(0,6), ret);
                return ret;
            }
        }
        DEBUGLOG("++++++block height:{}, Hash:{}",block.height(), block.hash().substr(0,6));
    }
    return 0;
}

void BlockHelper::processPostTransaction(const CBlock &block)
{
    if (isContractBlocked(block))
    {
        MagicSingleton<TransactionCache>::GetInstance()->contractBlockNotification(block.hash());
    }

    MagicSingleton<PeerNode>::GetInstance()->SetSelfHeight(block.height());

    // Run http callback
    if (MagicSingleton<BlockHttpCallbackHandler>::GetInstance()->IsRunning())
    {
        MagicSingleton<BlockHttpCallbackHandler>::GetInstance()->AddBlock(block);
    }
}

void BlockHelper::PostSaveProcess(const CBlock &block)
{
    MagicSingleton<Benchmark>::GetInstance()->add_block_pool_save_map_end(block.hash());
    MagicSingleton<TaskPool>::GetInstance()->commitCaTask(std::bind(&BlockHelper::processPostTransaction, this, block));

    auto found = _pendingBlocks.find(block.height() + 1);
    if (found != _pendingBlocks.end())
    {
        auto& blocks = found->second;
        auto targetBegin = blocks.lower_bound(block.hash());
        auto target_end = blocks.upper_bound(block.hash());
        for (; targetBegin != target_end ; targetBegin++)
        {
            DEBUGLOG("_pendingBlocks Add block height:{}, hash:{}", targetBegin->second.height(), targetBegin->second.hash().substr(0,6));
            SaveBlock(targetBegin->second, global::ca::SaveType::Broadcast, global::ca::blockMean::processByPreHash);
        }     
    }
    for(auto& tx : block.txs())
    {
        if ((global::ca::TxType)tx.txtype() != global::ca::TxType::TX_TYPE_INVOKE_CONTRACT || (global::ca::TxType)tx.txtype() != global::ca::TxType::kTransactionTypeDeploy)
        {
            break;
        }
        auto contract_block_iterator = contractBlocks.find(tx.hash());
        if(contract_block_iterator != contractBlocks.end())
        {
            auto contractBlock = contract_block_iterator->second.second;
            std::string contractTxPreviousHash;
            auto ret = check_contract_block(contractBlock, contractTxPreviousHash);
            if(ret < 0)
            {
                DEBUGLOG("check_contract_block error, contractBlockHash:{}, contractTxPreviousHash:{}",contractBlock.hash().substr(0,6), contractTxPreviousHash);
                break;
            }
            if(ret == 0)
            {
                if(!contractTxPreviousHash.empty())
                {
                    DEBUGLOG("Still can't find contractTxPreviousHash, contractBlockHash:{}, contractTxPreviousHash:{}",contractBlock.hash().substr(0,6), contractTxPreviousHash);
                    break;
                }
                else
                {
                    std::string blockRaw;
                    DBReader dbReader;
                    if(DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(contractBlock.prevhash(), blockRaw))
                    {
                        AddPendingBlock(contractBlock);
                        return;
                    }
                    DEBUGLOG("__contractBlocks Add block height:{}, hash:{}", contract_block_iterator->second.second.height(), contract_block_iterator->second.second.hash().substr(0,6));
                    SaveBlock(contract_block_iterator->second.second, global::ca::SaveType::Broadcast, global::ca::blockMean::processByPreHash);
                }
            }
        }
    }

    processPostMembershipCancellation(block);
}

int BlockHelper::rollbackContractBlock()
{
    int ret = 0;
    std::set<std::string> addrMap; 
    std::set<std::string> rollbackBlocksHashes;
    for (auto it = rollbackBlocks.rbegin(); it != rollbackBlocks.rend(); ++it)
    {
        for (auto sit = it->second.begin(); sit != it->second.end(); ++sit)
        {
            rollbackBlocksHashes.insert(sit->hash());
            if(isContractBlocked(*sit))
            {
                auto preHashKeyList = get_contract_pre_hash(*sit);
                if(!preHashKeyList.empty())
                {
                    addrMap.insert(preHashKeyList.begin(), preHashKeyList.end());
                }

                for(auto& tx :sit->txs())
                {
                    auto addr = GetContractAddr(tx);
                    if(!addr.empty())
                    {
                        addrMap.insert(addr);
                    }
                }
            }
        }
    }

    uint64_t nodeSelfHeight = 0;
    DBReader dbReader;
    auto status = dbReader.getBlockTop(nodeSelfHeight);
    if (DBStatus::DB_SUCCESS != status)
    {
        ERRORLOG("rollbackContractBlock getBlockTop error");
        return -1;
    }

    uint64_t beginHeight = rollbackBlocks.begin()->first;
    std::vector<std::string> block_hashes;
    if(DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(beginHeight, nodeSelfHeight, block_hashes))
    {
        ERRORLOG("rollbackContractBlock getBlockHashesByBlockHeight error");
        return -2;
    }

    for(const auto& blockHash: block_hashes)
    {
        std::string blockStr;
        CBlock block;
        if(DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(blockHash, blockStr))
        {
            ERRORLOG("rollbackContractBlock getBlockByBlockHash error");
            return -3;
        }
        block.ParseFromString(blockStr);

        auto findBlock = rollbackBlocksHashes.find(block.hash());
        if(findBlock != rollbackBlocksHashes.end())
        {
            continue;
        }

        if(isContractBlocked(block))
        {
            auto preHashKeyList = get_contract_pre_hash(block);
            for(auto& tx :block.txs())
            {
                auto addr = GetContractAddr(tx);
                preHashKeyList.insert(addr);
                if(!preHashKeyList.empty())
                {
                    auto intersection = std::set<std::string>();
                    std::set_intersection(preHashKeyList.begin(), preHashKeyList.end(), addrMap.begin(), addrMap.end(), std::inserter(intersection, intersection.begin()));

                    if(!intersection.empty())
                    {
                        rollbackBlocks[block.height()].insert(block);
                    }
                }
            }
        }
    }

    return 0;
}

int BlockHelper::RollbackBlock(const std::string& blockHash, uint64_t height)
{
    DEBUGLOG("RollbackBlock Start");
    std::vector<std::string> node_ids_to_send;

    uint64_t chainHeight = 0;
    if(!BlockHelper::getChainHeight(chainHeight))
    {
        return -1;
    }
    uint64_t nodeSelfHeight = 0;
    std::vector<std::string> pledgeAddr;
    {
        DBReader dbReader;
        auto status = dbReader.getBlockTop(nodeSelfHeight);
        if (DBStatus::DB_SUCCESS != status)
        {
            return -2;
        }

        std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();

        for (const auto &node : nodelist)
        {
            int ret = VerifyBonusAddr(node.address);

            int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
            if (stakeTime > 0 && ret == 0)
            {
                pledgeAddr.push_back(node.address);
            }
        }
    }

    SyncBlock::getSyncNodeSimplified(pledgeAddr.size(), chainHeight, pledgeAddr, node_ids_to_send);

    std::vector<CBlock> retBlocks;
    int ret = SyncBlock::fetchRollbackBlock(node_ids_to_send, height, height, nodeSelfHeight, retBlocks);
    if(ret != 0)
    {
        ERRORLOG("RollbackBlock fetchRollbackBlock error");
        return -3;
    }

    std::map<uint64_t, std::set<CBlock, BlockComparator>> rollBackMap;

    for(const auto& block : retBlocks)
    {
        if(block.hash() == blockHash)
        {
            rollBackMap[height] = {block};
            break;
        }
    }

    if (!rollBackMap.empty())
    {
        DEBUGLOG("Adding block to rollback queue at height {}, blockHash: {}", height, blockHash);
        MagicSingleton<BlockHelper>::GetInstance()->rollback_block_(rollBackMap);
    }
    else
    {
        ERRORLOG("No block hash with > 51% consensus found for rollback, blockHash: {}", blockHash);
        return -4;
    }
    
    DEBUGLOG("RollbackBlock End");
    return 0;
}

int BlockHelper::RollbackBlocks()
{
    if (rollbackBlocks.empty())
    {
        return 0;
    }

    int ret = rollbackContractBlock();
    if(ret != 0)
    {
        ERRORLOG("rollbackContractBlock error, error num: {}", ret);
        return -1;
    }

    for (auto it = rollbackBlocks.rbegin(); it != rollbackBlocks.rend(); ++it)
    {
        for (auto sit = it->second.begin(); sit != it->second.end(); ++sit)
        {
            DEBUGLOG("roll back {} at height {}", sit->hash(), sit->height());
            ret = RollbackBlock(sit->hash(), sit->height());
            if (ret != 0)
            {
                ERRORLOG("rollback hash {} fail, ret: ", sit->hash(), ret);
                return -1;
            }
            
        }
    }
    return 0;
}

void BlockHelper::set_missing_prehash()
{
    missing_prehash = true;
}

void BlockHelper::resetMissingPrehash()
{
    missing_prehash = false;
}

void BlockHelper::pushMissUtxo(const std::string& utxo)
{
    std::lock_guard<std::mutex> lock(missingUtxosMutex);
    missingUtxos.push(utxo);
    if(missingUtxos.size() > MAX_MISSING_UXTO_SIZE)
    {
        std::stack<std::string>().swap(missingUtxos);
    }
}

bool BlockHelper::getMissingBlock()
{
    std::string utxo;
    {
        std::lock_guard<std::mutex> lock(missingUtxosMutex);
        if(missingUtxos.empty())
        {
            INFOLOG("utxo is empty!");
            return false;
        }
        utxo = missingUtxos.top();
    }

    auto asyncThread = std::thread(sendBlockByUtxoRequest, utxo);
	asyncThread.detach();
    return true;
}
void BlockHelper::popMissUtxo()
{
    std::scoped_lock lock(helperMutex, missingUtxosMutex);
    if(missingUtxos.empty())
    {
        return;
    }
    missingUtxos.pop();
}

void BlockHelper::Process()
{
    static int count = 0;
    static bool processing_ = false;
    if(processing_)
    {
        DEBUGLOG("BlockPoll::Process is processing_");
        return;
    }
    processing_ = true;
    std::lock_guard<std::mutex> lock(helperMutex);
    commitCostAfter = 0;
    committedPostCount = 0;
    DBReader dbReader;
    uint64_t nodeHeight = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(nodeHeight))
    {
        return;
    }

    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();

    ON_SCOPE_EXIT{
        processing_ = false;
        MagicSingleton<BlockManager>::GetInstance()->remove_expired_blocks_(std::chrono::seconds(60));
        uint64_t newTop = 0;
        DBReader reader;
        if (reader.getBlockTop(newTop) == DBStatus::DB_SUCCESS)
        {
            if (nodeHeight != newTop)
            {
                notifyNodeHeightChangeRequest();
                DEBUGLOG("notifyNodeHeightChangeRequest update ok.");
            }
        }
        fastSyncBlocks.clear();
        auto begin = _pendingBlocks.begin();
        std::vector<decltype(begin)> pendingBlockToDelete;
        for(auto iter = begin; iter != _pendingBlocks.end(); ++iter)
        {
            if (newTop >= iter->first + 20)
            {
                pendingBlockToDelete.push_back(iter);
            }
        }

        for (auto pendingIter : pendingBlockToDelete)
        {
            DEBUGLOG("_pendingBlocks.erase height:{}", pendingIter->first);
            _pendingBlocks.erase(pendingIter);
        }
        rollbackBlocks.clear();
        _syncBlocks.clear();
        broadcastBlocks.clear();
        
        for(auto iter = doubleSpendBlocks.begin(); iter != doubleSpendBlocks.end();)
        {
            if(nowTime >= iter->second.time() + 30 * 1000000ull)
            {
                DEBUGLOG("AAAC doubleSpendBlocks deleteBlockHash:{}", iter->first);
                doubleSpendBlocks.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }

        for(auto iter = _duplicateChecker.begin(); iter != _duplicateChecker.end();)
        {
            if(nowTime >= iter->second.second + 60 * 1000000)
            {
                _duplicateChecker.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }

        for(auto iter = contractBlocks.begin(); iter != contractBlocks.end();)
        {
            if(newTop >= iter->second.second.height() + 10 || nowTime >= iter->second.second.time() + 30 * 1000000ull)
            {
                contractBlocks.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }
    };


    uint64_t RollbackTimeout = 15 * 1000000;

    std::map<uint64_t, std::vector<std::set<CBlock, BlockComparator>::iterator>> deleteRollbackBlocks;
    for(auto iter = rollbackBlocks.begin(); iter != rollbackBlocks.end(); ++iter)
    {
        for(auto rollBack = iter->second.begin(); rollBack != iter->second.end(); ++rollBack)
        {
            if(nowTime <= RollbackTimeout + rollBack->time())
            {
                deleteRollbackBlocks[iter->first].push_back(rollBack);
            }
        }
    }

    for(auto& iter : deleteRollbackBlocks)
    {
        if(rollbackBlocks.find(iter.first) != rollbackBlocks.end())
        {
            for(auto& rollBack : iter.second)
            {
                DEBUGLOG("RollBackBlock No timeout blockHash:{}", rollBack->hash().substr(0,6));
                rollbackBlocks[iter.first].erase(rollBack);
                
            }
        }
        if(rollbackBlocks[iter.first].empty())
        {
            DEBUGLOG("rollbackBlocks[*{}*].empty()", iter.first);
            rollbackBlocks.erase(iter.first);
        }
    }

    int result = RollbackBlocks();
    if(result != 0)
    {
        return;
    }

    uint64_t chainHeight = 0;
    if(!getChainHeight(chainHeight))
    {
        return;
    }

    for(const auto& block : fastSyncBlocks)
    {
        global::ca::blockMean obtain_mean = global::ca::blockMean::Normal;
        if (block.height() + 1 == nodeHeight)
        {
            obtain_mean = global::ca::blockMean::processByPreHash;
        }
        DEBUGLOG("fastSyncBlocks SaveBlock Hash: {}, height: {}, PreHash:{}", block.hash().substr(0, 6), block.height(), block.prevhash().substr(0, 6));
        result = SaveBlock(block, g_syncType, obtain_mean);
        usleep(100000);
        DEBUGLOG("fast_sync save block height: {}\tblock hash: {}\tresult: {}", block.height(), block.hash(), result);

        if (result == -2)
        {
            DEBUGLOG("next run new sync, start height: {}", block.height() - 1);
            SyncBlock::set_new_sync_height(block.height() - 1);
            return;
        }

        if(result != 0)
        {
            break;
        }
    }
    for(const auto& block : utxoMissingBlocks)
    {
        DEBUGLOG("utxoMissingBlocks SaveBlock Hash: {}, height: {}, PreHash:{}", block.hash().substr(0, 6), block.height(), block.prevhash().substr(0, 6));
        result = SaveBlock(block, g_syncType, global::ca::blockMean::ByUtxo);
        if(result != 0)
        {
            if(utxoMissingBlocks.size() > MAX_MISSING_BLOCK_SIZE)
            {
                utxoMissingBlocks.clear();
            }
            break;
        }
    }
    utxoMissingBlocks.clear();

    for(const auto& block : _syncBlocks)
    {
        if(!_stopBlocking)
        {
            return;
        }

        DEBUGLOG("chain height: {}, height: {}, sync type: {}", chainHeight, block.height(), g_syncType);
        DEBUGLOG("_syncBlocks SaveBlock Hash: {}, height: {}, PreHash:{}", block.hash().substr(0, 6), block.height(), block.prevhash().substr(0, 6));
        result = SaveBlock(block, g_syncType, global::ca::blockMean::Normal);
        if(result != 0)
        {
            break;
        }
    }

    for(const auto& block : broadcastBlocks)
    {
        std::string blockRaw;
        if (DBStatus::DB_SUCCESS == dbReader.getBlockByBlockHash(block.hash(), blockRaw))
        {
            INFOLOG("block {} already saved", block.hash().substr(0,6));
            continue;
        }
        DEBUGLOG("broadcastBlocks SaveBlock Hash: {}, height: {}, PreHash:{}", block.hash().substr(0, 6), block.height(), block.prevhash().substr(0, 6));
        SaveBlock(block, global::ca::SaveType::Broadcast, global::ca::blockMean::Normal);
    }

    auto begin = hashPendingBlocks.begin();
    auto end = hashPendingBlocks.end();
    std::vector<decltype(begin)> removeUtxoBlocks;
    for(auto iter = begin; iter != end; ++iter)
    {
        if(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp() - iter->second.first > 3 * 1000000)
        {
            DEBUGLOG("hashPendingBlocks.erase timeout block height:{}, hash:{}",iter->second.second.height(), iter->second.second.hash());
            removeUtxoBlocks.push_back(iter);
            continue;
        }
        DEBUGLOG("hashPendingBlocks SaveBlock Hash: {}, height: {}, PreHash:{}", iter->second.second.hash().substr(0, 6), iter->second.second.height(), iter->second.second.prevhash().substr(0, 6));
        int result = SaveBlock(iter->second.second, global::ca::SaveType::Broadcast, global::ca::blockMean::ByUtxo);
        if(result == 0)
        {
            DEBUGLOG("hashPendingBlocks Add <success> block height:{}, hash:{}",iter->second.second.height(), iter->second.second.hash());
            removeUtxoBlocks.push_back(iter);
        }
        else
        {
            DEBUGLOG("hashPendingBlocks Add <fail> block height:{}, hash:{}", iter->second.second.height(), iter->second.second.hash());
        }

    }

    for(auto utxoBlockIterator: removeUtxoBlocks)
    {
        hashPendingBlocks.erase(utxoBlockIterator);
    }
    
    return;
}

void BlockHelper::SeekBlockThreadObject()
{
    if(_missingBlocks.empty())
    {
        DEBUGLOG("_missingBlocks.empty() == true");
        return;
    }

    DEBUGLOG("SeekBlockThreadObject start");
    std::map<std::string, bool> missingHashs;
    auto begin = _missingBlocks.begin();
    auto end = _missingBlocks.end();
    std::vector<decltype(begin)> shouldDeleteMissingBlocks;

    {
        DBReader dbReader;
        uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        std::unique_lock<std::mutex> locker(seekMutex);
        for(auto iter = begin; iter != end; ++iter)
        {
            if(nowTime - iter->_time > 30 * 1000000 || *iter->_triggerCount > 3)
            {
                DEBUGLOG("missing_Hash:{}, timeout:({}),*iter->_triggerCount:({}),*iter->txOrBlockVariant:{}", iter->_hash, nowTime - iter->_time > 30 * 1000000, *iter->_triggerCount > 3, *iter->txOrBlockVariant);
                shouldDeleteMissingBlocks.push_back(iter);
                continue;
            }
            else if(nowTime - iter->_time > 15 * 1000000)
            {
                std::string strBlock;
                if(*iter->txOrBlockVariant)
                {
                    if (DBStatus::DB_SUCCESS == dbReader.getBlockHashByTransactionHash(iter->_hash, strBlock))
                    {
                        shouldDeleteMissingBlocks.push_back(iter);
                        continue;
                    }
                }
                else if(DBStatus::DB_SUCCESS == dbReader.getBlockByBlockHash(iter->_hash, strBlock))
                {
                    shouldDeleteMissingBlocks.push_back(iter);
                    continue;
                }

                if(missingHashs.find(iter->_hash) == missingHashs.end())
                {
                    DEBUGLOG("missing_Hash:{},*iter->_triggerCount:{},*iter->txOrBlockVariant:{}", iter->_hash, *iter->_triggerCount, *iter->txOrBlockVariant);
                    missingHashs[iter->_hash] = *(iter->txOrBlockVariant);
                }
                else
                {
                    //Filtering duplicate hash
                    shouldDeleteMissingBlocks.push_back(iter);
                }
                *iter->_triggerCount = *iter->_triggerCount + 1;
            }
            else
            {
                break;
            }
        }

        for(auto iter: shouldDeleteMissingBlocks)
        {
            DEBUGLOG("_missingBlocks.erase_Hash:{}", iter->_hash);
            _missingBlocks.erase(iter);
        }
    }

    if(!missingHashs.empty())
    {
        sendBlockByHashRequest(missingHashs);
    }
            
}

void BlockHelper::add_seek_block(std::vector<std::pair<CBlock,std::string>>& seekBlocks)
{
    std::lock_guard<std::mutex> lock(helperMutex);
    for(const auto &iter : seekBlocks)
    {
        auto& block = iter.first;
        auto found = hashPendingBlocks.find(block.hash());
        if(found == hashPendingBlocks.end())
        {
            MagicSingleton<Benchmark>::GetInstance()->addBlockPoolSaveMapStart(block.hash());
            hashPendingBlocks[block.hash()] = {MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp(), block};
        }

        DEBUGLOG("add_seek_block missing_block_hash:{}, tx_or_block_hash:{}", block.hash(), iter.second); 
    }
}

void BlockHelper::checkDoubleBlooming(const std::pair<DOUBLE_SPEND_TYPE, CBlock>& double_spend_data, const CBlock &block)
{
    if(double_spend_data.first == DOUBLE_SPEND_TYPE::isDoubleSpend)
    {
        auto found = _duplicateChecker.find(block.hash());
        DEBUGLOG("DOUBLE_SPEND_TYPE:{}, status:{}, blockHash:{}", double_spend_data.first, found->second.first, block.hash().substr(0,6));
        if(found != _duplicateChecker.end() && found->second.first)
        {
            transactionStatusMessage(double_spend_data.second, block);
            return;
        }
    }  
    else if(double_spend_data.first == DOUBLE_SPEND_TYPE::previousDoubleSpend)
    {
        auto found = _duplicateChecker.find(double_spend_data.second.hash());
        DEBUGLOG("DOUBLE_SPEND_TYPE:{}, status:{}, blockHash:{}", double_spend_data.first, found->second.first, block.hash().substr(0,6));
        if(found != _duplicateChecker.end() && found->second.first)
        {
            transactionStatusMessage(block, double_spend_data.second);
            return;
        }
    }
    return;
}

void BlockHelper::transactionStatusMessage(const CBlock &oldBlock, const CBlock &newBlock)
{
    DEBUGLOG("AAAC transactionStatusMessage oldBlock:{}, newBlock:{}", oldBlock.hash().substr(0,6), newBlock.hash().substr(0,6));
    BlockStatus blockStatus;
    for(const auto& tx1 : oldBlock.txs())
    {
        if(GetTransactionType(tx1) != kTransactionTypeTx)
        {
            continue;
        }

        for(const auto& tx2 : newBlock.txs())
        {
            if(GetTransactionType(tx2) != kTransactionTypeTx)
            {
                continue;
            }

            if(Checker::CheckConflict(tx1, tx2) == true)
            {
                DEBUGLOG("AAAC transactionStatusMessage oldBlocktx1:{}, newBlocktx2:{}", tx1.hash().substr(0,10), tx2.hash().substr(0,10));
                auto txStatus = blockStatus.add_txstatus();
                txStatus->set_txhash(tx2.hash());
                txStatus->set_status(global::ca::DoubleSpend::DoubleBlock);
            }
        }
    }

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    blockStatus.set_blockhash(newBlock.hash());
    blockStatus.set_status(-99);
    blockStatus.set_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    std::string destNode = GenerateAddr(newBlock.sign(0).pub());
    if(destNode != defaultAddr)
    {
        DEBUGLOG("AAAC protocolBlockStatus, destNode:{}, ret:{}, blockHash:{}", destNode, -99, newBlock.hash().substr(0,6));
        protocolBlockStatus(blockStatus, destNode);
    }
        
}

CONTRACT_PRE_HASH_STATUS BlockHelper::contractPreHashStatus(const std::string& contractAddr, const std::string& memContractPreHashResponse, const uint64_t blockTime, std::string& dbBlockHash)
{
    if(memContractPreHashResponse.empty() || contractAddr.empty())
    {
        return CONTRACT_PRE_HASH_STATUS::Err;
    }

    DBReader dbReader;
    std::string dbContractPreHash;
    if (DBStatus::DB_SUCCESS != dbReader.getLatestUtxoByContractAddr(contractAddr, dbContractPreHash))
    {
        return CONTRACT_PRE_HASH_STATUS::Err;
    }
    if(dbContractPreHash == memContractPreHashResponse)
    {
        return CONTRACT_PRE_HASH_STATUS::Normal;
    }

    std::string prevBlockHash;
    if(dbReader.getBlockHashByTransactionHash(dbContractPreHash, prevBlockHash) != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("getBlockHashByTransactionHash failed!");
        return CONTRACT_PRE_HASH_STATUS::Err;
    }

    std::string blockRaw;
    if(dbReader.getBlockByBlockHash(prevBlockHash, blockRaw) != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("getBlockByBlockHash failed!");
        return CONTRACT_PRE_HASH_STATUS::Err;
    }

    CBlock block;
    if(!block.ParseFromString(blockRaw))
    {
        ERRORLOG("parse failed!");
        return CONTRACT_PRE_HASH_STATUS::Err;
    }

    dbBlockHash = block.hash();

    try
    {
        nlohmann::json prevData = nlohmann::json::parse(block.data());

        for (const auto&[key, value] : prevData.items())
        {
            if(key == dbContractPreHash)
            {
                for(auto &it : value[Evmone::contractPreHashKey].items())
                {
                    if(it.key() == contractAddr && memContractPreHashResponse == it.value().get<std::string>())
                    {
                        return CONTRACT_PRE_HASH_STATUS::Normal;
                    }
                    else if(it.key() == contractAddr && memContractPreHashResponse != it.value().get<std::string>())
                    {
                        if(blockTime > block.time())
                        {
                            return CONTRACT_PRE_HASH_STATUS::MemoryBlockException;
                        }
                        else
                        {
                            return CONTRACT_PRE_HASH_STATUS::DatabaseBlockException;
                        }
                    }
                }
                return CONTRACT_PRE_HASH_STATUS::Waiting;
            }
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return CONTRACT_PRE_HASH_STATUS::Err;
    }
    return CONTRACT_PRE_HASH_STATUS::Err;
}
int BlockHelper::contractBlockCacheChecker(const CBlock& block, const std::string& contractAddr, const std::string& contractTxPreviousHash)
{
    auto contract_block_iterator = contractBlocks.find(contractTxPreviousHash);
    if(contract_block_iterator != contractBlocks.end() && contractAddr ==  contract_block_iterator->second.first)
    {
        if(contract_block_iterator->second.second.time() > block.time())
        {
            add_contract_block(block, contractAddr, contractTxPreviousHash);
            DEBUGLOG("delete mem oldContractBlock ,contractTxPreviousHash:{}, oldblockHash:{}, newblockHash:{}, contractAddr:{}", contractTxPreviousHash, contract_block_iterator->second.second.hash().substr(0,6), block.hash().substr(0,6), contractAddr);
            return 0;
        }
        else
        {
            DEBUGLOG("delete mem newContractBlock ,contractTxPreviousHash:{}, oldblockHash:{}, newblockHash:{}, contractAddr:{}", contractTxPreviousHash, contract_block_iterator->second.second.hash().substr(0,6), block.hash().substr(0,6), contractAddr);
            return -1;
        }
    }
    else
    {
        add_contract_block(block, contractAddr, contractTxPreviousHash);
    }
    return 0;
}
int BlockHelper::check_contract_block(const CBlock& block, std::string& contractTxPreviousHash)
{
    DBReader dbReader;
    uint64_t nodeSelfHeight;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(nodeSelfHeight))
    {
        DEBUGLOG("Get nodeSelfHeight error");
        return -1;
    }

    try
    {
        std::map<std::string, std::vector<std::pair<std::string, std::string>>> contractTxPreCacheMap;
        std::set<std::string> seenPrehashes_;
        nlohmann::json dataJson = nlohmann::json::parse(block.data());
        for (const auto&[key, value] : dataJson.items())
        {
            for(auto &it : value[Evmone::contractPreHashKey].items())
            {
                if (seenPrehashes_.find(it.value()) != seenPrehashes_.end())
                {
                    continue;
                }
                
                contractTxPreCacheMap[key].push_back({it.key(), it.value()});
                seenPrehashes_.insert(it.value().get<std::string>());
            }
        }

        for(auto& iter : contractTxPreCacheMap)
        {
            for(auto& preHashPair : iter.second)
            {
                if(contractTxPreCacheMap.find(preHashPair.second) != contractTxPreCacheMap.end())
                {
                    continue;
                }

                std::string dbBlockHash;
                auto preHashVerificationStatus = contractPreHashStatus(preHashPair.first, preHashPair.second, block.time(), dbBlockHash);
                if(preHashVerificationStatus == CONTRACT_PRE_HASH_STATUS::Normal)
                {
                    DEBUGLOG("contractBlockCacheChecker blockHash:{}, contractPrehash:{}", block.hash().substr(0,6), preHashPair.second.substr(0,10));
                    if(contractBlockCacheChecker(block, preHashPair.first, preHashPair.second) != 0)
                    {
                        return -2;
                    }
                    continue;
                }
                else if(preHashVerificationStatus == CONTRACT_PRE_HASH_STATUS::MemoryBlockException)
                {
                    DEBUGLOG("contractBlockConflicts dbBlockHash :{}, blockHash:{}", dbBlockHash.substr(0,6), block.hash().substr(0,6));
                    return -3;
                }
                else if(preHashVerificationStatus == CONTRACT_PRE_HASH_STATUS::DatabaseBlockException)
                {
                    DEBUGLOG("contractBlock rollback RollBlockHash:{},blockHash:{}", dbBlockHash.substr(0,6), block.hash().substr(0,6));
                    if(dbBlockHash == block.hash())
                    {
                        continue;
                    }

                    unsigned int height = 0;
                    if (DBStatus::DB_SUCCESS != dbReader.getBlockHeightByBlockHash(dbBlockHash, height))
                    {
                        return -4;
                    }
                    auto ret = RollbackBlock(dbBlockHash, height);
                    if (ret != 0)
                    {
                        ERRORLOG("contractBlock rollback hash {} fail, ret:{}", dbBlockHash, ret);
                        return -5;
                    }
                    continue;
                }
                else if(preHashVerificationStatus == CONTRACT_PRE_HASH_STATUS::Waiting)
                {
                    if(contractBlockCacheChecker(block, preHashPair.first, preHashPair.second) != 0)
                    {
                        return -6;
                    }
                    contractTxPreviousHash = preHashPair.second;
                    break;
                }
            }

            if(!contractTxPreviousHash.empty())
            {
                if(block.height() <= nodeSelfHeight + 3)
                {
                    DEBUGLOG("_missingContractBlocks.insert height:{}, hash:{}, contractTxPreviousHash:{}, ", block.height(), block.hash().substr(0,6), contractTxPreviousHash.substr(0,6));
                    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
                    std::unique_lock<std::mutex> locker(seekMutex);
                    _missingBlocks.insert({contractTxPreviousHash, nowTime, 1});
                }
                return 0;
            }
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return -6;
    }
    return 0;
}
void BlockHelper::AddPendingBlock(const CBlock& block)
{
    DBReader dbReader;
    uint64_t nodeSelfHeight;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(nodeSelfHeight))
    {
        DEBUGLOG("Get nodeSelfHeight error");
        return;
    }

    uint64_t blockHeight = block.height();
    INFOLOG("_pendingBlocks height:{}, hash:{}", blockHeight, block.hash().substr(0,6));
    if(_pendingBlocks.size() < 1000)
    {
        _pendingBlocks[blockHeight].insert({block.prevhash(), block}); 
    }
    
    if(blockHeight > nodeSelfHeight + 3)
    {
        return;
    }

    DEBUGLOG("_missingBlocks.insert height:{}, hash:{}, prevhash:{}, ", blockHeight, block.hash().substr(0,6), block.prevhash().substr(0,6));
    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    std::unique_lock<std::mutex> locker(seekMutex);
    _missingBlocks.insert({block.prevhash(), nowTime, 0});
}

void BlockHelper::add_contract_block(const CBlock& block, const std::string& contractAddr, const std::string& contractTxPreviousHash)
{
    INFOLOG("contractBlocks height:{}, hash:{}, contractTxPreviousHash:{}, contractAddr:{}", block.height(), block.hash().substr(0,6), contractTxPreviousHash, contractAddr);
    if(contractBlocks.size() < 1000)
    {
        contractBlocks[contractTxPreviousHash] = {contractAddr, block};
    }
    return;
}

void BlockHelper::AddBroadcastBlock(const CBlock& block, bool status)
{

    std::lock_guard<std::mutex> lockLow1(helperMutexLow1);
    std::lock_guard<std::mutex> lock(helperMutex);
    
    if(_duplicateChecker.find(block.hash()) != _duplicateChecker.end())
    {
        DEBUGLOG("+++Duplicate Block hash:{}, status:{}", block.hash().substr(0,6), status);
        if(status) _duplicateChecker[block.hash()] = {status, block.time()};
        return;
    }

    if(doubleSpendBlocks.find(block.hash()) != doubleSpendBlocks.end())
    {
        DEBUGLOG("doubleSpendBlocks blockHash:{}", block.hash().substr(0, 6));
        return;
    }

    DEBUGLOG("Duplicate Block hash:{}, status:{}", block.hash().substr(0,6), status);
    _duplicateChecker[block.hash()] = {status, block.time()};

    for (auto it = broadcastBlocks.begin(); it != broadcastBlocks.end(); ++it) 
    {
        auto &curr_block = *it;
        bool ret = Checker::CheckConflict(curr_block, block);
        if(ret)
        {
            if((curr_block.height() == block.height() && curr_block.time() <= block.time()) || (curr_block.height() < block.height()))
            {
                if(status)
                {
                    MagicSingleton<TaskPool>::GetInstance()->commitCaTask(std::bind(&BlockHelper::transactionStatusMessage, this, curr_block, block));
                } 
                INFOLOG("block {} has conflict, discard!", block.hash().substr(0,6));
                return;
            }
            else
            {
                auto result = _duplicateChecker.find(curr_block.hash());
                if(result != _duplicateChecker.end() && result->second.first)
                {
                    MagicSingleton<TaskPool>::GetInstance()->commitCaTask(std::bind(&BlockHelper::transactionStatusMessage, this, block, curr_block));
                }
                INFOLOG("blockHash:{}, deleteBlockHash:{}", block.hash().substr(0,6), curr_block.hash().substr(0,6));
                it = broadcastBlocks.erase(it);
                break;
            }
        }
    }
    DEBUGLOG("broadcastBlocks broadcastBlocks.size:{}", broadcastBlocks.size());
    
    DBReader dbReader;
    uint64_t nodeSelfHeight;
    auto res = dbReader.getBlockTop(nodeSelfHeight);
    if (DBStatus::DB_SUCCESS != res)
    {
        DEBUGLOG("Get nodeSelfHeight error");
        return;
    }

    std::string blockRaw;
    auto prevHashStatus_ = dbReader.getBlockByBlockHash(block.prevhash(), blockRaw);
    bool contractPrevHashStatus = true;
    std::string contractPrevBlockHash_;
    
    bool isContractualBlock = false;
    if(isContractBlocked(block))
    {
        std::string contractTxPreviousHash;
        auto ret = check_contract_block(block, contractTxPreviousHash);
        if(ret < 0)
        {
            DEBUGLOG("check_contract_block error, contractBlockHash:{}, contractTxPreviousHash:{}",block.hash().substr(0,6), contractTxPreviousHash);
            return;
        }
        if(ret == 0 && !contractTxPreviousHash.empty())
        {
            contractPrevHashStatus = false;
        }
        isContractualBlock = true;
    }

    if(!isContractualBlock)
    {
        if(DBStatus::DB_SUCCESS == prevHashStatus_)
        {
            INFOLOG("broadcastBlocks height:{}, hash:{}, status:{}", block.height(), block.hash().substr(0,6), status);
            if(block.height() <= nodeSelfHeight + 1000)
            {
                broadcastBlocks.insert(block);
            }
        }
        else
        {
            AddPendingBlock(block);
        }
    }

    if(isContractualBlock)
    {
        if(contractPrevHashStatus)
        {
            if(DBStatus::DB_SUCCESS == prevHashStatus_)
            {
                INFOLOG("broadcastBlocks height:{}, hash:{}, status:{}", block.height(), block.hash().substr(0,6), status);
                if(block.height() <= nodeSelfHeight + 1000)
                {
                    broadcastBlocks.insert(block);
                }
            }
            else
            {
                AddPendingBlock(block);
            }
        }
    }
}

void BlockHelper::AddSyncBlock(const std::map<uint64_t, std::set<CBlock, BlockComparator>> &syncBlockData, global::ca::SaveType type)
{
    DEBUGLOG("AddSyncBlock syncBlockData.size(): {}", syncBlockData.size());
    std::lock_guard<std::mutex> lock(helperMutex);
    for(const auto&[key,value]:syncBlockData)
    {
        for(const auto& sit: value)
        {
            MagicSingleton<Benchmark>::GetInstance()->addBlockPoolSaveMapStart(sit.hash());
            _syncBlocks.insert(std::move(sit));
        }
    }
    g_syncType = type;
}

void BlockHelper::AddFastSyncBlock(const std::map<uint64_t, std::set<CBlock, BlockComparator>> &syncBlockData, global::ca::SaveType type)
{
    std::lock_guard<std::mutex> lock(helperMutex);
    for (auto it = syncBlockData.begin(); it != syncBlockData.end(); ++it)
    {
        for (auto sit = it->second.begin(); sit != it->second.end(); ++sit)
        {
            MagicSingleton<Benchmark>::GetInstance()->addBlockPoolSaveMapStart(sit->hash());
            fastSyncBlocks.insert(*sit);
        }
    }
    g_syncType = type;
}

void BlockHelper::rollback_block_(const std::map<uint64_t, std::set<CBlock, BlockComparator>> &rollbackBlockInfo)
{
    std::lock_guard<std::mutex> lock(helperMutex);
    rollbackBlocks = rollbackBlockInfo;
}

void BlockHelper::AddMissingBlock(const CBlock& block)
{
    std::lock_guard<std::mutex> lock(helperMutex);
    MagicSingleton<Benchmark>::GetInstance()->addBlockPoolSaveMapStart(block.hash());
    utxoMissingBlocks.push_back(block);
}

bool BlockHelper::getChainHeight(uint64_t& chainHeight)
{
    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return false;
    }

    std::vector<Node> nodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    if (nodes.empty())
    {
        DEBUGLOG("nodes.empty() == true, chainHeight:{}", chainHeight);
        chainHeight = top;
        return true;
    }

    std::vector<Node> qualifyingNode;

    if(top < global::ca::MIN_UNSTAKE_HEIGHT)
    {
        qualifyingNode = nodes;
    }
    else
    {
        for (const auto &node : nodes)
        {
            int ret = VerifyBonusAddr(node.address);

            int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
            if (stakeTime > 0 && ret == 0)
            {
                qualifyingNode.push_back(node);
            }
        }
        if(qualifyingNode.empty())
        {
            DEBUGLOG("qualifyingNode.empty() == true, chainHeight:{}", chainHeight);
            chainHeight = top;
            return true;
        }
    }


    std::vector<uint64_t> nodeHeights;
    for (auto &node : qualifyingNode)
    {
        nodeHeights.push_back(node.height);
    }
    std::sort(nodeHeights.begin(), nodeHeights.end(), std::greater<uint64_t>());
    //const static int malicious_node_tolerated_amount = 25;
    double sampleRate = 0.75;

    int verifyNum = nodeHeights.size() * sampleRate;
    if (verifyNum >= nodeHeights.size())
    {
        ERRORLOG("get chain height error index:{}:{}", verifyNum, nodeHeights.size());
        return false;
    }
    chainHeight = nodeHeights.at(verifyNum);

    return true;
}


void BlockHelper::RollbackTest()
{
    std::cout << "1.Rollback block from Height" << std::endl;
    std::cout << "2.Rollback block from Hash" << std::endl;
    std::cout << "0.Quit" << std::endl;

    int iSwitch = 0;
    std::cin >> iSwitch;
    switch (iSwitch)
    {
        case 0:
        {
            break;
        }
        case 1:
        {
            unsigned int height = 0;
            std::cout << "Rollback block height: ";
            std::cin >> height;
            std::lock_guard<std::mutex> lock(helperMutex);
            auto ret = ca_algorithm::RollBackToHeight(height);
            if (0 != ret)
            {
                std::cout << std::endl
                          << "ca_algorithm::RollBackToHeight:" << ret << std::endl;
                break;
            }
            MagicSingleton<PeerNode>::GetInstance()->SetSelfHeight();
            break;
        }
        case 2:
        {
            std::map<uint64_t, std::set<CBlock, BlockComparator>> rollBackMap;
            std::string hash;
            std::cout << "Enter rollback block hash, Enter 0 exit" << std::endl;
            std::cin >> hash;
            while(hash != "0")
            {
                CBlock block;
                std::string blockStr;
                DBReader dbReader;
                if(DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(hash, blockStr))
                {
                    std::cout << "rollbackContractBlock getBlockByBlockHash error" << std::endl;
                    return;
                }
                block.ParseFromString(blockStr);
                rollBackMap[block.height()].insert(block);
                hash.clear();
                std::cout << "Enter rollback block hash, Enter 0 exit" << std::endl;
                std::cin >> hash;
            }
            if(!rollBackMap.empty())
            {
                MagicSingleton<BlockHelper>::GetInstance()->rollback_block_(rollBackMap);
            }
            return;
        }
        default:
        {
            std::cout << "Input error !" << std::endl;
            break;
        }
    }
}

