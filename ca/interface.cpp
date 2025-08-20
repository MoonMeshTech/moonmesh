#include "ca/global.h"
#include "ca/txhelper.h"
#include "ca/interface.h"
#include "ca/algorithm.h"
#include "ca/transaction.h"
#include "ca/sync_block.h"
#include "ca/dispatchtx.h"

#include "ca/resend_reconnect_node.h"
#include "ca/transaction_cache.h"
#include "ca/bonus_addr_cache.h"

#include "utils/util.h"
#include "utils/time_util.h"
#include "utils/magic_singleton.h"
#include "utils/account_manager.h"
#include "utils/contract_utils.h"
#include "ca/dispatchtx.h"

#include "db/db_api.h"
#include "net/interface.h"
#include "common/global_data.h"
#include "include/scope_guard.h"


std::string NetGetSelfNodeId()
{
	return MagicSingleton<PeerNode>::GetInstance()->GetSelfId();
}




/*************************************SDK access block height, utxo, pledge list, Delegating list, block request*************************************/
int get_sdk_all_need(const std::shared_ptr<GetSDKReq> & req, GetSDKAck & ack)
{
    ack.set_version(global::GetVersion());
    ack.set_type(req->type());
    DEBUGLOG("req type:{}",req->type());
    std::vector<std::string> fromAddr;

    for(int i = 0;i<req->address_size();++i)
    {
        fromAddr.emplace_back(req->address(i));
    }

    if (fromAddr.empty() )
    {
        ack.set_code(-1);
        std::cout<<"request is empty()"<<std::endl;
        ack.set_message("request is empty()");
        return -1;
    }

    for(auto& from : fromAddr)
	{
		if (!isValidAddress(from))
		{
            ack.set_code(-2);
            ack.set_message("request is isValidAddress failed ");
            std::cout<<"request is isValidAddress failed"<<std::endl;
            ERRORLOG("Fromaddr is a non  address!");
            return -2;
		}
	}

    std::vector<Node> nodelist;
	Node selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
	std::vector<Node> tmp = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	nodelist.insert(nodelist.end(), tmp.begin(), tmp.end());
	nodelist.push_back(selfNode);
    
    std::vector<Node> satisfiedNode;
    for(const auto & node : nodelist)
    {
        //Verification of Delegating and pledge
        int ret = VerifyBonusAddr(node.address);
        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
        if (stakeTime > 0 && ret == 0)
        {
            satisfiedNode.push_back(node);
        }
    }
   
    std::sort(satisfiedNode.begin(), satisfiedNode.end(), [&](const Node &n1, const Node &n2)
			  { return n1.height < n2.height; });
    uint64_t height  = satisfiedNode[satisfiedNode.size()-1].height;


    for(auto &node :satisfiedNode)
    {
       SDKNodeInfo *nodeinfo =  ack.add_nodeinfo();
       nodeinfo->set_height(node.height);
       nodeinfo->set_listen_ip(node.listenIp);
       nodeinfo->set_listen_port(node.listenPort);
       nodeinfo->set_public_ip(node.publicIp);
       nodeinfo->set_public_port(node.publicPort);
       nodeinfo->set_addr(node.address);
       nodeinfo->set_pub(node.pub);
       nodeinfo->set_sign(node.sign);
       nodeinfo->set_identity(node.identity);
    }


    auto nodeinfolist = ack.nodeinfo();
    std::cout<<"nodeinfolist = "<<nodeinfolist.size()<<std::endl;
    
    if(height == 0)
    {
        ack.set_code(-3);
        ack.set_message("The height is zero.");
        return -3;
    }
    ack.set_height(height);

    //get utxo
    std::vector<TxHelper::Utxo> sdkUtxos;
    for(auto& from : fromAddr)
    {
        std::vector<TxHelper::Utxo> single_address_utxos;
        int ret = TxHelper::GetUtxos(from, single_address_utxos);
        if (ret != 0)
        {
            std::cout<<"TxHelper::GetUtxos"<<std::endl;
            return ret -= 10;
        }
        for(auto &uxto : single_address_utxos)
        {
            sdkUtxos.emplace_back(uxto);
        }
    }

    for (auto & item : sdkUtxos)
    {
        SDKUtxo* utxo = ack.add_utxos();
        utxo->set_address(item.addr);
        utxo->set_hash(item.hash);
        utxo->set_value(item.value);
        utxo->set_n(item.n);
    }
    
    //print utxo
    auto v= ack.utxos();
    for(auto &t : v)
    {
        std::cout<< t.address() << std::endl;
        std::cout<< t.value() << std::endl;
        std::cout<< t.hash() << std::endl;
        std::cout<< t.n() << std::endl;
    }

     //get block
    std::vector<std::string> hashes;
    DBReader dbReader;
	
	if (DBStatus::DB_SUCCESS != dbReader.getBlockHashsByBlockHeight(height, hashes))
	{
        ack.set_code(-4);
        ack.set_message("Get block hash failed");
        std::cout<<"Get block hash failed"<<std::endl;
		return -4;
	}

	std::vector<CBlock> blocks;
	for (const auto &hash : hashes)
	{
		std::string blockStr;
		dbReader.getBlockByBlockHash(hash, blockStr);
        ack.add_blocks(blockStr);
	}

    std::vector<std::string> stakeAddresses;
	if (dbReader.getStakeAddr(stakeAddresses) != DBStatus::DB_SUCCESS)
	{
        std::cout<<"Get all stake address failed!"<<std::endl;
	}
    for(auto &addr :stakeAddresses)
    {
        ack.add_pledgeaddr(addr);
    }

    std::string assetType;
    auto ret = ca_algorithm::GetCanBeRevokeAssetType(assetType);
    if(ret != 0){
        ERRORLOG("Get Can BeRevoke AssetType fail!");
        ack.set_code(-5);
        ack.set_message("Get Can BeRevoke AssetType fail!");
    }

    std::vector<std::string> utxos;
    if (dbReader.getStakeAddrUtxo(fromAddr.at(0), assetType, utxos) != DBStatus::DB_SUCCESS)
	{
        std::cout<<"Get stake utxo from address failed!"<<std::endl;
	}
    for(auto &utxo :utxos)
    {
        ack.add_pledgeutxo(utxo);
    }

    for(auto& from : stakeAddresses)
    {
        std::vector<std::string> utxoes;
        auto dbStatus = dbReader.getStakeAddrUtxo(from, assetType, utxoes);
        if (DBStatus::DB_SUCCESS != dbStatus)
        {
            ack.set_code(-4);
            ack.set_message("Get Stake Address Utxo failed");
            std::cout<<"Get Stake Address Utxo failed"<<std::endl;
            continue;
        }

        for (auto & utxoDataString: utxoes)
        {
            std::string serialTxRawData;
            auto dbStatus = dbReader.getTransactionByHash(utxoDataString, serialTxRawData);
            if (DBStatus::DB_SUCCESS != dbStatus)
            {
                ERRORLOG("Get stake tx error");
                std::cout<<"Get stake tx error!"<<std::endl;
                continue;
            }
            SDKPledgeTx *pledgeTransactionRequest  = ack.add_pledgetx();
            pledgeTransactionRequest->set_address(from);
            pledgeTransactionRequest->set_tx(serialTxRawData);
            pledgeTransactionRequest->set_utxo(utxoDataString);
            std::cout<<"***************** "<<std::endl;  
        }
    }

    std::cout<<"ack.pledgetx() size = "<<ack.pledgetx().size()<<std::endl;
    for(auto& bonusAddr : fromAddr)
    {
        uint64_t delegateAmount;
        
        auto ret = MagicSingleton<mm::ca::BonusAddrCache>::GetInstance()->getAmount(bonusAddr, delegateAmount);
        if (ret < 0)
        {
            ERRORLOG("delegating BonusAddr: {}, ret:{}", bonusAddr, ret);
            std::cout<<"delegating BonusAddr: {}, ret:{}"<<std::endl;
            continue;
        }
        SDKBonusamout * sdkBonusValue = ack.add_bonusamount();
        sdkBonusValue->set_address(bonusAddr);
        sdkBonusValue->set_delegating_amount(delegateAmount);
    }
    
    std::vector<std::string> bonusAddr;
	auto status = dbReader.getBonusAddrByDelegatingAddr(fromAddr.at(0), bonusAddr);
	if (status == DBStatus::DB_SUCCESS && !bonusAddr.empty())
	{
        std::cout<<"The delegatingor have already delegatinged in a node!"<<std::endl;
	}

    for(auto &addr :bonusAddr)
    {
        ack.add_bonusaddr(addr);
    }

    std::vector<std::string> addresses;
    status = dbReader.getDelegatingAddrByBonusAddr(req->toaddr(), addresses);
	if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
	{
        std::cout<<"Get delegating addrs by node failed!!"<<std::endl;
	}

    for(auto &addr :addresses)
    {
        ack.add_delegatingedaddr(addr);;
    }

   
   for (auto &address : addresses)
   {
        std::vector<std::string> utxos;
        if (dbReader.getBonusAddrDelegatingAddrUtxoByBonusAddr(req->toaddr(), address, utxos) != DBStatus::DB_SUCCESS)
        {
            std::cout<<"getBonusAddrDelegatingAddrUtxoByBonusAddr failed!"<<std::endl;
            continue;
        }
        for (const auto &utxo : utxos)
        {
            ack.add_bonusaddrdelegateutxos(utxo);
            std::string strDelegatingTx;
            if (DBStatus::DB_SUCCESS != dbReader.getTransactionByHash(utxo, strDelegatingTx))
            {
                ERRORLOG("Delegating tx not found!");
                std::cout<<"Delegating tx not found!"<<std::endl;
                continue;
            }
            SDKBonusTx * adkBonusTransmitRequest = ack.add_bonustx();
            adkBonusTransmitRequest->set_address(address);
            adkBonusTransmitRequest->set_utxo(utxo);
            adkBonusTransmitRequest->set_tx(strDelegatingTx);
        }
   }


    uint64_t totalCirculation = 0;

    ack.set_m2(totalCirculation);

    uint64_t Totaldelegating=0;
    if (DBStatus::DB_SUCCESS != dbReader.getTotalDelegatingAmount(Totaldelegating))
    {
        std::cout<<"getTotalDelegatingAmount failed!"<<std::endl;
    }
    ack.set_totaldelegating(Totaldelegating);


    std::map<std::string, double> addr_percent;
    std::unordered_map<std::string, uint64_t> addrSignCount;
    uint64_t curTime = req->time();
    ret = ca_algorithm::fetchAbnormalSignAddrListByPeriod(curTime, addr_percent, addrSignCount);
    if(ret < 0) 
    {
        std::cout<<"fetchAbnormalSignAddrListByPeriod failed!"<<std::endl;
    }
   

    std::cout<<"addrSignCount size = "<<addrSignCount.size()<<std::endl;
    for(auto& kv:addrSignCount)
    {
        AbnormalAddrCnt *banOrCount =  ack.add_abnormaladdr_cnt();
        banOrCount->set_address(kv.first);
        banOrCount->set_count(kv.second);
    }
    
    std::vector<std::string> delegatingAddresses;
    status = dbReader.getDelegatingAddrByBonusAddr(fromAddr.at(0), delegatingAddresses);
	if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
	{
        std::cout<<"Get delegating addrs by node failed!!"<<std::endl;
	}

    for(auto &Addr :delegatingAddresses)
    {
        ack.add_claimdelegatingedaddr(Addr);
    }
    
    for (auto &address : delegatingAddresses)
   {
        std::vector<std::string> utxos;
        if (dbReader.getBonusAddrDelegatingAddrUtxoByBonusAddr(fromAddr.at(0), address, utxos) != DBStatus::DB_SUCCESS)
        {
            std::cout<<"getBonusAddrDelegatingAddrUtxoByBonusAddr failed!"<<std::endl;
            continue;
        }
        for (const auto &utxo : utxos)
        {
            ack.add_claimbonusaddrdelegateutxos(utxo);
            std::string strDelegatingTx;
            if (DBStatus::DB_SUCCESS != dbReader.getTransactionByHash(utxo, strDelegatingTx))
            {
                std::cout<<"Delegating tx not found!"<<std::endl;
                continue;
            }
            
            SDKClaimBonusTx * claimbonustx = ack.add_claimbonustx();
            claimbonustx->set_address(address);
            claimbonustx->set_utxo(utxo);
            claimbonustx->set_tx(strDelegatingTx);
        }
   }

    std::vector<std::string> claimed_utxos;
    uint64_t Period = MagicSingleton<TimeUtil>::GetInstance()->GetPeriod(curTime);
	
    auto retstatus = dbReader.getBonusUtxoByPeriod(Period, claimed_utxos);
    if (retstatus != DBStatus::DB_SUCCESS && retstatus != DBStatus::DB_NOT_FOUND)
	{
		std::cout<<"getBonusUtxoByPeriod failed!"<<std::endl;
	}

    for(auto utxo = claimed_utxos.rbegin(); utxo != claimed_utxos.rend(); utxo++)
    {
        std::string strTx;
        if (DBStatus::DB_SUCCESS != dbReader.getTransactionByHash(*utxo, strTx) )
        {
            std::cout<<"Delegating tx not found!"<<std::endl;
            continue;
        }
        Claimtx * claimtx = ack.add_claimtx();
        claimtx->set_utxo(*utxo);
        claimtx->set_tx(strTx);
    }
    ack.set_code(0);
    ack.set_message("success");
    return 0;
}


std::map<int32_t, std::string> getSdkAllNeedRequest()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair( 0, "getSdkAllNeedRequest Success"), 

											 };
	return errInfo;												
}
//SDK access block height, utxo, pledge list, Delegating list, block request
int getSdkAllNeedsRequest(const std::shared_ptr<GetSDKReq>& req, const MsgData & msgData)
{
    auto errInfo = getSdkAllNeedRequest();
    GetSDKAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        returnAcknowledgmentCode<GetSDKAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = get_sdk_all_need(req, ack);
    if (ret != 0)
	{
		return ret -= 10;
	}

    return ret;
}

std::map<int32_t, std::string> requestBlockActionCode()
{
	std::map<int32_t, std::string> errInfo = {  
                                                std::make_pair(-1, "The version is wrong"),
												std::make_pair(-12, "By block height failure"), 
												};

	return errInfo;												
}
int getBlockRequestImplementation(const std::shared_ptr<GetBlockReq>& req, GetBlockAck & ack)
{
	ack.set_version(global::GetVersion());

    DBReader dbReader;
    std::vector<std::string> hashes;
	uint64_t top = req->height();
    uint64_t blockHeight = top;
	if(top >= global::ca::MIN_UNSTAKE_HEIGHT)
	{
		blockHeight = top - 10;
	}
	
	if (DBStatus::DB_SUCCESS != dbReader.getBlockHashsByBlockHeight(blockHeight, hashes))
	{
        ack.set_code(-2);
        ack.set_message("Get block hash failed");
		return -2;
	}

	std::vector<CBlock> blocks;
	for (const auto &hash : hashes)
	{
		std::string blockStr;
		dbReader.getBlockByBlockHash(hash, blockStr);
		CBlock block;
		block.ParseFromString(blockStr);
		blocks.push_back(block);
	}
	std::sort(blocks.begin(), blocks.end(), [](const CBlock &x, const CBlock &y)
			  { return x.time() < y.time(); });

    
    for(const auto &block:blocks)
    {
        BlockItem *blockitem = ack.add_list();
        blockitem->set_blockhash(block.hash());
        for(int i = 0; i<block.sign().size();  ++i)
        {
            blockitem->add_addr(GenerateAddr(block.sign(i).pub())) ;
        }
    }
    {
        std::vector<std::string> blockHashes;
        if (DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(top, top, blockHashes))
        {
            ERRORLOG("can't getBlockHashesByBlockHeight");
            return false;
        }

        std::vector<CBlock> blocksTime;
        for (auto &hash : blockHashes)
        {
            std::string blockStr;
            if(DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(hash, blockStr))
            {
                ERRORLOG("getBlockByBlockHash error block hash = {} ", hash);
                return false;
            }

            CBlock block;
            if(!block.ParseFromString(blockStr))
            {
                ERRORLOG("block parse from string fail = {} ", blockStr);
                return false;
            }
            blocksTime.push_back(block);
        }

        std::sort(blocksTime.begin(), blocksTime.end(), [](const CBlock& x, const CBlock& y){ return x.time() < y.time(); });
        ack.set_timestamp(blocksTime.at(blocksTime.size()-1).time());
	
    }

    ack.set_code(0);
    ack.set_message("success");
    ack.set_height(blockHeight);
	return 0;
}
int handleBlockFetchRequest(const std::shared_ptr<GetBlockReq>& req, const MsgData & msgData)
{

    auto errInfo = requestBlockActionCode();
    GetBlockAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        returnAcknowledgmentCode<GetBlockAck>(msgData, errInfo, ack, ret); 
    };
    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}
    
	ret = getBlockRequestImplementation(req, ack);
	if (ret != 0)
	{
		return ret -= 10;
	}

    return 0;
}
/*************************************Get the balance*************************************/
int getBalanceRequestImpl(const std::shared_ptr<GetBalanceReq>& req, GetBalanceAck & ack)
{
	ack.set_version(global::GetVersion());

    std::string addr = req->address();
    if(addr.size() == 0)
    {
        return -1;
    } 

    if (!isValidAddress(addr))
    {
        return -2;
    }

    DBReader dbReader;

    uint64_t blockHeight = 0;
    dbReader.getBlockTop(blockHeight);

	std::vector<std::string> addr_utxo_hashes;
    DBStatus dbStatus = dbReader.getUtxoHashsByAddress(addr, addr_utxo_hashes);
    if (DBStatus::DB_SUCCESS != dbStatus)
    {
        if (dbStatus == DBStatus::DB_NOT_FOUND)
        {
            return -3;
        }
        else 
        {
            return -4;
        }
    }
	
	uint64_t balance = 0;
	std::string txRaw;
	CTransaction tx;
	for (auto utxo_hash : addr_utxo_hashes)
	{
		if (DBStatus::DB_SUCCESS != dbReader.getTransactionByHash(utxo_hash, txRaw))
		{
			return -5;
		}
		if (!tx.ParseFromString(txRaw))
		{
            return -6;
		}
        for(auto &utxo :tx.utxos())
        {
        for (auto &vout : utxo.vout())
		{
			if (vout.addr() == addr)
			{
				balance += vout.value();
			}
		}
        }
	}

    ack.set_address(addr);
    ack.set_balance(balance);
    ack.set_height(blockHeight);
    ack.set_code(0);
    ack.set_message("success");

	return 0;
}

std::map<int32_t, std::string> getBalanceRequestCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get Amount Success"), 
												std::make_pair(-1, "addr is empty"), 
												std::make_pair(-2, " addr invalid"), 
												std::make_pair(-3, "search balance not found"),
                                                std::make_pair(-4, "get tx failed"),
                                                std::make_pair(-5, "getTransactionByHash failed!"),
                                                std::make_pair(-6, "parse tx failed"),
												};

	return errInfo;												
}
int getBalanceRequest(const std::shared_ptr<GetBalanceReq>& req, const MsgData& msgData)
{
    auto errInfo = getBalanceRequestCode();
    GetBalanceAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{

        returnAcknowledgmentCode<GetBalanceAck>(msgData, errInfo, ack, ret);
        
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}
    
	ret = getBalanceRequestImpl(req, ack);
	if (ret != 0)
	{
		return ret -= 10;
	}

    return ret;    
}
/*************************************Get node information*************************************/
int getNodeInfoRequestImplementation(const std::shared_ptr<GetNodeInfoReq>& req, GetNodeInfoAck & ack)
{
	ack.set_version(global::GetVersion());

    ack.set_address(MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr());
    
    Node selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
    ack.set_ip(IpPort::IpSz(selfNode.publicIp));

    DBReader dbReader;
	uint64_t height = 0;
    DBStatus dbStatus = dbReader.getBlockTop(height);
    if (DBStatus::DB_SUCCESS != dbStatus)
    {
        return -1;
    }

    ack.set_height(height);
    ack.set_ver(global::GetVersion());

    ack.set_code(0);
    ack.set_message("success");

	return 0;
}

std::map<int32_t, std::string> getNodeInfoRequestCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get Node Info Success"), 
												std::make_pair(-1, "Invalid Version"), 
												std::make_pair(-11, "Get Top Failed"),
												std::make_pair(-12, "Get Gas Failed"),
												};

	return errInfo;												
}
int handleGetNodeInfoRequest(const std::shared_ptr<GetNodeInfoReq>& req, const MsgData& msgData)
{
    auto errInfo = getNodeInfoRequestCode();
    GetNodeInfoAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{

        returnAcknowledgmentCode<GetNodeInfoAck>(msgData, errInfo, ack, ret);
        
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}
    
	ret = getNodeInfoRequestImplementation(req, ack);
	if (ret != 0)
	{
		return ret -= 10;
	}

    return ret;
}
/*************************************Stake list*************************************/
int getStakeListRequestImplementation(const std::shared_ptr<GetStakeListReq>& req, GetStakeListAck & ack)
{
	ack.set_version(global::GetVersion());

	std::string addr = req->addr();
    if (addr.length() == 0)
    {
        return -1;
    }

	if (!isValidAddress(addr))
    {
        return -2;
    }

    std::string assetType;
    int ret = ca_algorithm::GetCanBeRevokeAssetType(assetType);
    if(ret != 0){
        ERRORLOG("Get Can BeRevoke AssetType fail!");
    }

    std::vector<std::string> utxoes;
    DBReader dbReader;
    auto dbStatus = dbReader.getStakeAddrUtxo(addr, assetType, utxoes);
    if (DBStatus::DB_SUCCESS != dbStatus)
    {
        return -3;
    }

    if (utxoes.size() == 0)
    {
        return -4;
    }

    reverse(utxoes.begin(), utxoes.end());

    for (auto & utxoDataString: utxoes)
    {
        std::string serialTxRawData;
        dbStatus = dbReader.getTransactionByHash(utxoDataString, serialTxRawData);
        if (DBStatus::DB_SUCCESS != dbStatus)
        {
            ERRORLOG("Get stake tx error");
            continue;
        }

        CTransaction tx;
        tx.ParseFromString(serialTxRawData);

        for(const auto& utxo : tx.utxos())
            {
                if(utxo.gasutxo() == 1)
                {
                    if(utxo.vout_size() != 2)
                    {
                        ERRORLOG("invalid tx");
                        continue;
                    }
                }

                if(utxo.vout_size() != 3)
                {
                    ERRORLOG("invalid tx");
                    continue;
                }
        }

        if (tx.hash().length() == 0)
        {
            ERRORLOG("Get stake tx error");
            continue;
        }

        std::string strBlockHash;
        dbStatus = dbReader.getBlockHashByTransactionHash(tx.hash(), strBlockHash);
        if (DBStatus::DB_SUCCESS != dbStatus)
        {
            ERRORLOG("Get stake block hash error");
            continue;
        }

        std::string serBlock;
        dbStatus = dbReader.getBlockByBlockHash(strBlockHash, serBlock);
        if (dbStatus != 0)
        {
            ERRORLOG("Get stake block error");
            continue;
        }

        CBlock block;
        block.ParseFromString(serBlock);

        if (block.hash().empty())
        {
            ERRORLOG("Block error");
            continue;
        }

        std::vector<std::string> transactionOwnerVector = {};
        for (auto &utxo : tx.utxos())
        {
            transactionOwnerVector.insert(transactionOwnerVector.end(), utxo.owner().begin(), utxo.owner().end());
        }
        if (transactionOwnerVector.size() == 0)
        {
            continue;
        }

        StakeItem * pItem = ack.add_list();
        
        pItem->set_blockhash(block.hash());
        pItem->set_blockheight(block.height());
        pItem->set_utxo(utxoDataString);
        pItem->set_time(tx.time());

        pItem->set_fromaddr(transactionOwnerVector[0]);

        for (auto &utxo : tx.utxos())
        {
            for (int i = 0; i < utxo.vout_size(); i++)
            {
                CTxOutput txout = utxo.vout(i);
                if (txout.addr() == global::ca::kTargetAddress)
                {
                    pItem->set_toaddr(txout.addr());
                    pItem->set_amount(txout.value());
                    break;
                }
            }
        }

            if ((global::ca::TxType)tx.txtype() != global::ca::TxType::TX_TYPE_TX)
            {
                nlohmann::json dataJson = nlohmann::json::parse(tx.data());
                pItem->set_detail(dataJson["TxInfo"]["StakeType"].get<std::string>());
            }
        }

    ack.set_code(0);
    ack.set_message("success");

	return 0;
}

std::map<int32_t, std::string> getStakeListRequestCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get Stake List Success"), 
												std::make_pair(-1, "addr is empty !"), 
												std::make_pair(-2, " addr invalid"), 
												std::make_pair(-3, "Get Stake utxo error"),
                                                std::make_pair(-4, "No stake"),
												};

	return errInfo;												
}
int handleGetStakeListRequest(const std::shared_ptr<GetStakeListReq>& req, const MsgData & msgData)
{
	auto errInfo = getStakeListRequestCode();
    GetStakeListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        returnAcknowledgmentCode<GetStakeListAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

	ret = getStakeListRequestImplementation(req, ack);
	if (ret != 0)
	{
		return ret -= 10;
	}
	return ret;
}

/*************************************List of Delegatings*************************************/
int GetDelegateListReqImpl(const std::shared_ptr<GetDelegateListReq>& req, GetDelegateListAck & ack)
{
	ack.set_version(global::GetVersion());
        
    std::string addr = req->addr();
    if (addr.length() == 0)
    {
        return -1;
    }

	if (!isValidAddress(addr))
    {
        return -2;
    }

    std::vector<std::string> utxoes;
    std::vector<std::string> bonus_addresses_list;

    DBReader dbReader;
    auto dbStatus = dbReader.getBonusAddrByDelegatingAddr(addr, bonus_addresses_list);
    if (DBStatus::DB_SUCCESS != dbStatus)
    {
        return -3;
    }

    for (auto & bonusAddr : bonus_addresses_list)
    {
        dbStatus = dbReader.getBonusAddrDelegatingAddrUtxoByBonusAddr(bonusAddr, addr, utxoes);
        if (DBStatus::DB_SUCCESS != dbStatus)
        {
            return -4;
        }

        if (utxoes.size() == 0)
        {
            return -5;
        }

        reverse(utxoes.begin(), utxoes.end());

        for (auto & utxoDataString: utxoes)
        {
            std::string serialTxRawData;
            dbStatus = dbReader.getTransactionByHash(utxoDataString, serialTxRawData);
            if (DBStatus::DB_SUCCESS != dbStatus)
            {
                ERRORLOG("Get delegating tx error");
                continue;
            }

            CTransaction tx;
            tx.ParseFromString(serialTxRawData);

            if (tx.hash().length() == 0)
            {
                ERRORLOG("Get delegating tx error");
                continue;
            }

            std::string strBlockHash;
            dbStatus = dbReader.getBlockHashByTransactionHash(tx.hash(), strBlockHash);
            if (DBStatus::DB_SUCCESS != dbStatus)
            {
                ERRORLOG("Get pledge block hash error");
                continue;
            }

            std::string serBlock;
            dbStatus = dbReader.getBlockByBlockHash(strBlockHash, serBlock);
            if (dbStatus != 0)
            {
                ERRORLOG("Get delegating block error");
                continue;
            }

            CBlock block;
            block.ParseFromString(serBlock);

            if (block.hash().empty())
            {
                ERRORLOG("Block error");
                continue;
            }

            std::vector<std::string> transactionOwnerVector = {};
            for (auto &utxo : tx.utxos())
            {
                transactionOwnerVector.insert(transactionOwnerVector.end(), utxo.owner().begin(), utxo.owner().end());
            }

            if (transactionOwnerVector.size() == 0)
            {
                continue;
            }

            DelegateItem *pItem = ack.add_list();

            pItem->set_blockhash(block.hash());
            pItem->set_blockheight(block.height());
            pItem->set_utxo(utxoDataString);
            pItem->set_time(tx.time());

            pItem->set_fromaddr(transactionOwnerVector[0]);

            for (auto &utxo : tx.utxos())
            {
                for (int i = 0; i < utxo.vout_size(); i++)
                {
                    CTxOutput txout = utxo.vout(i);
                    if (txout.addr() == global::ca::kVirtualDelegatingAddr)
                    {
                        pItem->set_toaddr(txout.addr());
                        pItem->set_amount(txout.value());
                        break;
                    }
                }
            }
                if ((global::ca::TxType)tx.txtype() != global::ca::TxType::TX_TYPE_TX)
                {
                    nlohmann::json dataJson = nlohmann::json::parse(tx.data());
                    pItem->set_detail(dataJson["TxInfo"].dump());
                }
            }
    }

    ack.set_code(0);
    ack.set_message("success");


    return 0;
}

std::map<int32_t, std::string> GetDelegateListReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair( 0, "Get Delegating List Success"), 
												std::make_pair(-1, "addr is empty !"), 
												std::make_pair(-2, " addr invalid !"), 
												std::make_pair(-3, "GetBonusAddr failed !"),
												std::make_pair(-4, "GetBonusAddrDelegateUtxos failed !"),
                                                std::make_pair(-5, "No Delegating"),
												};

	return errInfo;												
}

int HandleGetDelegateListReq(const std::shared_ptr<GetDelegateListReq>& req, const MsgData & msgData)
{
    auto errInfo = GetDelegateListReqCode();
    GetDelegateListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        returnAcknowledgmentCode<GetDelegateListAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

	ret = GetDelegateListReqImpl(req, ack); 
	if (ret != 0)
	{
		return ret -= 10;
	}
	return ret;
}
/*************************************Query UTXO*************************************/
int getUtxoRequestImpl(const std::shared_ptr<GetUtxoReq>& req, GetUtxoAck & ack)
{
    ack.set_version(global::GetVersion());

    std::string address = req->address();
    if (address.empty() || !isValidAddress(address))
    {
        return -1;
    }

    ack.set_address(address);

    std::vector<TxHelper::Utxo> utxos;
    int ret = TxHelper::GetUtxos(address, utxos);
    if (ret != 0)
    {
        return ret -= 10;
    }

    for (auto & item : utxos)
    {
        Utxo* utxo = ack.add_utxos();
        utxo->set_hash(item.hash);
        utxo->set_value(item.value);
        utxo->set_n(item.n);
    }
    
    ack.set_code(0);
    ack.set_message("success");

    return 0;
}

std::map<int32_t, std::string> getUtxoRequestCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair( 0,  "Get Utxo Success"), 
												std::make_pair(-1,  "The addr is empty /  addr invalid"), 
												std::make_pair(-12, "GetUtxos : getUtxoHashsByAddress failed !"),
												};

	return errInfo;												
}
int handleGetUtxoRequest(const std::shared_ptr<GetUtxoReq>& req, const MsgData & msgData)
{
    auto errInfo = getUtxoRequestCode();
    GetUtxoAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        returnAcknowledgmentCode<GetUtxoAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = getUtxoRequestImpl(req, ack);
    if (ret != 0)
    {
        return ret -= 100;
    }

    return 0;
}

/*************************************Query all Delegating accounts and amounts on the delegatingee node*************************************/
int GetAllDelegatingAddressReqImpl(const std::shared_ptr<GetAllDelegateAddressReq>& req, GetAllDelegateAddressAck & ack)
{
    ack.set_version(global::GetVersion());
    
    std::string address = req->addr();
    if (address.empty() || !isValidAddress(address))
    {
        return -1;
    }

    ack.set_addr(address);

    DBReader dbReader;
    std::vector<std::string> addresses;
    auto dbStatus = dbReader.getDelegatingAddrByBonusAddr(address,addresses);
    if (dbStatus != DBStatus::DB_SUCCESS)
    {
        return -2;
    }

    if(addresses.size() == 0)
    {
        return -3;
    }

    for(auto& addr : addresses)
    {
        std::vector<std::string> utxos;
	    uint64_t total = 0;
        dbStatus = dbReader.getBonusAddrDelegatingAddrUtxoByBonusAddr(address,addr,utxos);
        if (dbStatus != DBStatus::DB_SUCCESS)
        {
            return -4;
        }
        for (auto &item : utxos) 
        {
            std::string txRawData;
            if (DBStatus::DB_SUCCESS != dbReader.getTransactionByHash(item, txRawData))
            {
                return -5;
            }
            CTransaction utxoTx;
            utxoTx.ParseFromString(txRawData);
            for (auto &utxo : utxoTx.utxos())
            {
                for (auto &vout : utxo.vout())
                {
                    if (vout.addr() == global::ca::kVirtualDelegatingAddr)
                    {
                        total += vout.value();
                    }
                }
            }
        }
        DelegateAddressItem * item = ack.add_list();
        item->set_addr(addr);
        item->set_value(total);
    }
    ack.set_code(0);
    ack.set_message("success");
    return 0;
}
std::map<int32_t, std::string> GetAllDelegatingAddressReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get AllDelegatingAddress Success"), 
												std::make_pair(-1, "The addr is empty /  addr invalid"), 
												std::make_pair(-2, "getDelegatingAddrByBonusAddr failed !"), 
												std::make_pair(-3, "No Delegatinged addrs !"),
                                                std::make_pair(-4, "GetBonusAddrDelegateUtxos failed !"),
                                                std::make_pair(-5, "getTransactionByHash failed !"),
												};

	return errInfo;												
}
int HandleGetAllDelegatingAddressReq(const std::shared_ptr<GetAllDelegateAddressReq>& req, const MsgData & msgData)
{
    auto errInfo = GetAllDelegatingAddressReqCode();
    GetAllDelegateAddressAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        returnAcknowledgmentCode<GetAllDelegateAddressAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = GetAllDelegatingAddressReqImpl(req, ack);
    if (ret != 0)
    {
        return ret -= 10;
    }

    return 0;
}

/*************************************Get all delegatingable nodes*************************************/
int stakeNodeListRequestImpl(const std::shared_ptr<GetAllStakeNodeListReq>& req, GetAllStakeNodeListAck & ack)
{
    ack.set_version(global::GetVersion());

	std::vector<Node> nodelist;
    
	Node selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
	std::vector<Node> tmp = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    if(tmp.empty())
    {
        ack.set_code(-1);
        ack.set_message("peerlist is empty");
    }
	nodelist.insert(nodelist.end(), tmp.begin(), tmp.end());
	nodelist.push_back(selfNode);

    for (auto &node : nodelist)
	{
        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
        if(stakeTime < 0)
        {
            continue;
        }

        StakeNode* stakeNodePtr =  ack.add_list();
        stakeNodePtr->set_addr("0x"+node.address);
        stakeNodePtr->set_name(node.name);
        stakeNodePtr->set_height(node.height);
        stakeNodePtr->set_identity(node.identity);
        stakeNodePtr->set_logo(node.logo);
        stakeNodePtr->set_ip(std::string(IpPort::IpSz(node.publicIp)) );
        stakeNodePtr->set_version(node.ver);
	}
    ack.set_code(0);
    ack.set_message("success");

    return 0;
}

std::map<int32_t, std::string> getAllStakeNodeListRequestCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get GetAllStakeNode List Success"), 
												std::make_pair(-1, "getStakeAddr failed !"), 
												std::make_pair(-12, "getStakeAddrUtxo failed !"), 
												std::make_pair(-21, "No failure of the transaction."),
                                                std::make_pair(-24, "GetBonusAddrDelegateUtxos failed !"),
                                                std::make_pair(-25, "getTransactionByHash failed !"),
												};

	return errInfo;												
}
int handleGetAllStakeNodeListRequest(const std::shared_ptr<GetAllStakeNodeListReq>& req, const MsgData & msgData)
{
    auto errInfo = getAllStakeNodeListRequestCode();
    GetAllStakeNodeListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        returnAcknowledgmentCode<GetAllStakeNodeListAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = stakeNodeListRequestImpl(req, ack);
    if (ret != 0)
    {
        return ret -= 100;
    }

    return 0;
}

/*************************************Get a list of signatures*************************************/
int GetSignCountListRequestImpl(const std::shared_ptr<GetSignCountListReq>& req, GetSignCountListAck & ack)
{
    ack.set_version(global::GetVersion());

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if (!isValidAddress(defaultAddr))
    {
        return -1;
    }

    std::map<std::string, double> addr_percent;
    std::unordered_map<std::string, uint64_t> addrSignCount;
    uint64_t curTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    ca_algorithm::fetchAbnormalSignAddrListByPeriod(curTime, addr_percent, addrSignCount);
    for (auto & item : addrSignCount)
    {
        SignCount* Sign_list =  ack.add_list();
        Sign_list->set_addr(item.first);
        Sign_list->set_count(item.second);
    }

    ack.set_code(0);
    ack.set_message("success");
    return 0;
}

std::map<int32_t, std::string> getSignCountListRequestCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get SignCountList List Success"), 
												std::make_pair(-1, "defaultAddr is invalid !"), 
												std::make_pair(-2, "getBonusAddr failed !"), 
												std::make_pair(-3, "getDelegatingAddrByBonusAddr failed !"),
                                                std::make_pair(-4, "BonusdAddrs < 1 || BonusdAddrs > 999"),
                                                std::make_pair(-5, "getBonusAddrDelegatingAddrUtxoByBonusAddr failed !"),
                                                std::make_pair(-6, "getTransactionByHash failed !"),
                                                std::make_pair(-7, "Parse tx failed !"),
                                                std::make_pair(-8, "Total amount delegatinged < 5000 !"),
                                                std::make_pair(-9, "GetSignUtxoByAddr failed !"),
												};

	return errInfo;												
}
int handleGetSignCountListRequest(const std::shared_ptr<GetSignCountListReq>& req, const MsgData & msgData)
{
    auto errInfo = getSignCountListRequestCode();
    GetSignCountListAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        returnAcknowledgmentCode<GetSignCountListAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = GetSignCountListRequestImpl(req, ack);
    if (ret != 0)
    {
        return ret -= 10;
    }

    return 0;
}

/*************************************Calculate the commission*************************************/
int getHeightRequestImplementation(const std::shared_ptr<GetHeightReq>& req, GetHeightAck & ack)
{
    ack.set_version(global::GetVersion());
    DBReader dbReader;
    uint64_t height = 0;
    if(dbReader.getBlockTop(height) != DBStatus::DB_SUCCESS)
    {
        return -1;
    }
    ack.set_height(height);
    
    return 0;
}

std::map<int32_t, std::string> getHeightRequestCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(    0, " GetHeight Success "),
                                                std::make_pair(   -1, " Get Block Height fail")
												};

	return errInfo;												
}
int getHeightRequest(const std::shared_ptr<GetHeightReq>& req, const MsgData & msgData)
{
    auto errInfo = getHeightRequestCode();
    GetHeightAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        returnAcknowledgmentCode<GetHeightAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = getHeightRequestImplementation(req, ack);
    if (ret != 0)
    {
        return ret -= 100;
    }

    return 0;
}

/*************************************Check the current claim amount*************************************/
int GetBonusListRequestImpl(const std::shared_ptr<GetBonusListReq> & req, GetBonusListAck & ack)
{
    ack.set_version(global::GetVersion());

    std::string addr = req->bonusaddr();
    if(addr.size() == 0)
    {
        return -1;
    } 

    if (!isValidAddress(addr))
    {
        return -2;
    }

    ack.set_bonusaddr(addr);
    uint64_t curTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    std::map<std::string, uint64_t> values;
    int ret = ca_algorithm::CalcBonusValue(curTime, addr, values);
    if (ret != 0)
    {
        ERRORLOG("CalcBonusValue RET : {}", ret);
        return -3;
    }

    for (auto & bonus : values)
    {
        BonusItem * item = ack.add_list();
        item->set_addr(bonus.first);
        item->set_value(bonus.second);
    }

    ack.set_code(0);
    ack.set_message("success");

    return 0;
}

std::map<int32_t, std::string> getBonusListRequestCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get Stake List Success !"), 
                                                std::make_pair(-1, "addr is empty !"),
                                                std::make_pair(-2, "addr is invalid !"),
                                                std::make_pair(-11, "addr is AbnormalSignAddr !"),
                                                std::make_pair(-111, "getDelegatingAddrByBonusAddr failed !"),
                                                std::make_pair(-112, "getBonusAddrDelegatingAddrUtxoByBonusAddr failed!"),
                                                std::make_pair(-113, "getTransactionByHash failed !"),
                                                std::make_pair(-114, "Parse Transaction failed !"),
                                                std::make_pair(-115, "GetDelegatingAndUndelegatingUtxoByAddr failed !"),
                                                std::make_pair(-116, "getTransactionByHash failed !"),
                                                std::make_pair(-117, "Parse Transaction failed !"),
                                                std::make_pair(-118, "Unknown transaction type !"),
                                                std::make_pair(-119, "mpDelegatingAddr2Amount is empty !"),
                                                std::make_pair(-212, "GetBonusUtxo failed !"),
                                                std::make_pair(-213, "getTransactionByHash failed !"),
                                                std::make_pair(-214, "Parse Transaction failed !"),
                                                std::make_pair(-311, "getTotalDelegatingAmount failed !"),
                                                std::make_pair(-312, "GetAllDelegatingUtxo failed !"),
                                                std::make_pair(-313, "getTransactionByHash failed !"),
                                                std::make_pair(-314, "Parse Transaction failed !"),

											};

	return errInfo;												
}

int processBonusListRequest(const std::shared_ptr<GetBonusListReq>& req, const MsgData & msgData)
{
    auto errInfo = getBonusListRequestCode();
    GetBonusListAck ack;
    int ret = 0;
    ON_SCOPE_EXIT{
        returnAcknowledgmentCode<GetBonusListAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return -1;
	}

    ret = GetBonusListRequestImpl(req, ack);
    if (ret != 0)
    {
        ERRORLOG("GetBonusListRequestImpl ret : {}", ret);
        return -2;
    }

    return 0;
}

std::map<int32_t, std::string> GetReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get  List Success"), 
												std::make_pair(-1, "version unCompatible"), 
												};
	return errInfo;												
}

int sendConfirmationTransactionRequest(const std::shared_ptr<ConfirmTransactionReq>& msg,   ConfirmTransactionAck & ack)
{
    auto getRandomNumbers = [&](int limit) -> std::vector<int> {
        std::random_device seed;
        std::ranlux48 engine(seed());
        std::uniform_int_distribution<int> u(0, limit - 1);
        
        std::set<int> uniqueNumbers;
        while (uniqueNumbers.size() < 10) {
            int randomNum = u(engine);
            uniqueNumbers.insert(randomNum);
        }
        
        std::vector<int> result(uniqueNumbers.begin(), uniqueNumbers.end());
        return result;
    };

    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    auto nodelistsize = nodelist.size();
    if(nodelistsize == 0)
    {
        ERRORLOG("Nodelist Size is empty");
        ack.set_message("Nodelist size is empty");
        return -1;
    }
    DEBUGLOG("Nodelist Size {}",nodelistsize);
    
    std::vector<Node> filterHeightNodeList;
    for(auto &item : nodelist)
    {
        if(item.height >= msg->height())
        {
            filterHeightNodeList.push_back(item);
        }
    }
    auto send_num = filterHeightNodeList.size();
    if(send_num == 0)
    {
        ERRORLOG("filterHeightNodeList is empty");
        ack.set_message("filterHeightNodeList size is empty");
        return -2;
    }
    DEBUGLOG("filterHeightNodeList {}",send_num);
    ack.set_send_size(send_num);
    //send_size
    std::string msgId;
    std::map<std::string, uint32_t> successHash;
    if (!dataMgrPtr.CreateWait(10, send_num * 0.8, msgId))
    {
        ERRORLOG("sendConfirmationTransactionRequest CreateWait is error");
        ack.set_message("CreateWait error");
        return -3;
    }

    CheckTxReq req;
    req.set_version(global::GetVersion());
    req.set_msg_id(msgId);
    for(auto & hash : msg->txhash())
    {
        req.add_txhash(hash);
        successHash.insert(std::make_pair(hash, 0));
    }

    auto resultVector = getRandomNumbers(filterHeightNodeList.size());
    int index = 0;
    std::vector<std::string> resourceNodeRequired;
    for (auto &node : filterHeightNodeList)
    {   
        if(std::find(resultVector.begin(), resultVector.end(), index++) != resultVector.end())
        {
            req.set_isresponse(1);
            resourceNodeRequired.push_back(node.address);
        }else{
            req.set_isresponse(0);
        }
        
        if(!dataMgrPtr.AddResNode(msgId, node.address))
        {
            ERRORLOG("sendConfirmationTransactionRequest AddResNode is error");
            ack.set_message(" AddResNode error");
            return -4;
        }
        NetSendMessage<CheckTxReq>(node.address, req, net_com::Compress::kCompressDisabled, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    }

    std::vector<std::string> ret_datas;
    if (!dataMgrPtr.WaitData(msgId, ret_datas))
    {
        ERRORLOG("sendConfirmationTransactionRequest WaitData is error");
        ack.set_message(" WaitData error");
        return -5;
    }

    CheckTxAck copyAck;
    std::multimap<std::string, int> tx_flag_hashes;
    std::vector<std::string> txRawVector;

    auto findMostFrequentElement = [](const std::vector<std::string>& vec) {
        std::unordered_map<std::string, int> occurrenceMap;

        for (const std::string& element : vec) {
            occurrenceMap[element]++;
        }

        std::string mostFrequentElement;
        int maxOccurrences = 0;

        for (const auto& entry : occurrenceMap) {
            if (entry.second > maxOccurrences) {
                mostFrequentElement = entry.first;
                maxOccurrences = entry.second;
            }
        }

        return std::make_pair(mostFrequentElement, maxOccurrences);
    };

    for (auto &ret_data : ret_datas)
    {
        copyAck.Clear();
        if (!copyAck.ParseFromString(ret_data))
        {
            continue;
        }
        
        for(auto & flag_hash : copyAck.flaghash())
        {
            tx_flag_hashes.insert(std::make_pair(flag_hash.hash(),flag_hash.flag()));

            if(std::find(resourceNodeRequired.begin(),resourceNodeRequired.end(),copyAck.addr()) != resourceNodeRequired.end() && flag_hash.flag() == 1)
            {
                txRawVector.push_back(copyAck.tx());
            }
        }
    }
    
    for(auto & item : tx_flag_hashes)
    {
        for(auto & success : successHash)
        {
            if(item.second == 1 && item.first == success.first)
            {
                success.second ++;
                DEBUGLOG("success.second size = {}", success.second);
            }
        }
    }

    auto commonSize = findMostFrequentElement(txRawVector);
    ack.set_tx(commonSize.first);
    ack.set_version(global::GetVersion());
    ack.set_time(msg->time());

    for(auto & success : successHash)
    {
        SuccessRate * alsuc = ack.add_percentage();

        alsuc->set_hash(success.first);
        double rate = (double)success.second / (double)(ret_datas.size());
        DEBUGLOG("sendConfirmationTransactionRequest = {} , == {}, rate = ", success.second, send_num, rate);
        alsuc->set_rate(rate);
    }
    ack.set_received_size(ret_datas.size());
    return 0;
}
 
std::map<int32_t, std::string> getChainRequestId()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "Get  List Success"), 
												std::make_pair(-1, "version unCompatible"), 
												};
	return errInfo;												
}

int confirmTransactionRequestHandler(const std::shared_ptr<ConfirmTransactionReq>& req, const MsgData & msgData)
{
    auto errInfo = getChainRequestId();
    ConfirmTransactionAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        returnAcknowledgmentCode<ConfirmTransactionAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
	{
		return ret = -1;
	}

    ret = sendConfirmationTransactionRequest(req, ack);
    return ret;
}

int GetRestdelegateAmountReqImpl(const std::shared_ptr<GetRestDelegateAmountReq>& msg,  GetRestDelegateAmountAck & ack)
{
    // The node to be delegatinged can only be delegatinged by 999 people at most
    uint64_t delegateAmount = 0;
    DBReader dbReader;
    std::vector<std::string> addresses;
    auto status = dbReader.getDelegatingAddrByBonusAddr(msg->addr(), addresses);
    if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
    {
        ERRORLOG("Get delegating addrs by node failed!" );
        return -1;
    }
    if (addresses.size() + 1 > 999)
    {
        ERRORLOG("The account number to be delegatinged have been delegatinged by 999 people!" );
        return -2;
    }

    // The node to be delegatinged can only be be delegatinged 100000 at most
    uint64_t sumdelegateAmount = 0;
    for (auto &address : addresses)
    {
        std::vector<std::string> utxos;
        if (dbReader.getBonusAddrDelegatingAddrUtxoByBonusAddr(msg->addr(), address, utxos) != DBStatus::DB_SUCCESS)
        {
            ERRORLOG("getBonusAddrDelegatingAddrUtxoByBonusAddr failed!");
            return -3;
        }

        for (const auto &utxo : utxos)
        {
            std::string strTx;
            if (dbReader.getTransactionByHash(utxo, strTx) != DBStatus::DB_SUCCESS)
            {
                ERRORLOG("getTransactionByHash failed!");
                return -4;
            }

            CTransaction tx;
            if (!tx.ParseFromString(strTx))
            {
                ERRORLOG("Failed to parse transaction body!");
                return -5;
            }
            for(auto& utxo:tx.utxos())
            {
                for (auto &vout : utxo.vout())
                {
                    if (vout.addr() == global::ca::kVirtualDelegatingAddr)
                    {
                        sumdelegateAmount += vout.value();
                        break;
                    }
                }
            }
        }
    }
    delegateAmount = (3500000ull * global::ca::kDecimalNum) - sumdelegateAmount;
    ack.set_addr(msg->addr());
    ack.set_amount(delegateAmount);

    return 0;
}

std::map<int32_t, std::string> GetRestdelegateAmountReqCode()
{
	std::map<int32_t, std::string> errInfo = {  std::make_pair(0, "success"), 
												std::make_pair(-1, "Get delegating addrs by node failed!"), 
												std::make_pair(-2, "The account number to be delegatinged have been delegatinged by 999 people!"), 
												std::make_pair(-3, "getBonusAddrDelegatingAddrUtxoByBonusAddr failed!"), 
												std::make_pair(-4, "getTransactionByHash failed!"), 
												std::make_pair(-5, "Failed to parse transaction body!")
												};
	return errInfo;												
}

int HandleGetRestdelegateAmountReq(const std::shared_ptr<GetRestDelegateAmountReq>& req, const MsgData & msgData)
{
    auto errInfo = GetRestdelegateAmountReqCode();
    GetRestDelegateAmountAck ack;
    int ret = 0;

    ON_SCOPE_EXIT{
        returnAcknowledgmentCode<GetRestDelegateAmountAck>(msgData, errInfo, ack, ret);
    };

    if( 0 != Util::IsVersionCompatible( req->version() ) )
    {
        return ret = -1;
    }

    ret = GetRestdelegateAmountReqImpl(req,ack); 

    return ret;
}

void RegisterInterface()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetBlockReq>(handleBlockFetchRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetBalanceReq>(getBalanceRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetNodeInfoReq>(handleGetNodeInfoRequest);
	MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetStakeListReq>(handleGetStakeListRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetDelegateListReq>(HandleGetDelegateListReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetUtxoReq>(handleGetUtxoRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetAllDelegateAddressReq>(HandleGetAllDelegatingAddressReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetAllStakeNodeListReq>(handleGetAllStakeNodeListRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetSignCountListReq>(handleGetSignCountListRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetHeightReq>(getHeightRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetBonusListReq>(processBonusListRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetSDKReq>(getSdkAllNeedsRequest);
	MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<ConfirmTransactionReq>(confirmTransactionRequestHandler);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<CheckVinReq>(processCheckVinRequest);
	MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<CheckVinAck>(checkVinAcknowledge);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<GetRestDelegateAmountReq>(HandleGetRestdelegateAmountReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<BlockStatus>(blockStatusMessageHandler); //retransmit

    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<CheckContractPreHashReq>(processCheckContractPreHashRequest); // retransmit
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerCallback<CheckContractPreHashAck>(handleContractPreHashAck); // retransmit

    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<FastSyncGetHashReq>(processFastSyncGetHashRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<FastSyncGetHashAck>(handleFastSyncHashAcknowledge);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<FastSyncGetBlockReq>(handleFastSyncBlockRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<FastSyncGetBlockAck>(handleFastSyncBlockAcknowledge);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncGetSumHashReq>(processSyncSumHashRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncGetSumHashAck>(syncGetSumHashAcknowledge);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncGetHeightHashReq>(handleSyncGetHeightHashRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncGetHeightHashAck>(processSyncHeightHashAcknowledgement);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncGetBlockHeightAndHashReq>(syncBlockHeightAndHashRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncGetBlockHeightAndHashAck>(syncBlockHeightAndHashAck);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncGetBlockReq>(handleSynchronizationBlockRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncGetBlockAck>(handleSyncGetBlockAcknowledge);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncFromZeroGetSumHashReq>(syncGetSumHashRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncFromZeroGetSumHashAck>(zeroSyncSumHashAcknowledge);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncFromZeroGetBlockReq>(handleSyncGetBlockRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncFromZeroGetBlockAck>(handleZeroSyncBlockAcknowledge);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncNodeHashReq>(syncNodeHashRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SyncNodeHashAck>(processSyncNodeHashAcknowledgment);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<GetBlockByUtxoReq>(handleBlockByUtxoRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<GetBlockByUtxoAck>(handleBlockByUtxoAcknowledgment);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<GetBlockByHashReq>(blockByHashRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<GetBlockByHashAck>(handleBlockByHashAcknowledgment);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SeekPreHashByHightReq>(handleSeekGetPreHashRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<SeekPreHashByHightAck>(preHashAcknowledgment);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<GetCheckSumHashReq>(handleGetCheckSumHashRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSyncBlockCallback<GetCheckSumHashAck>(handleChecksumHashAcknowledge);

    // PCEnd correlation
    MagicSingleton<ProtobufDispatcher>::GetInstance()->TxRegisterCallback<TxMsgReq>(HandleTx); // PCEnd transaction flow
    MagicSingleton<ProtobufDispatcher>::GetInstance()->TxRegisterCallback<ContractTxMsgReq>(contractTransactionRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->TxRegisterCallback<ContractPackagerMsg>(handleContractPackagerMessage);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->TxRegisterCallback<ContractDependencyBroadcastMsg>(HandleContractDependencyBroadcastMsg); 
    MagicSingleton<ProtobufDispatcher>::GetInstance()->TxRegisterCallback<VRFConsensusInfo>(HandleVRFConsensusInfoReq);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerSaveBlockCallback<BuildBlockBroadcastMsg>(handleBuildBlockBroadcastMessage); // Building block broadcasting

    MagicSingleton<ProtobufDispatcher>::GetInstance()->blockRegisterCallback<BlockMsg>(HandleBlock);      // PCEnd transaction flow
    MagicSingleton<ProtobufDispatcher>::GetInstance()->blockRegisterCallback<SendReconnectNodeReq>(HandleSendReconnectNodeReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->blockRegisterCallback<newSeekContractPreHashReq>(handleSeekContractPreHashRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->blockRegisterCallback<newSeekContractPreHashAck>(handleSeekContractPreHashAcknowledgement);

}

