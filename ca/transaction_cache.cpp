#include <memory>
#include <unordered_map>
#include <future>
#include <chrono>


#include "ca/checker.h"
#include "ca/contract.h"
#include "ca/txhelper.h"
#include "ca/algorithm.h"
#include "ca/transaction.h"
#include "ca/block_helper.h"
#include "ca/transaction_cache.h"
#include "ca/failed_transaction_cache.h"
#include "ca/sync_block.h"

#include <nlohmann/json.hpp>
#include "utils/console.h"
#include "utils/tmp_log.h"
#include "utils/time_util.h"
#include "utils/time_util.h"
#include "utils/bench_mark.h"
#include "utils/contract_utils.h"
#include "utils/magic_singleton.h"
#include "utils/account_manager.h"

#include "db/db_api.h"
#include "net/unregister_node.h"

#include "common/time_report.h"
#include "common/global_data.h"
#include "ca/evm/evm_manager.h"

class contractDataContainer;

const int TransactionCache::BUILD_INTERVAL = 3 * 1000;
const time_t TransactionCache::_kTxExpireInterval  = 10;
const int TransactionCache::BUILD_THRESHOLD = 1000000;


int CreateBlock(const std::list<CTransaction>& txs, const uint64_t& blockHeight, CBlock& cblock)
{
	cblock.Clear();

	// Fill version
	cblock.set_version(global::ca::kCurrentBlockVersion);

	// Fill height
	uint64_t prevBlockHeight = blockHeight;
	cblock.set_height(prevBlockHeight + 1);

    nlohmann::json storage;
    bool isContractualBlock = false;

    uint64_t blockTime = 0;
    packDispatch packdis;
	// Fill tx
	for(auto& tx : txs)
	{
		// Add major transaction
		CTransaction * majorTx = cblock.add_txs();
		*majorTx = tx;
		auto& txHash = tx.hash();
        //DEBUGLOG("txHash:{}, txTime:{}", txHash, tx.time() / 1000000);
        auto txType = (global::ca::TxType)tx.txtype();
        if(tx.time() > blockTime)
        {
            blockTime = tx.time();
        }
        if (txType == global::ca::TxType::TX_TYPE_INVOKE_CONTRACT || txType == global::ca::TxType::kTransactionTypeDeploy)
        {
            isContractualBlock = true;
            nlohmann::json txStorage;
            if (MagicSingleton<TransactionCache>::GetInstance()->get_contract_info_cache(txHash, txStorage) != 0)
            {
                ERRORLOG("can't find storage of tx {}", txHash);
                return -1;
            }

            std::set<std::string> dirtyContractList_;
            if(!MagicSingleton<TransactionCache>::GetInstance()->get_dirty_contract_map(tx.hash(), dirtyContractList_))
            {
                ERRORLOG("get_dirty_contract_map fail!!! txHash:{}", tx.hash());
                return -2;
            }
            txStorage["dependentCTx"] = dirtyContractList_;

            storage[txHash] = txStorage;
        }
	}
    cblock.set_time(blockTime);

    cblock.set_data(storage.dump());
    // Fill preblockhash
    uint64_t seekPrehashTimeValue = 0;
    std::future_status status;
    auto futurePreHash = MagicSingleton<BlockStorage>::GetInstance()->GetPrehash(prevBlockHeight);
    if(!futurePreHash.valid())
    {
        ERRORLOG("futurePreHash invalid,hight:{}",prevBlockHeight);
        return -2;
    }
    status = futurePreHash.wait_for(std::chrono::seconds(6));
    if (status == std::future_status::timeout) 
    {
        ERRORLOG("seek prehash timeout, hight:{}",prevBlockHeight);
        return -3;
    }
    else if(status == std::future_status::ready) 
    {
        std::string preBlockHash = futurePreHash.get().first;
        if(preBlockHash.empty())
        {
            ERRORLOG("seek prehash <fail>!!!,hight:{},prehash:{}",prevBlockHeight, preBlockHash);
            return -4;
        }
        DEBUGLOG("seek prehash <success>!!!,hight:{},prehash:{},blockHeight:{}",prevBlockHeight, preBlockHash, blockHeight);
        seekPrehashTimeValue = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        cblock.set_prevhash(preBlockHash);
    }
    
	// Fill merkleroot
	cblock.set_merkleroot(ca_algorithm::calculateBlockMerkle(cblock));
	// Fill hash
	cblock.set_hash(Getsha256hash(cblock.SerializeAsString()));
    DEBUGLOG("blockHash:{}, \n storage:{}", cblock.hash().substr(0,6), storage.dump(4));
    DEBUGLOG("block hash = {} set time blockTime:{}",cblock.hash(), blockTime);
	return 0;
}

int BuildBlock(const std::list<CTransaction>& txs, const uint64_t& blockHeight, bool build_first, bool isConTractTx = false)
{
	if(txs.empty())
	{
		ERRORLOG("Txs is empty!");
		return -1;
	}

	CBlock cblock;
	int ret = CreateBlock(txs, blockHeight, cblock);
    if(ret != 0)
    {
        if(ret == -3 || ret == -4 || ret == -5)
        {
            MagicSingleton<BlockStorage>::GetInstance()->ForceCommitSeekJob(cblock.height() - 1);
        }
        auto tx_sum = cblock.txs_size();
        ERRORLOG("Create block failed! : {},  Total number of transactions : {} ", ret, tx_sum);
		return ret - 100;
    }
	std::string serBlock = cblock.SerializeAsString();
	ca_algorithm::PrintBlock(cblock);

    BlockMsg blockmsg;
    blockmsg.set_version(global::GetVersion());
    blockmsg.set_time(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
    blockmsg.set_block(serBlock);

    auto msg = std::make_shared<BlockMsg>(blockmsg);
	ret = handleBlock(msg);
    if(ret != 0)
    {
        ERRORLOG("handleBlock failed The error code is {} ,blockHash:{}",ret, cblock.hash().substr(0,6));
        CBlock cblock;
	    if (!cblock.ParseFromString(msg->block()))
	    {
		    ERRORLOG("fail to serialization!!");
		    return -5;
	    }
        return -6;
    }
    for(const auto& tx : cblock.txs())
    {
        DEBUGLOG("blockHash:{}, txHash:{}", cblock.hash().substr(0,6), tx.hash().substr(0,6));
    }
    DEBUGLOG("block successfully packaged, blockHash:{}", cblock.hash().substr(0,6));
	return 0;
}

TransactionCache::TransactionCache()
{
    _buildTimer.AsyncLoop(
        BUILD_INTERVAL, 
        [=](){ _blockBuilder.notify_one(); }
        );
}

uint64_t TransactionCache::getBlockCount()
{
    std::unique_lock<std::mutex> locker(transactionCacheMutex);
    return _transactionCache.rbegin()->get_tx_utxo_height();
}

int TransactionCache::AddCache(CTransaction& transaction, const std::shared_ptr<TxMsgReq>& msg)
{
    auto txType = (global::ca::TxType)transaction.txtype();
    bool isContractExecution = txType == global::ca::TxType::TX_TYPE_INVOKE_CONTRACT || txType == global::ca::TxType::kTransactionTypeDeploy;
    if (isContractExecution)
    {
        std::unique_lock<std::mutex> locker(contractCacheMutex);
        if(Checker::CheckConflict(transaction, _contractCache))
        {
            DEBUGLOG("DoubleSpentTransactions, txHash:{}", transaction.hash());
            return -1;
        }
        _contractCache.push_back({transaction, msg->txmsginfo().nodeheight(), false});
    }
    else
    {
        std::unique_lock<std::mutex> locker(transactionCacheMutex);
        if(Checker::CheckConflict(transaction, _transactionCache))
        {
            DEBUGLOG("DoubleSpentTransactions, txHash:{}", transaction.hash());
            return -2;
        }

        if((global::ca::TxType)transaction.txtype() == global::ca::TxType::kTXTypeFund){
            _transactionCache.push_back({*msg, transaction, msg->txmsginfo().nodeheight()});
        }else{
            _transactionCache.push_back({*msg, transaction, msg->txmsginfo().txutxoheight()});
        }

        if (_transactionCache.size() >= BUILD_THRESHOLD)
        {
            _blockBuilder.notify_one();
        }
    }
    return 0;
}

bool TransactionCache::Process()
{
    transactionCacheBuildThread = std::thread(std::bind(&TransactionCache::processTransactionCacheFunc, this));
    transactionCacheBuildThread.detach();
    return true;
}

void TransactionCache::Stop(){
    _threadRun=false;
}

int TransactionCache::get_build_block_height(std::vector<TransactionEntity>& txcache)
{
    int max_utxo_height = 0;
    for(const auto& item : txcache)
    {
        auto txUtxoBlockHeight = item.get_tx_utxo_height();
        if(max_utxo_height < txUtxoBlockHeight)
        {
            max_utxo_height = txUtxoBlockHeight;
        }
    }

    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::map<std::string, uint64_t> satisfiedAddresses;
    for(auto & node : nodelist)
    {
        //Verification of Delegating and pledge
        int ret = VerifyBonusAddr(node.address);
        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
        if (stakeTime > 0 && ret == 0)
        {
            satisfiedAddresses[node.address] = node.height;
        }
    }

    if (satisfiedAddresses.size() < global::ca::MIN_SYNC_QUAL_NODES && (max_utxo_height < global::ca::MIN_UNSTAKE_HEIGHT))
	{
		for(auto & node : nodelist)
        {
            if(satisfiedAddresses.find(node.address) == satisfiedAddresses.end())
            {
                satisfiedAddresses[node.address] = node.height;
            }
        }
	}
	else if(satisfiedAddresses.size() < global::ca::MIN_SYNC_QUAL_NODES)
	{
		DEBUGLOG("Insufficient qualified signature nodes satisfiedAddresses.size:{}", satisfiedAddresses.size());
		return -1;
	}


    std::map<uint64_t, uint64_t> heightCount;
    uint64_t heightMatchCount = 0;
    std::map<uint64_t, uint64_t> heightFrequency;

    for(auto &it : satisfiedAddresses)
    {
        if(it.second >= max_utxo_height)
        {        
            heightMatchCount++;
        }
        heightFrequency[it.second]++;
    }
    
    uint64_t cumulativeCount = 0;
    for(auto it = heightFrequency.rbegin(); it != heightFrequency.rend(); it++)
    {
        cumulativeCount += it->second; // Add the frequency of this height to the cumulative count
        heightCount[it->first] = cumulativeCount; 
        DEBUGLOG("heightFrequency, hegiht:{}, sum:{}", it->first, it->second);
    }

    if(heightMatchCount < satisfiedAddresses.size() * 0.75)
    {
        DEBUGLOG("heightMatchCount:{}, satisfiedAddresses.size:{}, {}, max_utxo_height:{}", heightMatchCount, satisfiedAddresses.size(), satisfiedAddresses.size() * 0.75, max_utxo_height);
        return -2;
    }

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
    }

    if(top >= max_utxo_height)
    {
        auto it = heightCount.find(top);
        DEBUGLOG("top:{}, it->second:{}, satisfiedAddresses.size:{}", top, it->second, satisfiedAddresses.size());
        if(it != heightCount.end() && it->second >= satisfiedAddresses.size() * 0.75)
        {
            DEBUGLOG("top top top top top");
            return top;
        }
    }



    for(auto item = heightCount.rbegin(); item != heightCount.rend(); item++)
    {
        DEBUGLOG("item.first:{}, max_utxo_height:{}, {}", item->first, max_utxo_height, satisfiedAddresses.size() * 0.75);
        if(item->first < max_utxo_height)
        {
            return -3;
        }
        if(item->second >= satisfiedAddresses.size() * 0.75)
        {
            return item->first;
        }
    }
    return -4;
}

void TransactionCache::processTransactionCacheFunc()
{
    while (_threadRun)
    {
        std::unique_lock<std::mutex> locker(transactionCacheMutex);
        _blockBuilder.wait(locker);

        std::list<CTransaction> buildTxs;
        if(_transactionCache.empty())
        {
            continue;
        }

        clearBuiltTransactions();
        int buildHeight = get_build_block_height(_transactionCache);
        if(buildHeight < 0)
        {
            _transactionCache.clear();
            ERRORLOG("get_build_block_height fail!!! ret:{}", buildHeight);
            continue;
        }
        MagicSingleton<BlockStorage>::GetInstance()->commitLookupTask(buildHeight);

        std::map<std::string, std::future<int>> taskResults;
        for(auto& txs : _transactionCache)
        {
            buildTxs.push_back(txs.GetTx());
        }

        auto ret = BuildBlock(buildTxs, buildHeight, false);
        if(ret != 0)
        {
            ERRORLOG("{} build block fail", ret);
            if(ret == -103 || ret == -104 || ret == -105)
            {
                continue;
            }
            
            _transactionCache.clear();
            std::cout << "block packaging fail" << std::endl;
            continue;
        }
        std::cout << "block successfully packaged" << std::endl;
        _transactionCache.clear();

        locker.unlock();
    }
}

void TransactionCache::ProcessContract(int64_t topTransactionHeight_)
{
    std::scoped_lock locker(contractCacheMutex, contractInfoCacheMutex, dirtyContractMapMutex);
    std::list<CTransaction> buildTxs;

    for(const auto& txEntity : _contractCache)
    {
        buildTxs.push_back(txEntity.GetTransaction());
    }
    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("getBlockTop error!");
        std::cout << "block packaging fail" << std::endl;
        return;
    }
    if (top > topTransactionHeight_)
    {
        MagicSingleton<BlockStorage>::GetInstance()->commitLookupTask(top);
        topTransactionHeight_ = top;
        DEBUGLOG("top:{} > topTransactionHeight_:{}", top, topTransactionHeight_);
    }
    if (buildTxs.empty())
    {
        DEBUGLOG("buildTxs.empty()");
        return;
    }

    ON_SCOPE_EXIT{
        removeExpiredFromDirtyContractMap();
        _contractCache.clear();
        contractInfoCache.clear();
    };

    std::list<std::pair<std::string, std::string>> contractTxPrevHashList;
    if(fetchContractTxPreHash(buildTxs,contractTxPrevHashList) != 0)
    {
        ERRORLOG("fetchContractTxPreHash fail");
        return;
    }
    if(contractTxPrevHashList.empty())
    {
        DEBUGLOG("contractTxPrevHashList empty");
    }
    else
    {
        auto ret = newSeekContractPreHash(contractTxPrevHashList);
        if ( ret != 0)
        {
            ERRORLOG("{} newSeekContractPreHash fail", ret);
            return;
        }
    }

    auto ret = BuildBlock(buildTxs, topTransactionHeight_, false,true);
    if(ret != 0)
    {
        ERRORLOG("{} build block fail", ret);
        std::cout << "block packaging fail" << std::endl;
    }
    else
    {
        std::cout << "block successfully packaged" << std::endl;
    }
    DEBUGLOG("FFF 555555555");
    return;
}

std::pair<int, std::string>
TransactionCache::executeContracts(const std::map<std::string, CTransaction> &dependentContractTxMap_,
                                    int64_t blockNumber)
{
    uint64_t StartTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    contractDataContainer contractDataStorage;
    std::map<std::string, std::string> contractPreHashCache_;
    for (auto iterPair : dependentContractTxMap_)
    {
        uint64_t StartTime1 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        auto& tx = iterPair.second;
        auto txType = (global::ca::TxType)tx.txtype();
        if ( (txType != global::ca::TxType::TX_TYPE_INVOKE_CONTRACT && txType != global::ca::TxType::kTransactionTypeDeploy)
            || addContractInfoCache(tx, contractPreHashCache_, &contractDataStorage, blockNumber + 1) != 0)
        {
            return {-1,tx.hash()};
        }
        uint64_t StartTime2 = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        DEBUGLOG("FFF executeContracts HHHHHHHHHH txHash:{}, time:{}", tx.hash().substr(0,6), (StartTime2 - StartTime1) / 1000000.0);
    }
    uint64_t EndTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    DEBUGLOG("FFF executeContracts HHHHHHHHHH txSize:{}, Time:{}",dependentContractTxMap_.size(), (EndTime - StartTime) / 1000000.0);
    return {0,""};
}

bool TransactionCache::verifyDirtyContractFlag(const std::string &transactionHash, const std::vector<std::string> &calledContract)
{
    auto found = dirtyContractMap.find(transactionHash);
    if (found == dirtyContractMap.end())
    {
        ERRORLOG("dirty contract not found hash: {}", transactionHash);
        return false;
    }
    std::set<std::string> calledContractContainer(calledContract.begin(), calledContract.end());
    std::vector<std::string> result;
    std::set_difference(calledContractContainer.begin(), calledContractContainer.end(),
                        found->second.second.begin(), found->second.second.end(),
                        std::back_inserter(result));
    if (!result.empty())
    {
        for (const auto& addr : calledContract)
        {
            ERRORLOG("executed {}", addr);
        }
        for (const auto& addr : found->second.second)
        {
            ERRORLOG("found {}", addr);
        }
        for (const auto& addr : result)
        {
            ERRORLOG("result {}", addr);
        }
        ERRORLOG("dirty contract doesn't match execute result, tx hash: {}", transactionHash);
        return false;
    }
    return true;
}

int TransactionCache::addContractInfoCache(const CTransaction &transaction,
                                            std::map<std::string, std::string> &contractPreHashCache_,
                                            contractDataContainer *contractDataStorage, int64_t blockNumber)
{
    auto txType = (global::ca::TxType)transaction.txtype();
    if (txType != global::ca::TxType::TX_TYPE_INVOKE_CONTRACT && txType != global::ca::TxType::kTransactionTypeDeploy)
    {
        return 0;
    }

    std::string contractOwnerEvmAddress;
    global::ca::VmType vmType;

    std::string code;
    std::string input;
    std::string deployerAddr;
    std::string destAddr;
    uint64_t contractTransfer;
    std::string contractAddress;
    std::string CallType = "";
    std::string transmitFlowMessage = "";
    try
    {
        nlohmann::json dataJson = nlohmann::json::parse(transaction.data());
        nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();

        if(txInfo.find(Evmone::contractSenderKeyName_) != txInfo.end())
        {
            contractOwnerEvmAddress = txInfo[Evmone::contractSenderKeyName_].get<std::string>();
        }
        vmType = txInfo[Evmone::contractVmKeyName].get<global::ca::VmType>();

        if (txType == global::ca::TxType::TX_TYPE_INVOKE_CONTRACT)
        {

            input = txInfo[Evmone::contract_input_key_name].get<std::string>();

            if (txInfo.find("CallType") != txInfo.end())
            {
                CallType = txInfo["CallType"].get<std::string>();
            }

            if(txInfo.find("transmitFlowMessage") != txInfo.end()){
                transmitFlowMessage = txInfo["transmitFlowMessage"].get<std::string>();
            }

            if(vmType == global::ca::VmType::EVM)
            {
                contractTransfer = txInfo[Evmone::contractTransferKeyLabel].get<uint64_t>();
                contractAddress = txInfo[Evmone::contractRecipientKeyNameStr].get<std::string>();
            }
        }
        else if (txType == global::ca::TxType::kTransactionTypeDeploy)
        {
            code = txInfo[Evmone::contract_input_key_name].get<std::string>();
            contractAddress = txInfo[Evmone::contractRecipientKeyNameStr].get<std::string>();
        }

    }
    catch (...)
    {
        ERRORLOG("json parse fail");
        return -2;
    }
              
    int64_t gasCost = 0;
    nlohmann::json txInfoJson;
    std::string expectedOutput;
    std::vector<std::string> calledContract;
    std::string coinbase = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();


    
    std::pair<std::string, std::string> gasTrade = {};
    if (!CallType.empty())
    {
        // gasTrade = {transaction.utxos(0).owner(0), global::ca::ASSET_TYPE_VOTE};
    }
    else if(!transmitFlowMessage.empty())
    {
        DEBUGLOG("transaction is transmitFlowMessage.");
        gasTrade = {transaction.utxos(0).owner(0), transaction.utxos(0).assettype()};
    }

    bool falg = false;
    for (const auto &utxo : transaction.utxos())
    {
        if (!falg && !CallType.empty() && utxo.vout_size() == 2 && utxo.vin_size() == 0 && transaction.utxos_size() == 1)
        {
            falg = true;
            continue;
        }

        if (utxo.gasutxo() != 1)
        {
            continue;
        }

        std::string currency = utxo.assettype();
        if (utxo.owner_size() != 0 && !currency.empty())
        {
            gasTrade = {utxo.owner(0), currency};
            DEBUGLOG("addContractInfoCache owner :{}, assetType :{}", utxo.owner(0), currency);
        }
    }


    std::string fromAddr;
    int ret = ca_algorithm::get_call_contract_from_addr(transaction,fromAddr);
    if (ret != 0)
    {
        ERRORLOG("get_call_contract_from_addr fail ret: {}", ret);
        return -1;
    }
    

    if(vmType == global::ca::VmType::EVM)
    {
        destAddr = contractOwnerEvmAddress;
        if(destAddr != fromAddr)
        {
            ERRORLOG("fromAddr {}  is not equal to detAddr {} ", fromAddr, destAddr);
            return -3;
        }
        

        EvmHost host(contractDataStorage);
        if(txType == global::ca::TxType::kTransactionTypeDeploy)
        {
            int ret = Evmone::VerifyContractAddress(fromAddr, contractAddress);
            if (ret < 0)
            {
                ERRORLOG("verify contract address fail, ret: {}", ret)
                return ret - 100;
            }
            DEBUGLOG("888Output: {}", code);

            host.code = evm_utils::StringTobytes(code);
            if (code.empty())
            {
                ERRORLOG("fail to convert contract code to hex format");
                return -21;
            }
            int64_t blockTimestamp = evmEnvironment::GetBlockTimestamp(transaction);
            if (blockTimestamp < 0)
            {
                return -5;
            }
            DEBUGLOG("777Output: {}", evmc::hex({host.code.data(), host.code.size()}) );
            int64_t blockPrevRandao = evmEnvironment::get_block_prev_randao(transaction);
            if (blockPrevRandao < 0)
            {
                return -6;
            }
            evmc_message message{};

            ret = Evmone::DeployContract(fromAddr, host, message,
                                         contractAddress, blockTimestamp, blockPrevRandao, blockNumber,gasTrade.second);

            if(ret != 0)
            {
                ERRORLOG("VM failed to deploy contract!, ret {}", ret);
                return ret - 100;
            }
            gasCost = host.gas_cost;
            expectedOutput = host.output;
            DEBUGLOG("111Output: {}", expectedOutput);

        }
        else if(txType == global::ca::TxType::TX_TYPE_INVOKE_CONTRACT)
        {
            evmc::bytes contractCode;
            if (evm_utils::GetContractCode(contractAddress, contractCode) != 0)
            {
                ERRORLOG("fail to get contract code");
                return -2;
            }
            
            host.code = contractCode;

            int64_t blockTimestamp = evmEnvironment::GetBlockTimestamp(transaction);
            if (blockTimestamp < 0)
            {
                return -5;
            }

            int64_t blockPrevRandao = evmEnvironment::get_block_prev_randao(transaction);
            if (blockPrevRandao < 0)
            {
                return -6;
            }
            evmc_message message{};
            int ret = Evmone::CallContract(fromAddr, input, host, contractTransfer, contractAddress,
                                       message, blockTimestamp, blockPrevRandao, blockNumber,gasTrade.second);
            if(ret != 0)
            {
                ERRORLOG("VM failed to call contract!, ret {}", ret);
                return ret - 200;
            }
            gasCost = host.gas_cost;
            expectedOutput = host.output;
            std::string validateOut;
            if (!CallType.empty())
            {
                ret = Evmone::ValidateMappingTransaction(input, contractAddress, contractOwnerEvmAddress, transaction,host,message,validateOut);
                if (ret != 0)
                {
                    ERRORLOG("ValidateMappingTransaction fail !!!,ret:{}", ret);
                    return -4;
                }
            }
        }

        int ret = Evmone::ContractInfoAdd(host, transaction.hash(), txType, transaction.version(), txInfoJson,
                                    contractPreHashCache_, true);
        if(ret != 0)
        {
            ERRORLOG("ContractInfoAdd fail ret: {}", ret);
            return -4;
        }

        Evmone::invokedContract(host, calledContract);
   
        if(host.contractDataStorage != nullptr) host.contractDataStorage->set(txInfoJson[Evmone::storageKeyForContract]);
        else return -7;
    }

    if (!verifyDirtyContractFlag(transaction.hash(), calledContract))
    {
        ERRORLOG("verifyDirtyContractFlag fail");
        return -7;
    }
    DEBUGLOG("999Output: {}", expectedOutput);
    txInfoJson[Evmone::contractOutputKey] = expectedOutput;
    DEBUGLOG("888Output: {}", txInfoJson.dump(4));
    txInfoJson[Evmone::contractBlockCoinbaseKeyName] = coinbase;
    addContractInfoToCache(transaction.hash(), txInfoJson, transaction.time());
    return 0;
}

void TransactionCache::addContractInfoToCache(const std::string& transactionHash, const nlohmann::json& txInfoJson, const uint64_t& txtime)
{
    std::unique_lock<std::shared_mutex> locker(contractInfoCacheMutex);
    contractInfoCache[transactionHash] = {txInfoJson, txtime};
    return;
}

int TransactionCache::get_contract_info_cache(const std::string& transactionHash, nlohmann::json& txInfoJson)
{
    auto found = contractInfoCache.find(transactionHash);
    if (found == contractInfoCache.end())
    {
        return -1;
    }

    txInfoJson = found->second.first;
    return 0;
}

std::string TransactionCache::get_contract_prev_block_hash()
{
    return preContractBlockHash;
}

bool TransactionCache::hasContractPackingPermission(const std::string& addr, uint64_t transactionHeight, uint64_t time)
{
    std::string packingAddr;
    if (calculatePackerByTime(time, packingAddr) != 0)
    {
        return false;
    }
    DEBUGLOG("time: {}, height: {}, packer {}", time, transactionHeight, packingAddr);
    return packingAddr == addr;
}

void TransactionCache::contractBlockNotification(const std::string& blockHash)
{
    if (preContractBlockHash.empty())
    {
        return;
    }
    if (blockHash == preContractBlockHash)
    {
        contractPreBlockWaiter.notify_one();
    }
}

std::string
TransactionCache::get_and_update_contract_pre_hash(const std::string &contractAddress, const std::string &transactionHash,
                                              std::map<std::string, std::string> &contractPreHashCache_)
{
    std::string prevTxHash;
    auto found = contractPreHashCache_.find(contractAddress);
    if (found == contractPreHashCache_.end())
    {
        DBReader dbReader;
        if (dbReader.getLatestUtxoByContractAddr(contractAddress, prevTxHash) != DBStatus::DB_SUCCESS)
        {
            ERRORLOG("GetLatestUtxo of ContractAddr {} fail", contractAddress);
            return "";
        }
        if (prevTxHash.empty())
        {
            return "";
        }
    }
    else
    {
        prevTxHash = found->second;
    }
    contractPreHashCache_[contractAddress] = transactionHash;

    return prevTxHash;
}

void TransactionCache::set_dirty_contract_map(const std::string& transactionHash, const std::set<std::string>& dirtyContract)
{
    std::unique_lock locker(dirtyContractMapMutex);
    uint64_t currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    dirtyContractMap[transactionHash]= {currentTime, dirtyContract};

}

bool TransactionCache::get_dirty_contract_map(const std::string& transactionHash, std::set<std::string>& dirtyContract)
{
    auto found = dirtyContractMap.find(transactionHash);
    if(found != dirtyContractMap.end())
    {
        dirtyContract = found->second.second;
        return true;
    }
    
    return false;
}

void TransactionCache::removeExpiredFromDirtyContractMap()
{
    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    for(auto iter = dirtyContractMap.begin(); iter != dirtyContractMap.end();)
    {
        if(nowTime >= iter->second.first + 60 * 1000000ull)
        {
            DEBUGLOG("remove txHash:{}", iter->first);
            dirtyContractMap.erase(iter++);
        }
        else
        {
            ++iter;
        }
    }
}

void TransactionCache::addBuildTransaction(const CTransaction &transaction)
{
    std::unique_lock<std::mutex> locker(buildTxsMutex);
    buildTxs.push_back(transaction);
}
const std::list<CTransaction>& TransactionCache::getBuildTransactions()
{
    std::unique_lock<std::mutex> locker(buildTxsMutex);
    return buildTxs;
}
void TransactionCache::clearBuiltTransactions()
{
    std::unique_lock<std::mutex> locker(buildTxsMutex);
    buildTxs.clear();
}

int get_contract_prehash_find_node(uint32_t num, uint64_t nodeSelfHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &node_ids_to_send)
{
    int ret = 0;
    if ((ret = SyncBlock::getSyncNodeSimplified(num, nodeSelfHeight, pledgeAddr, node_ids_to_send)) != 0)
    {
        ERRORLOG("get seek node fail, ret:{}", ret);
        return -1;
    }
    return 0;
}


int TransactionCache::handleContractPackagerMessage(const std::shared_ptr<ContractPackagerMsg> &msg, const MsgData &msgdata)
{
    std::unique_lock<std::mutex> locker(_threadMutex);
    DEBUGLOG("111111111111 HHHHHHHHHH");
    auto cSign = msg->sign();
    auto pub = cSign.pub();
    auto signature = cSign.sign();

    ContractPackagerMsg cp_msg = *msg;
    cp_msg.clear_sign();
	std::string message = Getsha256hash(cp_msg.SerializeAsString());
    Account account;
    if(MagicSingleton<AccountManager>::GetInstance()->getAccountPublicKeyByBytes(pub, account) == false){
        ERRORLOG(RED " handleContractPackagerMessage Get public key from bytes failed!" RESET);
        return -1;
    }
    if(account.Verify(message, signature) == false)
    {
        ERRORLOG(RED "handleBuildBlockBroadcastMessage Public key verify sign failed!" RESET);
        return -2;
    }

    std::string contractHash;
    std::vector<uint64_t> txTimestamps;
    for (const TxMsgReq& txMsg : msg->txmsgreq())
    {
        const TxMsgInfo& txMsgInfo = txMsg.txmsginfo();
        CTransaction transaction;
        if (!transaction.ParseFromString(txMsgInfo.tx()))
        {
            ERRORLOG("Failed to deserialize transaction body!");
            continue;
        }
        contractHash += transaction.hash();
        txTimestamps.push_back(transaction.time());
    }

	auto maxIterator = std::max_element(txTimestamps.begin(), txTimestamps.end());
	uint64_t maxTxTime = 0;
	if (maxIterator != txTimestamps.end()) {
		maxTxTime = *maxIterator;
	}

    std::string input = Getsha256hash(contractHash);
    DEBUGLOG("handleContractPackagerMessage input : {} , maxTxTime : {}", input, maxTxTime);
    DEBUGLOG("2222222222222 HHHHHHHHHH");
    
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    int ret = ContractPackVerifier(input, maxTxTime, defaultAddr);
    if(ret != 0)
    {
        ERRORLOG("ContractPackVerifier ret  {}", ret);
        return -8;
    }
    DEBUGLOG("333333333333333 HHHHHHHHHH,txSize:{}", msg->txmsgreq().size());
    std::set<CTransaction> ContractTxs;

    std::map<int64_t, std::vector<TxMsgReq>> blockTimestampGroupedTransaction;

    for (const TxMsgReq& txMsg : msg->txmsgreq())
    {
        const TxMsgInfo &txMsgInfo = txMsg.txmsginfo();
        CTransaction transaction;
        if (!transaction.ParseFromString(txMsgInfo.tx())) {
            ERRORLOG("Failed to deserialize transaction body!");
            continue;
        }
        int64_t blockTimestamp = evmEnvironment::GetBlockTimestamp(transaction);
        if (blockTimestamp <= 0)
        {
            ERRORLOG("block timestamp less than 0, tx: {}", transaction.hash());
            continue;
        }
        DEBUGLOG("time: {}, tx: {}", blockTimestamp, transaction.hash());
        blockTimestampGroupedTransaction[blockTimestamp].push_back(txMsg);
    }

    for (const auto& [blockTimestamp, transmitMessageRequests] : blockTimestampGroupedTransaction)
    {
        ContractTransaction(transmitMessageRequests);
    }
}

int TransactionCache::ContractTransaction(const std::vector<TxMsgReq> &transmitMessageRequests) {
    packDispatch packdis;
    std::map<std::string, std::future<int>> txTaskResults_;
    int64_t topTransactionHeight_ = 0;

    for (const TxMsgReq& txMsg : transmitMessageRequests)
    {
        const TxMsgInfo& txMsgInfo = txMsg.txmsginfo();
        CTransaction transaction;
        if (!transaction.ParseFromString(txMsgInfo.tx()))
        {
            ERRORLOG("Failed to deserialize transaction body!");
            continue;
        }

        if (txMsgInfo.nodeheight() > topTransactionHeight_)
        {
            topTransactionHeight_ = txMsgInfo.nodeheight();
        }
        DEBUGLOG("tx:{} height {}", transaction.hash(), txMsgInfo.nodeheight());
        DEBUGLOG("set_dirty_contract_map, txHash:{}", transaction.hash());
        set_dirty_contract_map(transaction.hash(), {txMsgInfo.contractstoragelist().begin(), txMsgInfo.contractstoragelist().end()});

        std::vector<std::string> dependentAddress(txMsgInfo.contractstoragelist().begin(), txMsgInfo.contractstoragelist().end());
        packdis.Add(transaction.hash(),dependentAddress,transaction);

        auto task = std::make_shared<std::packaged_task<int()>>(
                [txMsg, txMsgInfo, transaction] {
                    std::string dispatcherAddr = transaction.identity();
                    if(!hasContractPackingPermission(dispatcherAddr, txMsgInfo.nodeheight(), transaction.time()))
                    {
                        ERRORLOG("hasContractPackingPermission fail!!!, txHash:{}", transaction.hash().substr(0,6));
                        return -1;
                    }

                    int ret = handleTransaction(std::make_shared<TxMsgReq>(txMsg), *std::make_unique<CTransaction>());
                    if (ret != 0)
                    {
                        ERRORLOG("handleTransaction fail ret: {}, tx hash : {}", ret, transaction.hash());
                        return ret;
                    }
                    return 0;
                });
        try
        {
            txTaskResults_[transaction.hash()] = task->get_future();
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
        MagicSingleton<TaskPool>::GetInstance()->commitWorkTask([task](){(*task)();});
    }
    DEBUGLOG("block height will be {}", topTransactionHeight_);
    DEBUGLOG("3333333333333444 HHHHHHHHHH");

    std::map<uint32, std::future<std::pair<int, std::string>>> dependentContractTxTaskResults_;
    std::vector<std::future<std::pair<int, std::string>>> nonDependentContractTxTaskResults;

    std::map<uint32_t, std::map<std::string, CTransaction>> dependentContractTxMap_;
    std::map<std::string, CTransaction> non_dependent_contract_tx_map;
    packdis.GetDependentData(dependentContractTxMap_, non_dependent_contract_tx_map);
    DEBUGLOG("44444444444 HHHHHHHHHH");
    auto processDependencyContract = [&dependentContractTxTaskResults_](const uint32_t N, std::map<std::string, CTransaction> ContractTxs, int64_t blockNumber) {
        auto task = std::make_shared<std::packaged_task<std::pair<int, std::string>()>>(
            [ContractTxs, blockNumber] {
                return MagicSingleton<TransactionCache>::GetInstance()->executeContracts(ContractTxs, blockNumber);
            });

        if (task)
            dependentContractTxTaskResults_[N] =task->get_future();
        else
            return -10;

        MagicSingleton<TaskPool>::GetInstance()->CommitTransactionTask([task]() { (*task)(); });

        return 0; 
    };

    for(const auto& iter : dependentContractTxMap_)
    {
        DEBUGLOG("dependentContractTxMap_ HHHHHHHHHH first:{}, second size:{}", iter.first, iter.second.size());
        processDependencyContract(iter.first, iter.second, topTransactionHeight_);
    }
    DEBUGLOG("555555555555 HHHHHHHHHH");

    DEBUGLOG("non_dependent_contract_tx_map HHHHHHHHHH size:{}", non_dependent_contract_tx_map.size());
    for(const auto& iter : non_dependent_contract_tx_map)
    {
        std::map<std::string, CTransaction> ContractTxs;
        ContractTxs.emplace(iter.first, iter.second);

        auto task = std::make_shared<std::packaged_task<std::pair<int, std::string>()>>(
        [ContractTxs, topTransactionHeight_]
        {
            return MagicSingleton<TransactionCache>::GetInstance()->executeContracts(ContractTxs, topTransactionHeight_);
        });

        if(task)    nonDependentContractTxTaskResults.push_back(task->get_future());
        else    return -10;

        MagicSingleton<TaskPool>::GetInstance()->commit_block_task([task](){(*task)();});
    }
    DEBUGLOG("66666666666 HHHHHHHHHH");
    std::map<std::string, int> txRes;
    for (auto& res : txTaskResults_)
    {
        if (res.second.valid()) 
        {
            auto retPair = res.second.get();
            txRes[res.first] = retPair;
        }
    }
    DEBUGLOG("77777777777 HHHHHHHHHH");

    std::set<uint32_t> isFailureDependent;
    for (auto& res : txRes)
    {
        if(res.second != 0)
        {
            ERRORLOG("isFailureDependent txHash:{}, ret:{}", res.first, res.second);
            for (auto iter = dependentContractTxMap_.begin(); iter != dependentContractTxMap_.end();) 
            {
                if (iter->second.find(res.first) != iter->second.end()) 
                {
                    removeContractInfoCacheRequest(iter->second);
                    isFailureDependent.insert(iter->first);
                    ERRORLOG("verify tx fail !!! delete contract tx:{}", res.first);
                    iter->second.erase(res.first);
                }

                if (iter->second.empty()) 
                {
                    iter = dependentContractTxMap_.erase(iter);
                } 
                else 
                {
                    ++iter;
                }
            }
            if(non_dependent_contract_tx_map.find(res.first) != non_dependent_contract_tx_map.end())
            {
                std::map<std::string, CTransaction> contractTxs;
                contractTxs[res.first] = {};
                removeContractInfoCacheRequest(contractTxs);
            }
        }
    }

    for(const auto& iter : isFailureDependent)
    {
        dependentContractTxTaskResults_.erase(iter);
        processDependencyContract(iter, dependentContractTxMap_[iter], topTransactionHeight_);
    }

    std::map<std::string, int> dependentContextResult;
    for (auto& res : dependentContractTxTaskResults_)
    {
        if (res.second.valid()) 
        {
            auto retPair = res.second.get();
            dependentContextResult[retPair.second] = retPair.first;
        }
    }
    DEBUGLOG("888888888888 HHHHHHHHHH");

    std::map<std::string, int> non_dependent_ctx_result;
    for (auto& res : nonDependentContractTxTaskResults)
    {
        if(res.valid())
        {
            auto retPair = res.get();
            non_dependent_ctx_result[retPair.second] = retPair.first;
        }
        
    }
    DEBUGLOG("999999999999 HHHHHHHHHH");

    for (auto& res : dependentContextResult)
    {
        if(res.second != 0)
        {
            ERRORLOG("faildepenDentCTxRes txHash:{}, ret:{}", res.first, res.second);
            for(const auto& iter : dependentContractTxMap_)
            {
                if(iter.second.find(res.first) != iter.second.end())
                {
                    removeContractsCacheRequest(iter.second);
                }
            }
            ERRORLOG("_ExecuteDependentContractTx fail!!!, txHash:{}", res.first);
        }
    }

    for (auto& res : non_dependent_ctx_result)
    {
        if(res.second != 0)
        {
            ERRORLOG("non_dependent_ctx_result txHash:{}, ret:{}", res.first, res.second);
            std::map<std::string, CTransaction> contractTxs;
            contractTxs[res.first] = {};
            removeContractsCacheRequest(contractTxs);
            ERRORLOG("_ExecuteNonDependentContractTx fail!!!, txHash:{}", res.first);
        }
    }
    
    DEBUGLOG("1010101010101 HHHHHHHHHH");
    ProcessContract(topTransactionHeight_);
    DEBUGLOG("Packager handleContractPackagerMessage 005 ");
    return 0;
}

int TransactionCache::fetchContractTxPreHash(const std::list<CTransaction>& txs, std::list<std::pair<std::string, std::string>>& contractTxPrevHashList)
{
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> contractTxPreCacheMap;
    for(auto& tx : txs)
	{
        if (global::ca::TxType::kTransactionTypeDeploy == (global::ca::TxType)tx.txtype())
        {
            continue;
        }
        auto txHash = tx.hash();
        nlohmann::json txStorage;
        if (MagicSingleton<TransactionCache>::GetInstance()->get_contract_info_cache(txHash, txStorage) != 0)
        {
            ERRORLOG("can't find storage of tx {}", txHash);
            return -1;
        }

        for(auto &it : txStorage[Evmone::contractPreHashKey].items())
        {
            contractTxPreCacheMap[txHash].push_back({it.key(), it.value()});
        }
	}
    DBReader dbReader;
    for(auto& iter : contractTxPreCacheMap)
    {
        for(auto& preHashPair : iter.second)
        {
            if(contractTxPreCacheMap.find(preHashPair.second) != contractTxPreCacheMap.end())
            {
                continue;
            }
            std::string dbContractPreHash;
            if (DBStatus::DB_SUCCESS != dbReader.getLatestUtxoByContractAddr(preHashPair.first, dbContractPreHash))
            {
                ERRORLOG("getLatestUtxoByContractAddr fail !!! ContractAddr:{}", preHashPair.first);
                return -2;
            }
            if(dbContractPreHash != preHashPair.second)
            {
                ERRORLOG("dbContractPreHash:({}) != preHashPair.second:({})", dbContractPreHash, preHashPair.second);
                return -3;
            }
            contractTxPrevHashList.push_back(preHashPair);
        }
    }
    return 0;
}

bool TransactionCache::removeContractInfoCacheRequest(const std::map<std::string, CTransaction>& contractTxs)
{
    std::shared_lock<std::shared_mutex> locker(contractInfoCacheMutex);
    auto it = contractInfoCache.begin();
    while (it != contractInfoCache.end()) 
    {
        if (contractTxs.find(it->first) != contractTxs.end()) 
        {
            DEBUGLOG("txHash:{}", it->first);
            it = contractInfoCache.erase(it); 
        } 
        else 
        {
            ++it;
        }
    }
    return true;
}

bool TransactionCache::removeContractsCacheRequest(const std::map<std::string, CTransaction>& contractTxs) {
    std::unique_lock<std::mutex> locker(contractCacheMutex);
    auto it = _contractCache.begin();
    while (it != _contractCache.end()) {
        if (contractTxs.find(it->GetTransaction().hash()) != contractTxs.end()) {
            it = _contractCache.erase(it);
        } else {
            ++it;
        }
    }
    return true;
}

int handleSeekContractPreHashRequest(const std::shared_ptr<newSeekContractPreHashReq> &msg, const MsgData &msgdata)
{
    newSeekContractPreHashAck ack;
    ack.set_version(msg->version());
    ack.set_msg_id(msg->msg_id());
    ack.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    Node node;
	if (!MagicSingleton<PeerNode>::GetInstance()->locateNodeByFd(msgdata.fd, node))
	{
        ERRORLOG("locateNodeByFd fail !!!, seekId:{}", node.address);
		return -1;
	}

    DBReader dbReader;
    if(msg->seekroothash_size() >= 200)
    {
        ERRORLOG("msg->seekroothash_size:({}) >= 200", msg->seekroothash_size());
        return -2;
    }
    for(auto& preHashPair : msg->seekroothash())
    {
        std::string dbContractPreHash;
        if (DBStatus::DB_SUCCESS != dbReader.getLatestUtxoByContractAddr(preHashPair.contractaddr(), dbContractPreHash))
        {
            ERRORLOG("getLatestUtxoByContractAddr fail !!!");
            return -3;
        }
        if(dbContractPreHash != preHashPair.roothash())
        {
            DEBUGLOG("dbContractPreHash:({}) != roothash:({}) seekId:{}", dbContractPreHash, preHashPair.roothash(), node.address);
            std::string prevBlockHash;
            if(dbReader.getBlockHashByTransactionHash(dbContractPreHash, prevBlockHash) != DBStatus::DB_SUCCESS)
            {
                ERRORLOG("getBlockHashByTransactionHash failed!");
                return -4;
            }
            std::string blockRaw;
            if(dbReader.getBlockByBlockHash(prevBlockHash, blockRaw) != DBStatus::DB_SUCCESS)
            {
                ERRORLOG("getBlockByBlockHash failed!");
                return -5;
            }
            auto seekContractBlock = ack.add_seekcontractblock();
            seekContractBlock->set_contractaddr(preHashPair.contractaddr());
            seekContractBlock->set_roothash(prevBlockHash);
            seekContractBlock->set_blockraw(blockRaw);
        }
    }

    NetSendMessage<newSeekContractPreHashAck>(node.address, ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    return 0;
}
int handleSeekContractPreHashAcknowledgement(const std::shared_ptr<newSeekContractPreHashAck> &msg, const MsgData &msgdata)
{
    dataMgrPtr.waitDataToAdd(msg->msg_id(),msg->self_node_id(),msg->SerializeAsString());
    return 0;
}

int newSeekContractPreHash(const std::list<std::pair<std::string, std::string>> &contractTxPrevHashList)
{
    DEBUGLOG("newSeekContractPreHash.............");
    uint64_t chainHeight;
    if(!MagicSingleton<BlockHelper>::GetInstance()->getChainHeight(chainHeight))
    {
        DEBUGLOG("getChainHeight fail!!!");
    }
    uint64_t nodeSelfHeight = 0;
    std::vector<std::string> pledgeAddr;
    {
        DBReader dbReader;
        auto status = dbReader.getBlockTop(nodeSelfHeight);
        if (DBStatus::DB_SUCCESS != status)
        {
            DEBUGLOG("getBlockTop fail!!!");

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
    if (get_prehash_find_node(pledgeAddr.size(), chainHeight, pledgeAddr, node_ids_to_send) != 0)
    {
        ERRORLOG("get sync node fail");
    }

    if(node_ids_to_send.size() == 0)
    {
        DEBUGLOG("node_ids_to_send {}",node_ids_to_send.size());
        return -2;
    }

    //send_size
    std::string msgId;
    if (!dataMgrPtr.CreateWait(3, node_ids_to_send.size() * 0.8, msgId))
    {
        return -3;
    }

    newSeekContractPreHashReq req;
    req.set_version(global::GetVersion());
    req.set_msg_id(msgId);

    for(auto &item : contractTxPrevHashList)
    {
        preHashPair * _hashPair = req.add_seekroothash();
        _hashPair->set_contractaddr(item.first);
        _hashPair->set_roothash(item.second);
        DEBUGLOG("req contractAddr:{}, contractTxHash:{}", item.first, item.second);
    }
    
    for (auto &node : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, node))
        {
            return -4;
        }
        NetSendMessage<newSeekContractPreHashReq>(node, req, net_com::Compress::kCompressDisabled, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    }

    std::vector<std::string> ret_datas;
    if (!dataMgrPtr.WaitData(msgId, ret_datas))
    {
        return -5;
    }

    newSeekContractPreHashAck ack;
    std::map<std::string, std::set<std::string>> blockHashMap;
    std::map<std::string, std::pair<std::string, std::string>> testMap;
    for (auto &ret_data : ret_datas)
    {
        ack.Clear();
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        for(auto& iter : ack.seekcontractblock())
        {
            blockHashMap[ack.self_node_id()].insert(iter.blockraw());
            testMap[iter.blockraw()] = {iter.contractaddr(), iter.roothash()};
        }
    }

    std::unordered_map<std::string , int> countMap;
    for (auto& iter : blockHashMap) 
    {
        for(auto& iter_second : iter.second)
        {
            countMap[iter_second]++;
        }
        
    }

    DBReader dbReader;
    std::vector<std::pair<CBlock,std::string>> seekBlocks;
    for (const auto& iter : countMap) 
    {
        double rate = double(iter.second) / double(blockHashMap.size());
        auto test_iter = testMap[iter.first];
        if(rate < 0.66)
        {
            ERRORLOG("rate:({}) < 0.66, contractAddr:{}, contractTxHash:{}", rate, test_iter.first, test_iter.second);
            continue;
        }

        CBlock block;
        if(!block.ParseFromString(iter.first))
        {
            continue;
        }
        std::string blockStr;
        if(dbReader.getBlockByBlockHash(block.hash(), blockStr) != DBStatus::DB_SUCCESS)
        {
            seekBlocks.push_back({block, block.hash()});
            DEBUGLOG("rate:({}) < 0.66, contractAddr:{}, contractTxHash:{}, blockHash:{}", rate, test_iter.first, test_iter.second, block.hash());
            MagicSingleton<BlockHelper>::GetInstance()->add_seek_block(seekBlocks);
        }
    }

    uint64_t timeOut = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp() + 2 * 1000000;
    uint64_t currentTime;
    bool flag;
    do
    {
        flag = true;
        for(auto& it : seekBlocks)
        {
            std::string blockRaw;
            if(dbReader.getBlockByBlockHash(it.second, blockRaw) != DBStatus::DB_SUCCESS)
            {
                flag = false;
                break;
            }
        }
        if(flag)
        {
            DEBUGLOG("find block successfuly ");
            return 0;
        }
        sleep(1);
        currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    }while(currentTime < timeOut && !flag);
    return -6;
}

int handleContractPackagerMessage(const std::shared_ptr<ContractPackagerMsg> &msg, const MsgData &msgdata)
{
    MagicSingleton<TransactionCache>::GetInstance()->handleContractPackagerMessage(msg, msgdata);
    return 0;
}
