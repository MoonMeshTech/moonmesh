#include "ca/block_stroage.h"

#include "ca/test.h"
#include "ca/algorithm.h"
#include "ca/sync_block.h"
#include "ca/transaction.h"
#include "ca/block_helper.h"

#include "common/global.h"
#include "common/task_pool.h"
#include "common/global_data.h"

#include "proto/block.pb.h"
#include "net/peer_node.h"
#include "utils/bench_mark.h"
#include "utils/contract_utils.h"

void BlockStorage::_StartTimer()
{
	_blockTimer.AsyncLoop(100, [this](){
		_BlockCheck();
        checkExpiredDelete();
	});
}


int BlockStorage::AddBlock(const BlockMsg &msg)
{
	std::unique_lock<std::shared_mutex> lck(_blockMutex);

    CBlock block;
    block.ParseFromString(msg.block());

	std::vector<BlockMsg> msgVec;
	msgVec.push_back(msg); 
    _blockCnt.insert(std::pair<std::string,std::vector<BlockMsg>>(block.hash(),msgVec));
	DEBUGLOG("add TransactionCache");
	lck.unlock();

    return 0;
}

int BlockStorage::UpdateBlock(const BlockMsg &msg)
{
    std::unique_lock<std::shared_mutex> lck(_blockMutex);

    CBlock block;
    block.ParseFromString(msg.block());
    INFOLOG("recv block sign addr = {}, blockhash:{}",GenerateAddr(block.sign(1).pub()), block.hash());

    if(block.sign_size() != 2)
    {
		ERRORLOG("sign  size != 2");
        return -1;
    }
		
    auto it = _blockCnt.find(block.hash());
    if (it != _blockCnt.end())
    {
        _blockCnt[block.hash()].push_back(msg);
    }
    
	lck.unlock();

	return 0;
}


void BlockStorage::_BlockCheck()
{
    std::unique_lock<std::shared_mutex> lck(_blockMutex);

    int64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    const int64_t kTenSecond = (int64_t)1000000 * 10;

	std::vector<std::string> hashKey;
	for(auto &item : _blockCnt)
	{
        CBlock temBlock;
        temBlock.ParseFromString(item.second.at(0).block());
        DEBUGLOG("temBlock hash : {}" , temBlock.hash());
        uint64_t lagTime = abs(nowTime - (int64_t)temBlock.time());
        uint32_t msgSize = item.second.size();
        
        if(msgSize >= global::ca::kConsensus && lagTime <= kTenSecond)
        {
            DEBUGLOG("Block hash : {} Recv block sign node size : {}" ,temBlock.hash(),msgSize);
            BlockMsg outMsg;
            if(composeEndBlockMessage(item.second,outMsg,true) == 0){
                CBlock block;
                block.ParseFromString(outMsg.block());
                //After the verification is passed, the broadcast block is directly built
                if(blockStatusMap.find(block.hash()) == blockStatusMap.end())
                {
                    blockStatusMap[block.hash()] = {block.hash(), block};
                }
                auto NowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
                MagicSingleton<Benchmark>::GetInstance()->setByBlockHash(block.hash(), &NowTime, 2);
                MagicSingleton<BlockMonitor>::GetInstance()->sendBroadcastAddBlockRequest(outMsg.block(),block.height());
                DEBUGLOG("BuildBlockBroadcastMsg successful..., block hash : {}",block.hash());
            }else{
                ERRORLOG("Compose blockMsg failed!");
            }
            hashKey.push_back(temBlock.hash());
        }else if(lagTime > kTenSecond){
            hashKey.push_back(temBlock.hash());
            ERRORLOG("lagTime: {}, nowTime: {}, blockTime: {}", lagTime, nowTime, temBlock.time());
            ERRORLOG("Block Flow Timeout! block hash : {}",temBlock.hash());
        }
    }
	
    if(!hashKey.empty())
	{
		for (auto &hash : hashKey)
		{
			_Remove(hash);
		}
	}
	hashKey.clear();    
}


int BlockStorage::composeEndBlockMessage(const std::vector<BlockMsg> &msgVec, BlockMsg & outMsg , bool isVrf)
{
    std::vector<BlockMsg> vrfMessageVector;
    if(isVrf)
    {
        CBlock temBlock;
        temBlock.ParseFromString(msgVec.at(0).block());
        
        std::vector<BlockMsg> secondMsg = msgVec;
        secondMsg.erase(secondMsg.begin());

        vrfMessageVector = getRandomElements(secondMsg,(global::ca::kConsensus - 1));
        if(vrfMessageVector.size() != (global::ca::kConsensus - 1))
        {
            std::cout << "size" << vrfMessageVector.size() << std::endl;
            ERRORLOG("target lazy weight, size = {}",vrfMessageVector.size());
            return -3;
        }
    }    

    CBlock endBlock;
    endBlock.ParseFromString(msgVec[0].block()); 
	for(auto &msg : vrfMessageVector)
	{   
        CBlock block;
        block.ParseFromString(msg.block());

        if(block.sign_size() != 2)
        {
            continue;
        }
        else
        {
            CSign * sign  = endBlock.add_sign();
            sign->set_pub(block.sign(1).pub());
            sign->set_sign(block.sign(1).sign());
            INFOLOG("rand block sign = {}",GenerateAddr(block.sign(1).pub()));
        }
    }
    std::string addr = GenerateAddr(endBlock.sign(0).pub());
    outMsg.set_block(endBlock.SerializeAsString());
    return 0;       
}
void BlockStorage::_Remove(const std::string &hash)
{

    for(auto iter = _blockCnt.begin(); iter != _blockCnt.end();)
	{
		if (iter->first == hash)
		{
			iter = _blockCnt.erase(iter);
			DEBUGLOG("BlockStorage::Remove  _blockCnt hash:{}", hash);
		}
		else
		{
			iter++;
		}
	}
}

std::shared_future<RetType> BlockStorage::GetPrehash(const uint64_t height)
{
    std::shared_lock<std::shared_mutex> lck(prehashMutex);
    auto result = preHashMap.find(height);
    if(result != preHashMap.end())
    {
       return result->second;
    }
    return {};
}

bool BlockStorage::isSeekTask(uint64_t seekHeight)
{
    std::shared_lock<std::shared_mutex> lck(prehashMutex);
    if(preHashMap.find(seekHeight) != preHashMap.end())
    {
        DEBUGLOG("seek_prehash_task repeat");
        return true;
    }
    return false;
}


void BlockStorage::commitLookupTask(uint64_t seekHeight)
{
    if(isSeekTask(seekHeight))
    {
        return;
    }
    std::unique_lock<std::shared_mutex> lck(prehashMutex);
    if(preHashMap.size() > 100)
    {
        auto endHeight = preHashMap.end()->first;
        std::map<uint64_t, std::shared_future<RetType>> hashPreparer(preHashMap.find(endHeight - 10), preHashMap.end());
        preHashMap.clear();
        preHashMap.swap(hashPreparer);
    }
    DEBUGLOG("commitLookupTask, height:{}", seekHeight);
    auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(&BlockStorage::seekPreHashThread, this, seekHeight));
    try
    {
        preHashMap[seekHeight] = task->get_future();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    MagicSingleton<TaskPool>::GetInstance()->CommitSyncBlockJob([task](){(*task)();});
    return;
}

void BlockStorage::ForceCommitSeekJob(uint64_t seekHeight)
{
    std::unique_lock<std::shared_mutex> lck(prehashMutex);
    DEBUGLOG("ForceCommitSeekJob, height:{}", seekHeight);
    auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(&BlockStorage::seekPreHashThread, this, seekHeight));
    try
    {
        preHashMap[seekHeight] = task->get_future();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    MagicSingleton<TaskPool>::GetInstance()->CommitSyncBlockJob([task](){(*task)();});
    return;
}

void BlockStorage::clearPreHashMap()
{
    std::unique_lock<std::shared_mutex> lck(prehashMutex);
    preHashMap.clear();
}

int get_prehash_find_node(uint32_t num, uint64_t seekHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &node_ids_to_send)
{
    DEBUGLOG("find node base on Prehash height {}", seekHeight);
    auto discardComparisonFunc = std::less<>();
    auto compareReserve = std::greater_equal<>();
    
    int ret = 0;
    if ((ret = SyncBlock::get_sync_node_basic(num, seekHeight, discardComparisonFunc, compareReserve, pledgeAddr, node_ids_to_send)) != 0)
    {
        ERRORLOG("get seek node fail, ret:{}", ret);
        return -1;
    }
    return 0;
}

RetType BlockStorage::seekPreHashThread(uint64_t seekHeight)
{
    DEBUGLOG("seekPreHashThread Start");
    uint64_t nodeSelfHeight = 0;
    std::vector<std::string> pledgeAddr; // stake and delegatinged addr
    {
        DBReader dbReader;
        auto status = dbReader.getBlockTop(nodeSelfHeight);
        if (DBStatus::DB_SUCCESS != status)
        {
            DEBUGLOG("getBlockTop fail!!!");
            return {"",0};
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
    std::vector<std::string> node_ids_to_send;
    if (get_prehash_find_node(global::ca::MIN_SYNC_QUAL_NODES, seekHeight, pledgeAddr, node_ids_to_send) != 0)
    {
        ERRORLOG("get sync node fail");
        return {"",0};
    }
    if(seekHeight == 0 || seekHeight > nodeSelfHeight)
    {
        DEBUGLOG("seekHeight:{}, nodeSelfHeight:{}", seekHeight, nodeSelfHeight);
    }
    return seekPreHashByNode(node_ids_to_send, seekHeight, nodeSelfHeight);
}

RetType BlockStorage::seekPreHashByNode(
		const std::vector<std::string> &node_ids_to_send, uint64_t seekHeight, const uint64_t &nodeSelfHeight)
{
    std::string msgId;
    uint64_t successCounter = 0;
    if (!dataMgrPtr.CreateWait(10, node_ids_to_send.size() * 0.8, msgId))
    {
        ERRORLOG("CreateWait fail!!!");
        return {"", 0};
    }
    for (auto &nodeId : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, nodeId))
        {
            return {"", 0};
        }
        DEBUGLOG("new seek get block hash from {}", nodeId);
        sendSeekGetPreHashRequest(nodeId, msgId, seekHeight);
    }
    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        if(retDatas.size() < node_ids_to_send.size() * 0.5)
        {
            ERRORLOG("wait seek block hash time out send:{} recv:{}", node_ids_to_send.size(), retDatas.size());
            return {"", 0};
        }
    }

    std::map<std::string, bool> nodeAddrs;
    MagicSingleton<PeerNode>::GetInstance()->GetNodelist(nodeAddrs);
    
    SeekPreHashByHightAck ack;
    std::map<uint64_t, std::map<std::string, uint64_t>> pre_hashes_to_seek;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        successCounter++;
        uint64_t seekHeight = ack.seek_height();
        for(auto& prehash : ack.prehashes())
        {
            if(pre_hashes_to_seek[seekHeight].find(prehash) == pre_hashes_to_seek[seekHeight].end())
            {
                pre_hashes_to_seek[seekHeight][prehash] = 1;
            }
            else
            {
                pre_hashes_to_seek[seekHeight][prehash]++;
            }
        } 
    }

    std::set<std::string> verifyHashes;
    size_t verifyNum = successCounter / 5 * 3;

    for (auto &iter : pre_hashes_to_seek)
    {
        uint16_t maxPercentage = 0;
        std::string MAX_PERCENTAGE_PREHASH;
        for(auto &prehash : iter.second)
        {
            if (prehash.second >= verifyNum)
            {
                uint16_t percentage = prehash.second / (double)successCounter * 100;
                if(maxPercentage < percentage)
                {
                    maxPercentage = percentage;
                    MAX_PERCENTAGE_PREHASH = prehash.first;
                }
            }
        }
        if(maxPercentage >= 70)
        {
            DEBUGLOG("seekPreHashByNode <success> !!! ,seekHeight:{}, maxPercentage:{} > 70% , MAX_PERCENTAGE_PREHASH:{}", iter.first, maxPercentage, MAX_PERCENTAGE_PREHASH);
            return {MAX_PERCENTAGE_PREHASH, maxPercentage};
        }
        else
        {
            DEBUGLOG("seekPreHashByNode <fail> !!! ,seekHeight:{}, maxPercentage:{} < 70% , MAX_PERCENTAGE_PREHASH:{}", iter.first, maxPercentage, MAX_PERCENTAGE_PREHASH);
        }
    }
    return {"", 0};
}

int BlockStorage::CheckData(const BlockStatus& blockStatus)
{
    if(blockStatus.status() >= 0)
    {
        DEBUGLOG("AAAC block status invalid");
        return -1;
    }
    auto& hash = blockStatus.blockhash();
    auto destNode = blockStatus.id();
    auto found = blockStatusMap.find(hash);
    if(found == blockStatusMap.end())
    {
        return -2;
    }
    else if(blockStatusMap[hash].broadcastType == BroadcastType::verifyBroadcast)
    {
        if(found->second.verifyNodes.empty() || 
        found->second.verifyNodes.find(destNode) == found->second.verifyNodes.end())
        {
            return -3;
        }
        found->second.verifyNodes.erase(destNode);
        if(found->second.blockStatusList.size() > found->second.nodeVerificationCount)
        {
            ERRORLOG("found->second.blockStatusList.size{} found->second.nodeVerificationCount{}", found->second.blockStatusList.size(), found->second.nodeVerificationCount);
            return -4; 
        }
    }
    else if(blockStatusMap[hash].broadcastType == BroadcastType::level1BroadcastMessage)
    {
        if(found->second.level1Nodes.empty() || 
        found->second.level1Nodes.find(destNode) == found->second.level1Nodes.end())
        {
            return -5;
        }
        found->second.level1Nodes.erase(destNode);
        if(blockStatusMap[hash].blockStatusList.size() > found->second.level1_node_count * _failureRate)
        {
            return -6;
        }
    }

    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    if(nowTime > blockStatusMap[hash].block.time() + 5 * 1000000ull)
    {
        DEBUGLOG("AAAC,blockStatus nowTime:{},blockTime:{}, timeout:{}",nowTime, (blockStatusMap[hash].block.time() + 5 * 1000000ull), (nowTime - (blockStatusMap[hash].block.time() + 5 * 1000000ull))/1000000);
        return -7;
    }
    return 0;
}


void BlockStorage::AddBlockStatus(const std::string& blockHash, const CBlock& Block, const std::vector<std::string>& verifyNodes)
{
    std::unique_lock<std::mutex> lck(_statusMutex);
    if(blockStatusMap.find(blockHash) == blockStatusMap.end())
    {
        BlockStatusWrapper block_status;
        block_status.blockHash = blockHash;
        block_status.block = Block;
        block_status.broadcastType = BroadcastType::verifyBroadcast;
        block_status.nodeVerificationCount = global::ca::KReSend_node_threshold;
        block_status.verifyNodes.insert(verifyNodes.begin(), verifyNodes.end());

        blockStatusMap[blockHash] = block_status;
    }
}

void BlockStorage::checkExpiredDelete()
{
    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    std::unique_lock<std::mutex> lck(_statusMutex);
    for(auto iter = blockStatusMap.begin(); iter != blockStatusMap.end();)
    {
        if(nowTime >= iter->second.block.time() + 15ull * 1000000)
        {
            DEBUGLOG("blockStatusMap deleteBlockHash:{}", iter->second.block.hash().substr(0,6));
            blockStatusMap.erase(iter++);
        }
        else
        {
            ++iter;
        }
    }
}

void BlockStorage::AddBlockStatus(const std::string& blockHash, const CBlock& Block, const std::set<std::string>& level1Nodes)
{
    std::unique_lock<std::mutex> lck(_statusMutex);
    auto found = blockStatusMap.find(blockHash);
    if(found != blockStatusMap.end())
    {
        found->second.block = Block;
        found->second.broadcastType = BroadcastType::level1BroadcastMessage;
        found->second.level1_node_count = level1Nodes.size();
        found->second.blockStatusList.clear();
        found->second.level1Nodes.insert(level1Nodes.begin(), level1Nodes.end());
    }
}

void BlockStorage::AddBlockStatus(const BlockStatus& blockStatus)
{
    std::unique_lock<std::mutex> lck(_statusMutex);
    auto& hash = blockStatus.blockhash();
    auto ret = CheckData(blockStatus);
    if(ret != 0)
    {
        DEBUGLOG("AAAC Valid data, ret:{}, blockHash:{}, destNode:{}", ret, hash.substr(0,6), blockStatus.id());
        return;
    }

    blockStatusMap[hash].blockStatusList.push_back(blockStatus);
    DEBUGLOG("AAAC destNode:{}, blockhash:{}, blockStatus:{}, level_1_broadcast_nodes:{}, broadcastType:{}",blockStatus.id(), hash.substr(0,6), blockStatus.status(), blockStatusMap[hash].level1Nodes.size(), blockStatusMap[hash].broadcastType);

    if(blockStatusMap[hash].broadcastType == BroadcastType::level1BroadcastMessage && blockStatusMap[hash].blockStatusList.size() == (uint32_t)(blockStatusMap[hash].level1_node_count * _failureRate))
    {
        MagicSingleton<TaskPool>::GetInstance()->commit_block_task(std::bind(&BlockStorage::newBuildBlockByBlockStatus,this,hash));
    }
    else if(blockStatusMap[hash].broadcastType == BroadcastType::verifyBroadcast && blockStatusMap[hash].blockStatusList.size() == blockStatusMap[hash].nodeVerificationCount)
    {
        MagicSingleton<TaskPool>::GetInstance()->commit_block_task(std::bind(&BlockStorage::newBuildBlockByBlockStatus,this,hash));
    }
}


void BlockStorage::newBuildBlockByBlockStatus(const std::string blockHash)
{
    std::unique_lock<std::mutex> lck(_statusMutex);
    CBlock& oldBlock = blockStatusMap[blockHash].block;
    CBlock newBlock = oldBlock;
    uint32_t FailThreshold = 0;

    if(blockStatusMap[blockHash].broadcastType == BroadcastType::verifyBroadcast)
    {
        FailThreshold = blockStatusMap[blockHash].nodeVerificationCount * _failureRate;
    }
    else if(blockStatusMap[blockHash].broadcastType == BroadcastType::level1BroadcastMessage)
    {
        FailThreshold = blockStatusMap[blockHash].level1_node_count * _failureRate;
    }

    DEBUGLOG("AAAC oldBlockHash:{}, broadcastType:{}, FailThreshold:{}", blockHash.substr(0,6), blockStatusMap[blockHash].broadcastType, FailThreshold);

    newBlock.clear_txs();
    newBlock.clear_sign();
    newBlock.clear_hash();

    std::map<std::string, uint32> txsStatus;
    std::multimap<std::string, int> testMap;
    
    for(auto& iter : blockStatusMap[blockHash].blockStatusList)
    {
        for(auto& tx : iter.txstatus())
        {
            if(txsStatus.find(tx.txhash()) == txsStatus.end())
            {
                txsStatus[tx.txhash()] = 1;
            }
            else
            {
                txsStatus[tx.txhash()]++;
            }
            testMap.insert({tx.txhash(), tx.status()});
        }
    }

    for(auto &it : testMap)
    {
        DEBUGLOG("AAAC txstatus , txHash:{}, err:{}",it.first, it.second);
    }

    BlockMsg blockMsg;
    for(auto &tx : oldBlock.txs())
    {
        if(GetTransactionType(tx) != kTransactionTypeTx)
        {
            continue;
        }

        if((global::ca::TxType)tx.txtype() == global::ca::TxType::TX_TYPE_INVOKE_CONTRACT ||
        (global::ca::TxType)tx.txtype() == global::ca::TxType::kTransactionTypeDeploy)
        {
            return;
        }

        if(txsStatus.find(tx.hash()) != txsStatus.end() && txsStatus[tx.hash()] >= FailThreshold)
        {
            DEBUGLOG("AAAC delete tx txHash:{} ,poll:{}", tx.hash(), txsStatus[tx.hash()]);
            continue;
        }
        
        if(verifyTransactionTimeoutRequest(tx) != 0)
        {
            DEBUGLOG("AAAC time out tx hash = {}, blockHash:{}",tx.hash(), oldBlock.hash().substr(0,6));
            continue;
        }
        *newBlock.add_txs() = tx;
    }
    lck.unlock();
    if(newBlock.txs_size() == 0)
    {
        DEBUGLOG("newBlock.txs_size() == 0");
        return;
    }

    InitNewBlock(oldBlock, newBlock);

    std::ostringstream filestream;
    PrintBlock(newBlock, true, filestream);

    std::string test_str = filestream.str();
    DEBUGLOG("AAAC newBuildBlock --> {}", test_str);

    blockMsg.set_version(global::GetVersion());
    blockMsg.set_time(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
    blockMsg.set_block(newBlock.SerializeAsString());

    auto msg = std::make_shared<BlockMsg>(blockMsg);
	auto ret = handleBlock(msg);
    if(ret != 0)
    {
        CBlock cblock;
	    if (!cblock.ParseFromString(msg->block()))
	    {
		    ERRORLOG("fail to serialization!!");
		    return;
	    }
        ERRORLOG("AAAC handleBlock failed The error code is {}, block hash : {}",ret, cblock.hash().substr(0, 6));

        return;
    }

    DEBUGLOG("AAAC newBuildBlock success oldBlockHash:{}, newBlockHash:{}",oldBlock.hash().substr(0,6), newBlock.hash().substr(0,6));
    return;
}

int BlockStorage::InitNewBlock(const CBlock& oldBlock, CBlock& newBlock)
{
	newBlock.set_version(global::ca::kCurrentBlockVersion);
	newBlock.set_time(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
	newBlock.set_height(oldBlock.height());
    newBlock.set_prevhash(oldBlock.prevhash());
	newBlock.set_merkleroot(ca_algorithm::calculateBlockMerkle(newBlock));
	newBlock.set_hash(Getsha256hash(newBlock.SerializeAsString()));
	return 0;
}


void sendSeekGetPreHashRequest(const std::string &nodeId, const std::string &msgId, uint64_t seekHeight)
{
    SeekPreHashByHightReq req;
    req.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    req.set_msg_id(msgId);
    req.set_seek_height(seekHeight);
    NetSendMessage<SeekPreHashByHightReq>(nodeId, req, net_com::Compress::kCompressDisabled, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    return;
}

void sendPreHashAcknowledgment(SeekPreHashByHightAck& ack,const std::string &nodeId, const std::string &msgId, uint64_t seekHeight)
{
    DEBUGLOG("sendPreHashAcknowledgment, id:{}, height:{}",  nodeId, seekHeight);
    ack.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    DBReader dbReader;
    uint64_t nodeSelfHeight = 0;
    if (0 != dbReader.getBlockTop(nodeSelfHeight))
    {
        ERRORLOG("getBlockTop(txn, top)");
        return;
    }
    ack.set_msg_id(msgId);
    std::vector<std::string> blockHashes;
    if(seekHeight > nodeSelfHeight)
    {
        DEBUGLOG("seekHeight:{} > nodeSelfHeight:{}", seekHeight, nodeSelfHeight);
        return;
    }

    if (DBStatus::DB_SUCCESS != dbReader.getBlockHashsByBlockHeight(seekHeight, blockHashes))
    {
        ERRORLOG("getBlockHashsByBlockHeight fail !!!");
        return;
    }
    ack.set_seek_height(seekHeight);
    for(auto &hash : blockHashes)
    {
        ack.add_prehashes(hash);
    }
    
    return;
}

int handleSeekGetPreHashRequest(const std::shared_ptr<SeekPreHashByHightReq> &msg, const MsgData &msgData)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgData.fd, msg->self_node_id()))
    {
        return -1;
    }
    SeekPreHashByHightAck ack;
    sendPreHashAcknowledgment(ack,msg->self_node_id(), msg->msg_id(), msg->seek_height());
    NetSendMessage<SeekPreHashByHightAck>(msg->self_node_id(), ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    return 0;
}
int preHashAcknowledgment(const std::shared_ptr<SeekPreHashByHightAck> &msg, const MsgData &msgData)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgData.fd, msg->self_node_id()))
    {
        return -1;
    }

    dataMgrPtr.waitDataToAdd(msg->msg_id(), msg->self_node_id(), msg->SerializeAsString());
    return 0;
}

int protocolBlockStatus(const BlockStatus& blockStatus, const std::string destNode)
{
    NetSendMessage<BlockStatus>(destNode, blockStatus, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    return 0;
}

int blockStatusMessageHandler(const std::shared_ptr<BlockStatus> &msg, const MsgData &msgData)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgData.fd, msg->id()))
    {
        return -1;
    }

    MagicSingleton<BlockStorage>::GetInstance()->AddBlockStatus(*msg);
    return 0;
}

int BlockStorage::BlockFlowSignVerifier(const BlockMsg & blockMsg)
{
    CBlock block;
    block.ParseFromString(blockMsg.block());

	// Verify Block flow verifies the signature of the node
    std::vector<std::string> signNodes;
    
    MagicSingleton<VRF>::GetInstance()->getBlockFlowSignNodes(block.hash(), signNodes);

    //The signature node in the block flow
    std::vector<std::string> verifyNodes;
    std::string defaultaddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    for(auto &item : block.sign())
    {
        std::string addr = GenerateAddr(item.pub());
        if(addr != defaultaddr)
        {
            verifyNodes.push_back(addr);
        }
    }
    
    
    //Compare whether the nodes in the two containers are consistent
    for(auto & signNode : verifyNodes)
    {
        if(std::find(signNodes.begin(), signNodes.end(), signNode) == signNodes.end())
        {
            ERRORLOG(" The nodes in the two containers are inconsistent = {}, blockHash:{}",signNode, block.hash());
            return -1;
        }
    }

    return 0;
}


