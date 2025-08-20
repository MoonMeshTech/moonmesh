#include "advanced_menu.h"

#include <map>
#include <regex>
#include <thread>
#include <ostream>
#include <fstream>

#include "ca/ca.h"
#include "ca/test.h"
#include "ca/global.h"
#include "ca/contract.h"
#include "ca/txhelper.h"
#include "ca/interface.h"
#include "ca/algorithm.h"

#include "ca/transaction.h"
#include "ca/block_helper.h"
#include "ca/block_monitor.h"
#include "ca/genesis_config.h"
#include "ca/genesis_block_generator.h"
#include "ca/contract_transaction_cache.h"
#include "ca/dispatchtx.h"

#include "net/api.h"
#include "net/peer_node.h"

#include "include/scope_guard.h"
#include "include/logging.h"

#include "openssl/crypto.h"
#include "utils/tmp_log.h"
#include "utils/console.h"
#include "utils/time_util.h"
#include "utils/contract_utils.h"
#include "utils/bench_mark.h"
#include "utils/magic_singleton.h"
#include "utils/account_manager.h"
#include "utils/base64.h"
#include "utils/keccak_cryopp.hpp"

#include "db/db_api.h"
#include "ca/evm/evm_manager.h"
#include "openssl/rand.h"

constexpr size_t MAX_BATCH_SIZE = 990;
const static uint64_t inputLimitFirst = 500000;
struct contractJob{
    std::string fromAddr;
    std::string deployer;
    std::string contractAddresses;
    std::string arg;
    std::string tip;
    std::string money;
};

std::vector<contractJob> jobs;
std::atomic<int> jobs_index=0;
std::atomic<int> errNodeIndex=0;
boost::threadpool::pool test_pool;

void readContractJson(const std::string & file_name){
    std::ifstream file(file_name);
    std::stringstream buffer;  
    buffer << file.rdbuf();  
    std::string contents(buffer.str());
    if(contents.empty()){
        errorL("no data");
        return;
    }
    nlohmann::json json_object;
    try {
       json_object=nlohmann::json::parse(contents);
    } catch (std::exception & e) {
       errorL(e.what());
       return;
    }

    if(!json_object.is_array()){
        errorL("not a array");
       return;
    }
    try{
       for(auto &aitem:json_object){
            contractJob job;
            job.deployer=aitem["deployer"];
            job.contractAddresses=aitem["contractAddresses"];
            job.arg=aitem["arg"];
            job.money=aitem["money"];
            jobs.push_back(job);
       }
    }catch(std::exception & e){
        errorL("wath:%s",e.what());
       return;
    }
}

void invokeContract(contractJob job){
    

    infoL("fromAddr:%s",job.fromAddr);
    infoL("deployer:%s",job.deployer);
    infoL("deployutxo:%s",job.contractAddresses);
    infoL("money:%s",job.money);
    infoL("arg:%s",job.arg);

    std::string strFromAddr=job.fromAddr;

    if (!isValidAddress(strFromAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;
    }
    DBReader dataReader;

    std::string strToAddr=job.deployer;
    if(!isValidAddress(strToAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;        
    }

    std::string _contractAddresses=job.contractAddresses;
    std::string strInput=job.arg;
    if(strInput.substr(0, 2) == "0x")
    {
        strInput = strInput.substr(2);
    }

    
    std::regex pattern("^\\d+(\\.\\d+)?$");

    std::string transferContractString=job.money;
    if (!std::regex_match(transferContractString, pattern))
    {
        std::cout << "input contract transfer error ! " << std::endl;
        return;
    }
    uint64_t contractTransfer = (std::stod(transferContractString) + global::ca::MIN_DOUBLE_CONSTANT_PRECISION) * global::ca::kDecimalNum;
    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != dataReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return ;
    }
    int ret = 0;
    std::string assetTypeS;
    ret = ca_algorithm::GetCanBeRevokeAssetType(assetTypeS);
    if(ret != 0){
        ERRORLOG("Get Can BeRevoke AssetType fail!");\
        return;
    }
    
    std::pair<std::string, std::string> gasTrade = {strFromAddr, assetTypeS};
    CTransaction outTx;
 
    
    TxHelper::vrfAgentType needAgent;
    std::vector<std::string> dirtyContract;
    std::string encodedInfo = "";
    Account launchAccount;
    if(MagicSingleton<AccountManager>::GetInstance()->FindAccount(strFromAddr, launchAccount) != 0)
    {
        ERRORLOG("Failed to find account {}", strFromAddr);
        return;
    }

    ret = TxHelper::createEvmCallContractTransactionRequest(strFromAddr, strToAddr, strInput, encodedInfo, gasTrade, top + 1,
                                                     outTx, needAgent, contractTransfer,
                                                     dirtyContract, _contractAddresses, false);



    ContractTxMsgReq ContractMsg;
    ContractMsg.set_version(global::GetVersion());
    TxMsgReq * txMsg = ContractMsg.mutable_txmsgreq();
	txMsg->set_version(global::GetVersion());
    TxMsgInfo * txMsgInfo = txMsg->mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t txUtxoLocalHeight;
    ret = TxHelper::get_tx_utxo_height(outTx, txUtxoLocalHeight);
    if (ret != 0)
    {
        ERRORLOG("get_tx_utxo_height fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(txUtxoLocalHeight);

    std::cout << "size = " << dirtyContract.size() << std::endl;
    for (const auto &addr : dirtyContract)
    {
        std::cout << "addr = " << addHexPrefix(addr) << std::endl;
        txMsgInfo->add_contractstoragelist(addr);
    }
 
     // Check if contract transaction can be sent
    if (!MagicSingleton<DependencyManager>::GetInstance()->CanSendTransaction(ContractMsg))
    {
        // Add to contract transaction cache if not sendable
        int cacheRet = MagicSingleton<ContractTransactionCache>::GetInstance()->AddFailedTransaction(outTx, ContractMsg);
        if (cacheRet != 0)
        {
            ERRORLOG("Failed to add contract transaction to cache, ret: {}", cacheRet);
            std::cout << "Failed to cache contract transaction" << std::endl;
            return;
        }
        
        DEBUGLOG("Contract transaction {} added to cache due to sendability check", outTx.hash());
        std::cout << "Contract transaction cached for later sending" << std::endl;
        return;
    }

    auto msg = std::make_shared<ContractTxMsgReq>(ContractMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(needAgent == TxHelper::vrfAgentType::vrfAgentType_vrf)
    {
        ret = dropCallShippingRequest(msg,outTx);
        MagicSingleton<BlockMonitor>::GetInstance()->dropshippingTxVec(outTx.hash());
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}

std::string ReadFileIntoString(std::string filename)
{
	std::ifstream ifile(filename);
	std::ostringstream buf;
	char ch;
	while(buf&&ifile.get(ch))
    {
        buf.put(ch);
    }
	return buf.str();
}

int LoopReadFile(std::string& input, std::string& output, const std::filesystem::path& filename = "")
{
    std::filesystem::path contract_path;
    bool raise_info = false;
    if (input == "0")
    {
        contract_path = std::filesystem::current_path() / "contract" / filename;
    }
    else
    {
        contract_path = input;
        raise_info = true;
    }

    if (raise_info)
    {
        while(!exists(contract_path))
        {
            input.clear();
            std::cout << contract_path << " doesn't exist! please enter again: (0 to skip, 1 to exit)" << std::endl;
            std::cin >> input;
            if (input == "0")
            {
                return 1;
            }
            if (input == "1")
            {
                return -1;
            }
            contract_path = input;
        }
    }
    else
    {
        if (!exists(contract_path))
        {
            return 1;
        }
    }

    output = ReadFileIntoString(contract_path.string());
    if(output.size() > inputLimitFirst)
    {
        std::cout << "Input cannot exceed " << inputLimitFirst << " characters" << std::endl;
        return -2;
    }
    return 0;
}

void HandleMultiDeployContract1(const std::string &strFromAddr)
{
    if (!isValidAddress(strFromAddr))
    {
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    DBReader dataReader;
    uint64_t top = 0;
	if (DBStatus::DB_SUCCESS != dataReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return ;
    }

    CTransaction outTx;
    TxHelper::vrfAgentType needAgent;
    std::vector<std::string> dirtyContract;
    std::string assetTypeS;
    int ret1 = ca_algorithm::GetCanBeRevokeAssetType(assetTypeS);
    if (ret1 != 0) {
        ERRORLOG("Get Can Be Revoke AssetType fail!");
        return; 
    }
    
    std::pair<std::string,std::string> gasTrade = {strFromAddr, assetTypeS};

    std::string nContractPath = "0";
    std::string code;
    int ret = LoopReadFile(nContractPath, code, "contract");
    if (ret != 0)
    {
        return;
    }

    if(code.empty())
    {
        return;
    }

    auto result = Evmone::latestContractAddress(strFromAddr);
    if (!result.has_value())
    {
        return;
    }
    std::string encodedInfo = "";
    uint64_t contractTransfer = 0;
    std::string contractAddress = result.value();
    ret = TxHelper::createEvmDeployContractTransactionRequest(top + 1,
                                                       outTx, gasTrade, dirtyContract,
                                                       needAgent,
                                                       code, strFromAddr, contractTransfer, contractAddress, encodedInfo);
    if(ret != 0)
    {
        ERRORLOG("Failed to create DeployContract transaction! The error code is:{}", ret);
        return;
    }
    
    ContractTxMsgReq ContractMsg;
    ContractMsg.set_version(global::GetVersion());
    TxMsgReq * txMsg = ContractMsg.mutable_txmsgreq();
	txMsg->set_version(global::GetVersion());
    TxMsgInfo * txMsgInfo = txMsg->mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);
    uint64_t txUtxoLocalHeight;
    ret = TxHelper::get_tx_utxo_height(outTx, txUtxoLocalHeight);
    if(ret != 0)
    {
        ERRORLOG("get_tx_utxo_height fail!!! ret = {}", ret);
        return;
    }

    for (const auto& addr : dirtyContract)
    {
        txMsgInfo->add_contractstoragelist(addr);
    }

    // Check if contract transaction can be sent
    if (!MagicSingleton<DependencyManager>::GetInstance()->CanSendTransaction(ContractMsg))
    {
        // Add to contract transaction cache if not sendable
        int cacheRet = MagicSingleton<ContractTransactionCache>::GetInstance()->AddFailedTransaction(outTx, ContractMsg);
        if (cacheRet != 0)
        {
            ERRORLOG("Failed to add contract transaction to cache, ret: {}", cacheRet);
            std::cout << "Failed to cache contract transaction" << std::endl;
            return;
        }
        
        DEBUGLOG("Contract transaction {} added to cache due to sendability check", outTx.hash());
        std::cout << "Contract transaction cached for later sending" << std::endl;
        return;
    }

    auto msg = std::make_shared<ContractTxMsgReq>(ContractMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(needAgent == TxHelper::vrfAgentType::vrfAgentType_vrf )
    {
        ret = dropCallShippingRequest(msg, outTx);
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
    std::cout << "Transaction result : " << ret << std::endl;
}

void CreateAutomaticMultDeployContract()
{
    std::vector<std::string> acccountlist;
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(acccountlist);
    errNodeIndex=0;
    int time_s;
    std::string fromAddr;
    std::cout << "time_s:" ;
    std::cin >>  time_s;
    std::cout << "second:";
    long long _second;
    std::cin >> _second;
    std::cout << "hom much:";
    long long _much;
    std::cin >> _much;
    int oneSecond=0;
    while(time_s){
        oneSecond++;
        std::cout <<"xxx x" << std::endl;
        fromAddr = acccountlist[errNodeIndex];
        test_pool.schedule(boost::bind(HandleMultiDeployContract1, fromAddr));
        std::thread th=std::thread(HandleMultiDeployContract1,fromAddr);
        th.detach();
        errNodeIndex=++errNodeIndex%acccountlist.size();
        if(errNodeIndex == 0)
            break;
        ::usleep(_second *1000 *1000 / _much);
        if(oneSecond == _much){
            time_s--;
            oneSecond=0;
        }
    }
}


void contact_thread_test(){
    readContractJson("contract.json");
    std::vector<std::string> acccountlist;
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(acccountlist);
   
    jobs_index=0;
    errNodeIndex=0;
    int time_s;
    std::cout << "time_s:" ;
    std::cin >>  time_s;
    std::cout << "second:";
    long long _second;
    std::cin >> _second;
    std::cout << "hom much:";
    long long _much;
    std::cin >> _much;
    int oneSecond=0;
    while(time_s){
        oneSecond++;
        std::cout <<"hhh h" << std::endl;
        jobs[jobs_index].fromAddr=acccountlist[errNodeIndex];
        test_pool.schedule(boost::bind(invokeContract, jobs[jobs_index]));
        std::thread th=std::thread(invokeContract,jobs[jobs_index]);
        th.detach();
        jobs_index=++jobs_index%jobs.size();
        errNodeIndex=++errNodeIndex%acccountlist.size();
        ::usleep(_second *1000 *1000 / _much);
        if(oneSecond == _much){
            time_s--;
            oneSecond=0;
        }
    }
}


void RollBack()
{
    MagicSingleton<BlockHelper>::GetInstance()->RollbackTest();
}

void GetStakeList()
{
    DBReader dbReader;
    std::vector<std::string> addResses;
    dbReader.getStakeAddr(addResses);
    std::cout << "StakeList :" << std::endl;
    for (auto &it : addResses)
    {
        double timp = 0.0;
        ca_algorithm::GetCommissionPercentage(it, timp);
        std::cout << "addr: " << "0x"+it << "\tbonus pumping: " << timp << std::endl;
    }
}

void fetchExchequer(){
    std::map<std::string, TxHelper::ProposalInfo> assetMap;
    int ret = ca_algorithm::fetchAvailableAssetType(assetMap);
	if(ret != 0){
		ERRORLOG("Get available asset fail! ret : {}",ret);
		return;
	}
    DBReader dbReader; 
    uint64_t curTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    uint64_t period = MagicSingleton<TimeUtil>::GetInstance()->GetPeriod(curTime);
    TxHelper::ProposalInfo null;
    assetMap.insert({global::ca::ASSET_TYPE_VOTE,null});
    int choice;
    std::cout << "Enter 0 to see today and 1 to see yesterday: ";
    std::cin >> choice;
    if (choice == 1){
        period = period - 1;
    }
    std::map<std::string,uint64_t> assetTypeAmount;
    for(const auto& _assetType : assetMap){
        uint64_t gasAmount = 0;
        auto result = dbReader.getGasAmountByPeriod(period,_assetType.first,gasAmount);
        if(result != DBStatus::DB_SUCCESS && result != DBStatus::DB_NOT_FOUND){
            ERRORLOG("GetGasamountbyTimeType error!");
            return ;
        }
        if(gasAmount != 0){
            assetTypeAmount.insert({_assetType.first,gasAmount});
        }
    }
    if (assetTypeAmount.empty()) {
        std::cout << "assetTypeAmount is empty." << std::endl;
    }
    for(const auto& _assetType : assetTypeAmount){
        std::cout << "Asset Type: " << _assetType.first << ", Amount: " << _assetType.second << std::endl;
    }
}

int fetchBonusAddressInfo()
{
    DBReader dbReader;
    std::multimap<std::string, std::string> delegatingAddr_currency;
    std::vector<std::string> bonus_addresses_list;
    dbReader.getBonusAddr(bonus_addresses_list);
    std::ofstream outFile("getbonusaddrinfo.txt", std::ios::trunc);
    for (auto &bonusAddr : bonus_addresses_list)
    {
        usleep(1);
        delegatingAddr_currency.clear();
        std::cout << YELLOW << "BonusAddr: " << addHexPrefix(bonusAddr) << RESET << std::endl;
        outFile << "BonusAddr: " << addHexPrefix(bonusAddr) << std::endl;
        auto ret = dbReader.getDelegatingAddrByBonusAddr(bonusAddr, delegatingAddr_currency);
        if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND)
        {
            return -1;
        }

        uint64_t sumdelegateAmount = 0;
        std::cout << "DelegatingAddr:" << std::endl;
        outFile << "DelegatingAddr:" << std::endl;
        for (auto &[delegatingAddr, currency] : delegatingAddr_currency)
        {
            std::cout << addHexPrefix(delegatingAddr) << " currency: " << currency << std::endl;
            outFile << addHexPrefix(delegatingAddr) << " currency: " << currency << std::endl;
            std::vector<std::string> utxos;
            ret = dbReader.getBonusAddrDelegatingAddrUtxoByBonusAddr(bonusAddr, delegatingAddr, currency, utxos);
            if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND)
            {
                return -2;
            }

            uint64_t delegateAmount = 0;
            for (const auto &hash : utxos)
            {
                std::string txRaw;
                if (dbReader.getTransactionByHash(hash, txRaw) != DBStatus::DB_SUCCESS)
                {
                    return -3;
                }
                CTransaction tx;
                if (!tx.ParseFromString(txRaw))
                {
                    return -4;
                }
                for(const auto & utxo : tx.utxos())
                {
                    for (int i = 0; i < utxo.vout_size(); i++)
                    {
                        if (utxo.vout(i).addr() == global::ca::kVirtualDelegatingAddr)
                        {
                            delegateAmount += utxo.vout(i).value();
                            break;
                        }
                    }
                }
            }
            sumdelegateAmount += delegateAmount;
        }
        std::cout << "total delegating amount :" << sumdelegateAmount << std::endl;
        outFile << "total delegating amount :" << sumdelegateAmount << std::endl;
    }
    return 0;
}

#pragma region netMenu
void SendMessageToUser()
{
    if (net_com::sendOneMessageRequest() == 0){
        DEBUGLOG("send one msg Succ.");
    }else{
        DEBUGLOG("send one msg Fail.");
    }
        
}

void displayKBuckets()
{
    std::cout << "The K bucket is being displayed..." << std::endl;
    auto nodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    MagicSingleton<PeerNode>::GetInstance()->Print(nodeList);
}

void kickOutNode()
{
    std::string id;
    std::cout << "input id:" << std::endl;
    std::cin >> id;
    MagicSingleton<PeerNode>::GetInstance()->DeleteNode(id);
    std::cout << "Kick out node succeed!" << std::endl;
}

void TestEcho()
{
    
    std::string message;
    std::cout << "please input message:" << std::endl;
    std::cin >> message;
    std::stringstream ss;
    ss << message << "_" << std::to_string(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());

    EchoReq echoReq;
    echoReq.set_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    echoReq.set_message(ss.str());
    bool isSucceed = net_com::BroadCastMessage(echoReq, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::kPriorityLow0);
    if (isSucceed == false)
    {
        ERRORLOG(":broadcast EchoReq failed!");
        return;
    }
}

void printRequestAndAcknowledgment()
{
    double total = .0f;
    std::cout << "------------------------------------------" << std::endl;
    for (auto &item : global::requestCountMap)
    {
        total += (double)item.second.second;
        std::cout.precision(3);
        std::cout << item.first << ": " << item.second.first << " size: " << (double)item.second.second / 1024 / 1024 << " MB" << std::endl;
    }
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Total: " << total / 1024 / 1024 << " MB" << std::endl;
}

void menuBlockInfo()
{
    while (true)
    {
        DBReader reader;
        uint64_t top = 0;
        reader.getBlockTop(top);

        std::cout << std::endl;
        std::cout << "Height: " << top << std::endl;
        std::cout << "1.Get the total number of transactions \n"
                     "2.Get transaction block details\n"
                     "5.Get device password \n"
                     "6.Set device password\n"
                     "7.Get device private key\n"
                     "0.Exit \n";

        std::string strKey;
        std::cout << "please input your choice:";
        std::cin >> strKey;

        std::regex pattern("^[0-7]$");
        if (!std::regex_match(strKey, pattern))
        {
            std::cout << "Input invalid." << std::endl;
            return;
        }
        int key = std::stoi(strKey);
        switch (key)
        {
        case 0:
            return;
           

        case 2:
            retrieveTransactionBlockInfo(top);
            break;
       

        default:
            std::cout << "Invalid input." << std::endl;
            continue;
        }

        sleep(1);
    }
}

void retrieveTransactionBlockInfo(uint64_t &top)
{
    auto amount = std::to_string(top);
    std::string inputStart, inputEnd;
    uint64_t start, end;

    std::cout << "amount: " << amount << std::endl;
    std::cout << "pleace input start: ";
    std::cin >> inputStart;
    if (inputStart == "a" || inputStart == "pa")
    {
        inputStart = "0";
        inputEnd = amount;
    }
    else
    {
        if (std::stoul(inputStart) > std::stoul(amount))
        {
            std::cout << "input > amount" << std::endl;
            return;
        }
        std::cout << "pleace input end: ";
        std::cin >> inputEnd;
        if (std::stoul(inputStart) < 0 || std::stoul(inputEnd) < 0)
        {
            std::cout << "params < 0!!" << std::endl;
            return;
        }
        if (std::stoul(inputStart) > std::stoul(inputEnd))
        {
            inputStart = inputEnd;
        }
        if (std::stoul(inputEnd) > std::stoul(amount))
        {
            inputEnd = std::to_string(top);
        }
    }
    start = std::stoul(inputStart);
    end = std::stoul(inputEnd);

    std::cout << "Print to screen[0] or file[1] ";
    uint64_t nType = 0;
    std::cin >> nType;
    if (nType == 0)
    {
        printRocksdbInfo(start, end, true, std::cout);
    }
    else if (nType == 1)
    {
        std::string fileName = "print_block_" + std::to_string(start) + "_" + std::to_string(end) + ".txt";
        std::ofstream filestream;
        filestream.open(fileName);
        if (!filestream)
        {
            std::cout << "Open file failed!" << std::endl;
            return;
        }
        printRocksdbInfo(start, end, true, filestream);
    }
}

void PrintTxData()
{
    std::string hash;
    std::cout << "TX hash: ";
    std::cin >> hash;

    DBReader dbReader;

    CTransaction tx;
    std::string TxRaw;
    auto ret = dbReader.getTransactionByHash(hash, TxRaw);
    if (ret != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("getTransactionByHash failed!");
        return;
    }
    if (!tx.ParseFromString(TxRaw))
    {
        ERRORLOG("Transaction Parse failed!");
        return;
    }

    nlohmann::json dataJson = nlohmann::json::parse(tx.data());
    std::string data = dataJson.dump(4);
    std::cout << data << std::endl;
}



void ProcessBatch(const std::set<std::string>& batch, uint64_t amt, const std::vector<std::string>& assetType, std::pair<std::string, std::string>& gasTrade, bool isFindUtxo, uint64_t top,const std::string Addr) {
    std::map<std::string, int64_t> to_addr_amount;
    for (const auto &addr : batch) {
        std::string ss = remove0xPrefix(addr);
        to_addr_amount[ss] = (amt + global::ca::MIN_DOUBLE_CONSTANT_PRECISION) * global::ca::kDecimalNum;
    }

    std::vector<TxHelper::TransTable> txAsset;
    for (const auto &asset : assetType) {
        txAsset.emplace_back(TxHelper::TransTable{asset, {Addr}, to_addr_amount});
    }

    std::string txInfo = "";
    std::string encodedInfo = Base64Encode(txInfo);
    if (encodedInfo.size() > 1024) {
        std::cout << "The information entered exceeds the specified length" << std::endl;
        return;
    }

    CTransaction outTx;
    TxHelper::vrfAgentType needAgent;
    int ret = TxHelper::createTransactionRequest(txAsset, gasTrade, false, encodedInfo, top + 1, outTx, needAgent, isFindUtxo);
    if (ret != 0) {
        ERRORLOG("createTransactionRequest error!!");
        return;
    }

    TxMsgReq txMsg;
    txMsg.set_version(global::GetVersion());
    TxMsgInfo* txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t txUtxoLocalHeight;
    ret = TxHelper::get_tx_utxo_height(outTx, txUtxoLocalHeight);
    if (ret != 0) {
        ERRORLOG("get_tx_utxo_height fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(txUtxoLocalHeight);

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if (outTx.identity() == defaultAddr) {
        ret = handleTransaction(msg, outTx);
    } else {
        ret = DropShippingTransaction(msg, outTx, outTx.identity());
    }
    DEBUGLOG("Transaction result, ret:{}  txHash: {}, identity:{}", ret, outTx.hash(), outTx.identity());
    std::this_thread::sleep_for(std::chrono::seconds(8));
}

void MultiTx() {
    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "1.Import the txt file." << std::endl;
    std::cout << "2.Import all accounts in the cert directory." << std::endl;
    std::string strKey;
    std::cout << "please input your choice:";
    std::cin >> strKey;

    std::regex pattern("^[1-2]");
    if (!std::regex_match(strKey, pattern)) {
        std::cout << "Input invalid." << std::endl;
        return;
    }
    int key = std::stoi(strKey);

    std::set<std::string> toAddrs;

    if (key == 1) {
        std::ifstream fin("toaddr.txt", std::ifstream::binary);
        if (!fin.is_open()) {
            std::cout << "open file error" << std::endl;
            return;
        }
        std::string Addr;
        while (getline(fin, Addr)) {
            if (Addr[Addr.length() - 1] == '\r') {
                Addr = Addr.substr(0, Addr.length() - 1);
            }
            toAddrs.insert(Addr);
        }
        fin.close();
    } else if (key == 2) {
        std::string path = "./cert";
        try {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                toAddrs.insert(entry.path().filename().string());
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "File system error: " << e.what() << std::endl;
        }
    }

    Account account;
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(account);
    std::string strFromAddr = account.GetAddr();
    std::string fromAddr = remove0xPrefix(strFromAddr);
    std::cout << "default addr: " << "0x" + strFromAddr << std::endl;

    std::string Addr;
    std::cout << "input Addr>:";
    std::cin >> Addr;
    auto find = toAddrs.find(Addr);
    if (find != toAddrs.end()) {
        toAddrs.erase(find);
    }
    Addr = remove0xPrefix(Addr);
    find = toAddrs.find(Addr);
    if (find != toAddrs.end()) {
        toAddrs.erase(find);
    }

    std::cout << "================" << std::endl;
    for (auto &z : toAddrs) {
        std::cout << "toaddr:" << z << std::endl;
    }

    uint64_t amt = 0;
    std::cout << "input amount>:";
    std::cin >> amt;

    std::string _isGastrade = "0";
    std::pair<std::string, std::string> gasTrade = {};
    std::string assetTypeS;
    int ret = ca_algorithm::GetCanBeRevokeAssetType(assetTypeS);
    if(ret != 0){
        ERRORLOG("Get Can BeRevoke AssetType fail!");\
        return;
    }

    std::vector<std::string> assetType = {assetTypeS};

    uint64_t top = 0;
    DBReader dbReader;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top)) {
        ERRORLOG("db get top failed!!");
        return;
    }
    std::mutex batchMutex;
    size_t batchSize = 0;
    std::set<std::string> batch;
    for (const auto &addr : toAddrs)
    {
        batch.insert(addr);
        batchSize++;
        if (batchSize >= MAX_BATCH_SIZE) 
        {
            {
                ProcessBatch(batch, amt, assetType, gasTrade, false, top, Addr);
            }
            batch.clear();
            batchSize = 0;
        }
    }
    
    // Process any remaining addresses
    if (!batch.empty()) 
    {
        ProcessBatch(batch, amt, assetType, gasTrade, false, top, Addr);
    }
}

void testaddr2()
{    
    while(true)
    {
        std::cout << "isValidAddress: " << std::endl;
        std::string addr;
        std::cin>>addr;
        if (addr.substr(0, 2) == "0x") 
        {
            addr = addr.substr(2);
        }
        if (!isValidAddress(addr))
        {
            std::cout << "Input addr error!" << std::endl;
            return;
        }
    }
}


void getContractAddr()
{
    // std::cout << std::endl
    //           << std::endl;

    // std::cout << "AddrList : " << std::endl;
    // MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    // std::string strFromAddr;
    // std::cout << "Please enter your addr:" << std::endl;
    // std::cin >> strFromAddr;
    // if (!isValidAddress(strFromAddr))
    // {
    //     std::cout << "Input addr error!" << std::endl;
    //     return;
    // }

    // DBReader dbReader;
    // std::vector<std::string> deployerList;
    // dbReader.getAllEvmDeployerAddr(deployerList);
    // std::cout << "=====================deployers=====================" << std::endl;
    // for(auto& deployer : deployerList)
    // {
    //     std::cout << "deployer: " << deployer << std::endl;
    // }
    // std::cout << "=====================deployers=====================" << std::endl;
    // std::string strToAddr;
    // std::cout << "Please enter to addr:" << std::endl;
    // std::cin >> strToAddr;
    // if(!isValidAddress(strToAddr))
    // {
    //     std::cout << "Input addr error!" << std::endl;
    //     return;
    // }

    // std::vector<std::string> deployedUtxos;
    // dbReader.GetDeployUtxoByDeployerAddr(strToAddr, deployedUtxos);
    // std::cout << "=====================deployed utxos=====================" << std::endl;
    // for(auto& deployUtxoInfo : deployedUtxos)
    // {
    //     std::cout << "deployed utxo: " << deployUtxoInfo << std::endl;
    // }
    // std::cout << "=====================deployed utxos=====================" << std::endl;
    // std::string strTxHash;
    // std::cout << "Please enter tx hash:" << std::endl;
    // std::cin >> strTxHash;


    // std::string addr = evm_utils::GenerateContractAddr(strToAddr+strTxHash);


    // std::cout << addr << std::endl;
}

static bool benchmarkAtomicWriteSwitch = false;
void print_benchmark_to_file()
{
    if(benchmarkAtomicWriteSwitch)
    {
        benchmarkAtomicWriteSwitch = false;
        std::cout << "benchmark automic write has stoped" << std::endl;
        return;
    }
    std::cout << "enter write time interval (unit second) :";
    int interval = 0;
    std::cin >> interval;
    if(interval <= 0)
    {
         std::cout << "time interval less or equal to 0" << std::endl;
         return;
    }
    benchmarkAtomicWriteSwitch = true;
    auto benchmarkAtomicWriteThread = std::thread(
            [interval]()
            {
                while (benchmarkAtomicWriteSwitch)
                {
                    MagicSingleton<Benchmark>::GetInstance()->PrintBenchmarkSummary(true);
                    MagicSingleton<Benchmark>::GetInstance()->print_benchmark_summary_handle_tx(true);
                    sleep(interval);
                }
            }
    );
    benchmarkAtomicWriteThread.detach();
    return;
}

void get_balance_by_utxo()
{
    std::cout << "Inquiry address:";
    std::string addr;
    std::cin >> addr;
    if (addr.substr(0, 2) == "0x") 
    {
        addr = addr.substr(2);
    }
    std::string assetType;
    std::cout << "Please enter the type of asset you want to currency type :" << std::endl;
    std::cin >> assetType;

    DBReader reader;
    std::vector<std::string> utxoHashesList;
     reader.getUtxoHashsByAddress(addr, assetType, utxoHashesList);

    auto utxoOutput = [addr, utxoHashesList, &reader](std::ostream &stream)
    {
        stream << "account:" << addHexPrefix(addr) << " utxo list " << std::endl;

        uint64_t total = 0;
        for (auto i : utxoHashesList)
        {
            std::string txRaw;
            reader.getTransactionByHash(i, txRaw);

            CTransaction tx;
            tx.ParseFromString(txRaw);

            uint64_t value = 0;
            for(const auto & utxo : tx.utxos())
            {
                for (int j = 0; j < utxo.vout_size(); j++)
                
                {
                    CTxOutput txout = utxo.vout(j);
                    if (txout.addr() != addr)
                    {
                        continue;
                    }
                    value += txout.value();
                }
            }
            stream << i << " : " << value << std::endl;
            total += value;
        }

        stream << "address: " << addHexPrefix(addr) << " UTXO total: " << utxoHashesList.size() << " UTXO gross value:" << total << std::endl;
    };

    if (utxoHashesList.size() < 10)
    {
        utxoOutput(std::cout);
    }
    else
    {
        std::string fileName = "utxo_" + addr + ".txt";
        std::ofstream file(fileName);
        if (!file.is_open())
        {
            ERRORLOG("Open file failed!");
            return;
        }
        utxoOutput(file);
        file.close();
    }
}

int MockTransactionStruct()
{
    std::cout << "=== Testing Dynamic Genesis Block Generation ===" << std::endl;
    
    // The test uses the new Genesis block generator.
    GenesisConfig::GenesisBlockGenerator &generator = GenesisConfig::GenesisBlockGenerator::GetInstance();

    // Check if the network configuration has been loaded.
    const GenesisConfig::GenesisConfigManager& configManager = GenesisConfig::GenesisConfigManager::GetInstance();
    if (!configManager.IsConfigLoaded())
    {
        std::cout << "Network configuration not loaded! Loading default dev config..." << std::endl;
        GenesisConfig::GenesisConfigManager& mutableConfig = const_cast<GenesisConfig::GenesisConfigManager&>(configManager);
        if (!mutableConfig.LoadBuiltinConfig("dev"))
        {
            std::cout << "Failed to load default configuration!" << std::endl;
            return -1;
        }
    }
    
    // Display current configuration information
    const auto& networkInfo = configManager.GetNetworkInfo();
    const auto& genesisInfo = configManager.GetGenesisInfo();
    
    std::cout << "Network Type: " << networkInfo.type << std::endl;
    std::cout << "Network Name: " << networkInfo.name << std::endl;
    std::cout << "Genesis Account: " << genesisInfo.initAccountAddr << std::endl;
    std::cout << "Genesis Time: " << genesisInfo.genesisTime << std::endl;
    std::cout << "Asset Types:" << std::endl;
    for (const auto& balance : genesisInfo.initialBalance)
    {
        std::cout << "  " << balance.first << ": " << balance.second << std::endl;
    }
    
    // Generate the genesis block
    CBlock block;
    if (!generator.GenerateGenesisBlock(block))
    {
        std::cout << "Failed to generate genesis block!" << std::endl;
        return -2;
    }
    
    // Validate the genesis block
    if (!generator.ValidateGenesisBlock(block))
    {
        std::cout << "Genesis block validation failed!" << std::endl;
        return -3;
    }
    
    std::cout << "Genesis block generated and validated successfully!" << std::endl;
    std::cout << "Block Hash: " << block.hash() << std::endl;
    std::cout << "Block Height: " << block.height() << std::endl;
    std::cout << "Block Time: " << block.time() << std::endl;
    std::cout << "Transaction Count: " << block.txs_size() << std::endl;
    
    if (block.txs_size() > 0)
    {
        const CTransaction& tx = block.txs(0);
        std::cout << "Genesis Transaction Hash: " << tx.hash() << std::endl;
        std::cout << "Genesis Transaction UTXO Count: " << tx.utxos_size() << std::endl;
        
        for (int i = 0; i < tx.utxos_size(); ++i)
        {
            const CTxUtxos& utxo = tx.utxos(i);
            std::cout << "  UTXO " << i << " Asset Type: " << utxo.assettype() << std::endl;
            if (utxo.vout_size() > 0)
            {
                std::cout << "  UTXO " << i << " Value: " << utxo.vout(0).value() << std::endl;
                std::cout << "  UTXO " << i << " Address: " << utxo.vout(0).addr() << std::endl;
            }
        }
    }
    
    // Generate a hexadecimal string
    std::string hex = Str2Hex(block.SerializeAsString());
    std::cout << std::endl << "Genesis Block Raw Data:" << std::endl;
    std::cout << hex << std::endl;

    return 0;
}

void MultiAccountTransactionEntity()
{
    std::vector<std::string> Accouncstr;
    Account account;
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(Accouncstr);
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(account);
    std::string strAccount = account.GetAddr();
    auto it = std::remove(Accouncstr.begin(), Accouncstr.end(), strAccount);
    Accouncstr.erase(it, Accouncstr.end());
    
    std::string _isGastrade = "0";
    std::pair<std::string, std::string> gasTrade = {};
    std::vector<std::string> assetType = {};
    
    int i = -1; 
    while (true) {
        std::cout << "1. Transfer Vote assets" << std::endl;
        std::cout << "2. Transfer MM assets" << std::endl;
        std::cout << ">>>";
        std::cin >> i;

        if (i == 1) {
            assetType.push_back(global::ca::ASSET_TYPE_VOTE);
            break; 
        } 
        else if (i == 2) {
            std::string assetTypeS;
            int ret = ca_algorithm::GetCanBeRevokeAssetType(assetTypeS);
            if (ret != 0) {
                ERRORLOG("Get Can Be Revoke AssetType fail!");
                return; 
            }
            assetType.push_back(assetTypeS);
            break;
        } 
        else {
            std::cout << "Error! Please enter 1 or 0." << std::endl; 
        }
    }

    uint64_t amt = 0;
    std::cout << "input amount>:";
    std::cin >> amt;

    uint64_t top = 0;
    DBReader dbReader;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }
    std::mutex batchMutex;
    size_t batchSize = 0;
    std::set<std::string> batch;
    for (const auto &addr : Accouncstr)
    {
        batch.insert(addr);
        batchSize++;
        if (batchSize >= MAX_BATCH_SIZE)
        {
            {
                ProcessBatch(batch, amt, assetType, gasTrade, false, top, strAccount);
            }
            batch.clear();
            batchSize = 0;
        }
    }

    // Process any remaining addresses
    if (!batch.empty())
    {
        ProcessBatch(batch, amt, assetType, gasTrade, false, top, strAccount);
    }
}

void get_all_pledge_addresses()
{
    DBReader reader;
    std::vector<std::string> addressVec;
    reader.getStakeAddr(addressVec);

    auto allPledgeOutputs = [addressVec](std::ostream &stream)
    {
        stream << std::endl
               << "---- Pledged address start ----" << std::endl;
        for (auto &addr : addressVec)
        {
            uint64_t pledgeamount = 0;
            SearchStake(addr, pledgeamount, global::ca::StakeType::STAKE_TYPE_NODE);
            stream << addHexPrefix(addr) << " : " << pledgeamount << std::endl;
        }
        stream << "---- Number of pledged addresses:" << addressVec.size() << " ----" << std::endl
               << std::endl;
        stream << "---- Pledged address end  ----" << std::endl
               << std::endl;
    };

    if (addressVec.size() <= 10)
    {
        allPledgeOutputs(std::cout);
    }
    else
    {
        std::string fileName = "all_pledge.txt";

        std::cout << "output to a file" << fileName << std::endl;

        std::ofstream fileStream;
        fileStream.open(fileName);
        if (!fileStream)
        {
            std::cout << "Open file failed!" << std::endl;
            return;
        }

        allPledgeOutputs(fileStream);

        fileStream.close();
    }
}

void AutoTx()
{
    if (isCreateTx)
    {
        int i = 0;
        std::cout << "1. Close the transaction" << std::endl;
        std::cout << "0. Continue trading" << std::endl;
        std::cout << ">>>" << std::endl;
        std::cin >> i;
        if (i == 1)
        {
            bStopTx = true;
        }
        else if (i == 0)
        {
            return;
        }
        else
        {
            std::cout << "Error!" << std::endl;
            return;
        }
    }
    else
    {
        bStopTx = false;
        std::vector<std::string> addrs;

        MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();
        MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);

        double sleepTime = 0;
        std::cout << "Interval time (seconds):";
        std::cin >> sleepTime;
        sleepTime *= 1000000;
        std::thread th(TestCreateTx, addrs, (int)sleepTime);
        th.detach();
        return;
    }
}

void getBlockInfoByTxHash()
{
    DBReader reader;

    std::cout << "Tx Hash : ";
    std::string txHash;
    std::cin >> txHash;

    std::string blockHash;
    reader.getBlockHashByTransactionHash(txHash, blockHash);

    if (blockHash.empty())
    {
        std::cout << RED << "Error : getBlockHashByTransactionHash failed !" << RESET << std::endl;
        return;
    }

    std::string blockStr;
    reader.getBlockByBlockHash(blockHash, blockStr);
    CBlock block;
    block.ParseFromString(blockStr);

    std::cout << GREEN << "Block Hash : " << blockHash << RESET << std::endl;
    std::cout << GREEN << "Block height : " << block.height() << RESET << std::endl;
}

void GetTxHashByHeight(int64_t start,int64_t end,std::ofstream& filestream)
{
    int64_t localStart = start;
    int64_t localEnd = end;

    if (localEnd < localStart)
    {
        std::cout << "input invalid" << std::endl;
        return;
    }
    
    if (!filestream)
    {
        std::cout << "Open file failed!" << std::endl;
        return;
    }
    filestream << "TPS_INFO:" << std::endl;
    DBReader dbReader;
    uint64_t txTotal = 0;
    uint64_t blockTotal = 0;
    for (int64_t i = localEnd; i >= localStart; --i)
    {

        std::vector<std::string> block_hashes_temp;
        if (DBStatus::DB_SUCCESS != dbReader.getBlockHashsByBlockHeight(i, block_hashes_temp))
        {
            ERRORLOG("(GetTxHashByHeight) getBlockHashsByBlockHeight  Failed!!");
            return;
        }

        int txHashCounter = 0;
        for (auto &blockhash : block_hashes_temp)
        {
            std::string blockstr;
            dbReader.getBlockByBlockHash(blockhash, blockstr);
            CBlock block;
            block.ParseFromString(blockstr);
            txHashCounter += block.txs_size();
        }
        txTotal += txHashCounter;
        blockTotal += block_hashes_temp.size();
        filestream  << "Height >: " << i << " Blocks >: " << block_hashes_temp.size() << " Txs >: " << txHashCounter  << std::endl;
        for(auto &blockhash : block_hashes_temp)
        {
            std::string blockstr;
            dbReader.getBlockByBlockHash(blockhash, blockstr);
            CBlock block;
            block.ParseFromString(blockstr);
            std::string block_hash_temporary = block.hash();
            block_hash_temporary = block_hash_temporary.substr(0,6);
            int hash_size_temp = block.txs_size();
            filestream << " BlockHash: " << block_hash_temporary << " TxHashSize: " << hash_size_temp << std::endl;
        }
    }

    filestream  << "Total block sum >:" << blockTotal  << std::endl;
    filestream  << "Total tx sum >:" << txTotal   << std::endl;
    std::vector<std::string> startHashes;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockHashsByBlockHeight(localStart, startHashes))
    {
        ERRORLOG("getBlockHashsByBlockHeight fail  top = {} ", localStart);
        return;
    }

    //Take out the blocks at the starting height and sort them from the smallest to the largest in time
    std::vector<CBlock> startBlocks;
    for (auto &hash : startHashes)
    {
        std::string blockStr;
        dbReader.getBlockByBlockHash(hash, blockStr);
        CBlock block;
        block.ParseFromString(blockStr);
        startBlocks.push_back(block);
    }
    std::sort(startBlocks.begin(), startBlocks.end(), [](const CBlock &x, const CBlock &y)
              { return x.time() < y.time(); });

    std::vector<std::string> endHashes;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockHashsByBlockHeight(localEnd, endHashes))
    {
        ERRORLOG("getBlockHashsByBlockHeight fail  top = {} ", localEnd);
        return;
    }

    //Take out the blocks at the end height and sort them from small to large in time
    std::vector<CBlock> endBlocks;
    for (auto &hash : endHashes)
    {
        std::string blockStr;
        dbReader.getBlockByBlockHash(hash, blockStr);
        CBlock block;
        block.ParseFromString(blockStr);
        endBlocks.push_back(block);
    }
    std::sort(endBlocks.begin(), endBlocks.end(), [](const CBlock &x, const CBlock &y)
              { return x.time() < y.time(); });

    float timeDiff = 0;
    if (endBlocks[endBlocks.size() - 1].time() - startBlocks[0].time() != 0)
    {
        timeDiff = float(endBlocks[endBlocks.size() - 1].time() - startBlocks[0].time()) / float(1000000);
    }
    else
    {
        timeDiff = 1;
    }
    uint64_t transmissionCount = txTotal ;
    float tps = float(transmissionCount) / float(timeDiff);
    filestream << "TPS : " << tps << std::endl;
}




void TpsCount(){
    int64_t start = 0;
    int64_t end = 0;
    std::cout << "Please input start height:";
    std::cin >> start;

    std::cout << "Please input end height:";
    std::cin >> end;

    if (end < start)
    {
        std::cout << "input invalid" << std::endl;
        return;
    }
    std::string StartW = std::to_string(start);
    std::string EndW = std::to_string(end);
    std::string fileName =  "TPS_INFO_" +StartW +"_"+ EndW+".txt";
    std::ofstream filestream;
    filestream.open(fileName);
    GetTxHashByHeight(start,end,filestream);

     
}

void Get_DelegatingedNodeBlance()
{
    std::string addr;
    std::cout << "Please enter the address you need to inquire: " << std::endl;
    std::cin >> addr;
    if (addr.substr(0, 2) == "0x") 
    {
        addr = addr.substr(2);
    }

    int ret = 0;
    int size = 0;
    std::string errMessage = "success";
    do
    {
        DBReader dbReader;
        std::multimap<std::string, std::string> addresses_assetType;
        auto status = dbReader.getDelegatingAddrByBonusAddr(addr, addresses_assetType);
        if (status != DBStatus::DB_SUCCESS && status != DBStatus::DB_NOT_FOUND)
        {
            errMessage = "Database abnormal, Get delegating addrs by node failed!";
            ret = -1;
            break;
        }
        if (addresses_assetType.size() + 1 > 999)
        {
            errMessage = "The account number to be delegatinged have been delegatinged by 999 people!";
            ret = -2;
            break;
        }

        for (auto &[delegatingAddr, assetType] : addresses_assetType)
        {
            std::vector<std::string> utxos;
            if (dbReader.getBonusAddrDelegatingAddrUtxoByBonusAddr(addr, delegatingAddr, assetType, utxos) != DBStatus::DB_SUCCESS)
            {
                errMessage = "Get bonus addr delegating addr utxo failed!";
                ret = -3;
                break;
            }

            for (const auto &utxo : utxos)
            {
                std::string strTx;
                if (dbReader.getTransactionByHash(utxo, strTx) != DBStatus::DB_SUCCESS)
                {
                    errMessage = "Get transaction failed!";
                    ret = -4;
                    break;
                }

                CTransaction tx;
                if (!tx.ParseFromString(strTx))
                {
                    errMessage = "Failed to parse transaction body!";
                    ret = -5;
                    break;
                }
                for(auto &txUtxo : tx.utxos())
                {
                    for(auto &vout: txUtxo.vout()){
                        if (vout.addr() == global::ca::kVirtualDelegatingAddr)
                        {
                            ++size;
                            std::cout << "addr: " << delegatingAddr << "\tassetType: " << assetType << "\tvalue: " << std::to_string(vout.value()) << std::endl;
                            break;
                        }
                    }
                }
            }
        }
    } while(0);
    
    std::cout << "size: " << size << "\tcode: " << ret << "\tmessage: " <<  errMessage << std::endl;
    return;
}
void printDatabaseBlock()
{
    DBReader dbReader;
    std::string str = PrintBlocks(100, false);
    std::cout << str << std::endl;
}

void ThreadTest::testCreateTransactionMessage(const std::string &from, const std::string &to, bool isAmount, const double amount, std::string asset_type)
{
    std::cout << "from:" << addHexPrefix(from) << std::endl;
    std::cout << "to:" << addHexPrefix(to) << std::endl;

    uint64_t startTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    bool Initiate = false;
    ON_SCOPE_EXIT{
        if(!Initiate)
        {
            MagicSingleton<Benchmark>::GetInstance()->clearTransactionInitiateMap();
        }
    };

    std::string _isGastrade = "0";
    std::pair<std::string,std::string> gasTrade = {};
    if (CheckGasAssetsInput(_isGastrade,gasTrade) != 0)
    {
        return ;
    }

    std::vector<TxHelper::TransTable> txAsset;
    std::string amountStr = "";
    if (isAmount)
    {
        amountStr = std::to_string(amount);
    }else{
        int intPart = 0;
        double decPart = (double)(rand() % 10) / 10000;
        amountStr = std::to_string(intPart + decPart);
    }

    if (amountStr == "0")
    {
        std::cout << "amount = 0" << std::endl;
        DEBUGLOG("amount = 0");
        return;
    }

    if("default" != asset_type)
    {
        TxHelper::TransTable sucTrade3;
        sucTrade3.assetType = asset_type;
        sucTrade3.fromAddr.push_back(from);
        sucTrade3.toAddrAmount[to] = (std::stod(amountStr) + global::ca::MIN_DOUBLE_CONSTANT_PRECISION) * global::ca::kDecimalNum;
        txAsset.push_back(sucTrade3);
    }
    else
    {
        TxHelper::TransTable sucTrade3;
        sucTrade3.assetType = global::ca::ASSET_TYPE_VOTE;
        sucTrade3.fromAddr.push_back(from);
        sucTrade3.toAddrAmount[to] = (std::stod(amountStr) + global::ca::MIN_DOUBLE_CONSTANT_PRECISION) * global::ca::kDecimalNum;
        txAsset.push_back(sucTrade3);
        std::string assetType;
        int ret = ca_algorithm::GetCanBeRevokeAssetType(assetType);
        if(ret != 0){
            ERRORLOG("Get Can BeRevoke AssetType fail!");
            return;
        }
    
        TxHelper::TransTable sucTrade1;
        sucTrade1.assetType = assetType;
        sucTrade1.fromAddr.push_back(from);
        sucTrade1.toAddrAmount[to] = (std::stod(amountStr) + global::ca::MIN_DOUBLE_CONSTANT_PRECISION) * global::ca::kDecimalNum;
        txAsset.push_back(sucTrade1);
    }

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }
    
    
    CTransaction outTx;
    TxHelper::vrfAgentType needAgent;
    std::string encodedInfo = "";
    int ret = TxHelper::createTransactionRequest(txAsset, gasTrade, std::stoi(_isGastrade), encodedInfo,top + 1, outTx, needAgent,false);
    if (ret != 0)
    {
        ERRORLOG("createTransactionRequest error!! ret:{}", ret);
        return;
    }
    
    TxMsgReq txMsg;
    txMsg.set_version(global::GetVersion());
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t txUtxoLocalHeight;
    ret = TxHelper::get_tx_utxo_height(outTx, txUtxoLocalHeight);
    if(ret != 0)
    {
        ERRORLOG("get_tx_utxo_height fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(txUtxoLocalHeight);

    auto msg = std::make_shared<TxMsgReq>(txMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if (outTx.identity() == defaultAddr)
    {
        MagicSingleton<BlockMonitor>::GetInstance()->addDoHandleTransactionVector(outTx.hash());
        ret = handleTransaction(msg,outTx);
    }
    else
    {
        MagicSingleton<BlockMonitor>::GetInstance()->dropshippingTxVec(outTx.hash());
        ret = DropShippingTransaction(msg, outTx, outTx.identity());
    }
    
    global::ca::TxNumber++;
    DEBUGLOG("Transaction result,ret:{}  txHash:{}, TxNumber:{}, package:{}", ret, outTx.hash(), global::ca::TxNumber, outTx.identity());
    Initiate = true;
    MagicSingleton<Benchmark>::GetInstance()->addTransactionInitiateMap(startTime, MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
    
    std::cout << "=====Transaction initiator:" << addHexPrefix(from) << std::endl;
    std::cout << "=====Transaction recipient:" << addHexPrefix(to) << std::endl;
    std::cout << "=====Transaction amount:" << amountStr << std::endl;
    std::cout << "=======================================================================" << std::endl
              << std::endl
              << std::endl;
}

bool stopTx2 = true;
bool isCreateTransaction_2 = false;
static int i = -1;

void ThreadTest::setStopTransmissionFlag(const bool &flag)
{
    stopTx2 = flag;
}

void ThreadTest::stopTxFlag(bool &flag)
{
   flag =  stopTx2 ;
}

void ThreadTest::TestCreateTx(uint32_t tranNum, std::vector<std::string> addrs_,int timeout, std::string asset_type)
{
    DEBUGLOG("TestCreateTx start at {}", MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
    Cycliclist<std::string> addrs;

    for (auto &U : addrs_)
    {
        addrs.push_back(U);
    }

    if(addrs.isEmpty())
    {
        std::cout << "account list is empty" << std::endl;
        return;
    }
    auto iter=addrs.begin();
    while (stopTx2==false)
    {
        MagicSingleton<Benchmark>::GetInstance()->transactionInitiateBatchSize(tranNum);
        for (int i = 0; i < tranNum; i++)
        {
           
            std::string from = iter->data;
            iter++;
            std::string to = iter->data;
            std::thread th(ThreadTest::testCreateTransactionMessage, from, to,false,0, asset_type);
            th.detach();
            
        }
        sleep(timeout);
    }
}

void CreateNodeAutomaticTransferTransaction()
{
    std::vector<Node> pubNodeList_ =
        MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::vector<std::string> Accouncstr;
    for (auto &s : pubNodeList_)
    {
        Accouncstr.push_back(s.address);
    }
    Account account;
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(account);
    std::string strAccount = account.GetAddr();
    auto it = std::remove(Accouncstr.begin(), Accouncstr.end(), strAccount);
    Accouncstr.erase(it, Accouncstr.end());
    uint64_t amt = 0;
    std::cout << "input amount>:";
    std::cin >> amt;

    std::string _isGastrade = "0";
    std::pair<std::string, std::string> gasTrade = {};
    std::string assetTypeS;
    int ret = ca_algorithm::GetCanBeRevokeAssetType(assetTypeS);
    if(ret != 0){
        ERRORLOG("Get Can BeRevoke AssetType fail!");\
        return;
    }
    
    std::vector<std::string> assetType = {assetTypeS};

    uint64_t top = 0;
    DBReader dbReader;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    std::mutex batchMutex;
    size_t batchSize = 0;
    std::set<std::string> batch;
    for (const auto &addr : Accouncstr)
    {
        batch.insert(addr);
        batchSize++;
        if (batchSize >= MAX_BATCH_SIZE)
        {
            {
                ProcessBatch(batch, amt, assetType, gasTrade, false, top, strAccount);
            }
            batch.clear();
            batchSize = 0;
        }
    }

    // Process any remaining addresses
    if (!batch.empty())
    {
        ProcessBatch(batch, amt, assetType, gasTrade, false, top, strAccount);
    }
}

void createMultiThreadAutoTransaction()
{
    std::cout << "1. tx " << std::endl;
    std::cout << "2. close" << std::endl;

    int check=0;
     std::cout << "chose:" ;
     std::cin >> check;

     if(check==1){
       if(stopTx2==true){

            stopTx2=false;
       }else {

            std::cout << "has run" << std::endl;
            return;
       }
     }else if(check ==2){
        stopTx2=true;
        return;
     }else{
        std::cout<< " invalui" << std::endl;
        return;
     }
     if(stopTx2)
     {
        return;
     }

    int TxNum = 0;
    int timeout = 0;

    std::cout << "Interval time (seconds):";
    std::cin >> timeout;

    std::cout << "Interval frequency :" ;

    std:: cin >> TxNum;
    std::vector<std::string> addrs;

    std::string asset_type;
    std::cout << "Input assert type (input default send two types of transactions):";
    std::cin >> asset_type;

    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);

    std::thread th(ThreadTest::TestCreateTx,TxNum, addrs, timeout, asset_type);
    th.detach();
}

void test_create_stake(const std::string &from,const std::string & assetType)
{
    TxHelper::stakeType stakeType = TxHelper::stakeType::STAKE_TYPE_NODE;
    uint64_t stakeAmount = 100000  * global::ca::kDecimalNum ;

    DBReader data_reader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != data_reader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }
    CTransaction outTx;
    TxHelper::vrfAgentType needAgent;
    std::vector<TxHelper::Utxo> outVin;  
    std::string encodedInfo = "";
    std::pair<std::string,std::string> gasTrade = {};  
    if (TxHelper::CreateStakingTransaction(from,assetType,stakeAmount, top + 1,  stakeType, outTx, outVin, needAgent, 
                                                gasTrade, false,global::ca::MIN_COMMISSION_RATE, false, encodedInfo) != 0)
    {

        return;
    }
    std::cout << " from: " << addHexPrefix(from) << " amout: " << stakeAmount << std::endl;
    TxMsgReq txMsg;
    txMsg.set_version(global::GetVersion());
    TxMsgInfo * txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t txUtxoLocalHeight;
    auto ret = TxHelper::get_tx_utxo_height(outTx, txUtxoLocalHeight);
    if(ret != 0)
    {
        ERRORLOG("get_tx_utxo_height fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(txUtxoLocalHeight);

    auto msg = std::make_shared<TxMsgReq>(txMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(outTx.identity() == defaultAddr)
    {
        ret = handleTransaction(msg,outTx);
    }
    else
    {
        ret = DropShippingTransaction(msg, outTx, outTx.identity());
    }

    if (ret != 0)
    {
        ret -= 100;
    }
    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}


void createMultiThreadAutoStakeTransaction()
{
    std::vector<std::string> addrs;

    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);
    
    ca_algorithm::GetAllMappedAssets();
    std::string assetType_MM;
    std::cout << "Please enter the type of asset you want to currency type :" << std::endl;
    std::cin >> assetType_MM;
    for (int i = 0; i < addrs.size(); ++i)
    {
        std::thread th(test_create_stake, addrs[i],assetType_MM);
        th.detach();
    }
}

void testCreateDelegatingRequest(const std::string &strFromAddr, const std::string &strToAddr, const std::string &amountStr,const std::string & assetType)
{
    TxHelper::delegateType delegateType = TxHelper::delegateType::kdelegateType_NetLicence;
    uint64_t delegateAmount = std::stod(amountStr) * global::ca::kDecimalNum;

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }


    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    TxHelper::vrfAgentType needAgent;
    std::pair<std::string,std::string> gasTrade = {};  
    std::string encodedInfo = "";

   int ret = TxHelper::CreateDelegatingTransaction(strFromAddr, assetType, strToAddr, delegateAmount, top + 1,
                                                        delegateType, outTx, outVin, needAgent, gasTrade, 0, false, encodedInfo);
	if(ret != 0)
	{
        ERRORLOG("Failed to create Delegating transaction! The error code is:{}", ret);
        return;
    }
    TxMsgReq txMsg;
    txMsg.set_version(global::GetVersion());
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t txUtxoLocalHeight;
    ret = TxHelper::get_tx_utxo_height(outTx, txUtxoLocalHeight);
    if(ret != 0)
    {
        ERRORLOG("get_tx_utxo_height fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(txUtxoLocalHeight);

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();

    if(outTx.identity() == defaultAddr)
    {
        ret = handleTransaction(msg,outTx);
    }
    else
    {
        ret = DropShippingTransaction(msg, outTx, outTx.identity());
    }

    std::cout << "=====Transaction initiator:" << addHexPrefix(strFromAddr) << std::endl;
    std::cout << "=====Transaction recipient:" << addHexPrefix(strToAddr) << std::endl;
    std::cout << "=====Transaction amount:" << amountStr << std::endl;
    std::cout << "=======================================================================" << std::endl
              << std::endl
              << std::endl
              << std::endl;
}

void AutoDelegation()
{

    std::cout << "input aummot: ";
    std::string aummot;
    std::cin >> aummot;
    ca_algorithm::GetAllMappedAssets();
    std::string assetType_MM;
    std::cout << "Please enter the type of asset you want to currency type :" << std::endl;
    std::cin >> assetType_MM;

    std::vector<std::string> addrs;

    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);

    int i = 0;
    while (i < addrs.size())
    {
        std::string from;
        std::string to;
        from = addrs[i];
        if ((i + 1) >= addrs.size())
        {
            i = 0;
        }
        else
        {
            i += 1;
        }

        to = addrs[i];

        if (from != "")
        {
            if (!MagicSingleton<AccountManager>::GetInstance()->IsExist(from))
            {
                DEBUGLOG("Illegal account.");
                return;
            }
        }
        else
        {
            DEBUGLOG("Illegal account. from addr is null !");
            return;
        }
        std::thread th(testCreateDelegatingRequest, from, to, aummot,assetType_MM);
        th.detach();
        if (i == 0)
        {
            return;
        }
        sleep(1);
    }
}

void printAndVerifyNode()
{
    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();

    std::vector<Node> resultNode;
    for (const auto &node : nodelist)
    {
        int ret = CheckNodeValidity(node.address);
        if(ret == 0){
            resultNode.push_back(node);
        }
    }

    std::string fileName = "verify_node.txt";
    std::ofstream filestream;
    filestream.open(fileName);
    if (!filestream)
    {
        std::cout << "Open file failed!" << std::endl;
        return;
    }

    filestream << "------------------------------------------------------------------------------------------------------------" << std::endl;
    for (auto &i : resultNode)
    {
        filestream
            << "  addr(" << addHexPrefix(i.address) << ")"
            << std::endl;
    }
    filestream << "------------------------------------------------------------------------------------------------------------" << std::endl;
    filestream << "PeerNode size is: " << resultNode.size() << std::endl;
}

void GetRewardAmount()
{
    int64_t startHeight = 0;
    int64_t endHeight = 0;
    std::string addr;
    std::cout << "Please input start height:";
    std::cin >> startHeight;
    std::cout << "Please input end height:";
    std::cin >> endHeight;
    if(endHeight < startHeight)
    {
        std::cout<< "input invalid" << std::endl;
        return ;
    } 
    std::cout << "Please input the address:";
    std::cin >> addr;
    if (addr.substr(0, 2) == "0x") 
    {
        addr = addr.substr(2);
    }
    
    if(!isValidAddress(addr))
    {
        std::cout<< "Input addr error!" <<std::endl;
        return ; 
    }
    DBReader dbReader;
 
    uint64_t txTotall = 0;
    uint64_t claimAmount=0;
    for(int64_t i = startHeight; i <= endHeight; ++i)
    {
        std::vector<std::string> block_hashs;
        if(DBStatus::DB_SUCCESS != dbReader.getBlockHashsByBlockHeight(i,block_hashs)) 
        {
            ERRORLOG("(GetTxHashByHeight) GETBlockHashsByBlockHeight  Failed!!");
            return;
        }
        std::vector<CBlock> blocks;
        for(auto &blockhash : block_hashs)
        {
            std::string blockstr;
            if(DBStatus::DB_SUCCESS !=   dbReader.getBlockByBlockHash(blockhash,blockstr)) 
            {
            ERRORLOG("(getBlockByBlockHash) getBlockByBlockHash Failed!!");
            return;
            }
            CBlock block;
            block.ParseFromString(blockstr);
            blocks.push_back(block);
        }
            std::sort(blocks.begin(),blocks.end(),[](CBlock &a,CBlock &b){
                return a.time()<b.time();
            });
             
                for(auto & block :blocks)
                {
                   time_t s =(time_t)(block.time()/1000000);
                   struct tm * gmDate;
                   gmDate = localtime(&s);
                   std::cout<< gmDate->tm_year + 1900 << "-" << gmDate->tm_mon + 1 << "-" << gmDate->tm_mday << " "  << gmDate->tm_hour << ":" << gmDate->tm_min << ":" << gmDate->tm_sec << "(" << time << ")"<< std::endl;
                    for(auto tx : block.txs())
                    {
                        if((global::ca::TxType)tx.txtype() == global::ca::TxType::TX_TYPE_BONUS )
                        {
                            std::map< std::string,uint64_t> kmap;
                            try 
                            {
                                nlohmann::json dataJson = nlohmann::json::parse(tx.data());
                                nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();
                                claimAmount = txInfo["BonusAmount"].get<uint64_t>();
                            }
                            catch(...)
                            {
                                ERRORLOG(RED "JSON failed to parse data field!" RESET);
                                
                            } 
                            for(auto & utxo : tx.utxos())   
                            {                   
                                 for(const auto & owner : utxo.owner())
                                { 
                                    if(owner != addr)
                                    {
                                         for(auto &vout : utxo.vout())
                                        { 
                                            if(vout.addr() != owner && vout.addr() != "VirtualBurnGas")
                                            {
                                                kmap[vout.addr()]=vout.value();
                                                txTotall += vout.value();
                                            }
                                        }
                                    }
                                }
                            }
                            for(auto it = kmap.begin(); it != kmap.end();++it)
                            {
                                    std::cout << "reward addr:" << addHexPrefix(it->first) << "reward amount" << it->second <<std::endl;   
                            }
                            if(claimAmount!=0)
                            {
                                std::cout << "self node reward addr:" << addHexPrefix(addr) <<"self node reward amount:" << claimAmount-txTotall; 
                                std::cout << "total reward amount"<< claimAmount;
                            }
                        }
                    }
                }   
        }
}

void TestsHandleDelegating()
{
    std::cout << std::endl
              << std::endl;
    std::cout << "AddrList:" << std::endl;
    MagicSingleton<AccountManager>::GetInstance()->PrintAllAccount();

    Account account;
    MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(account);
    std::string strFromAddr = account.GetAddr();

    std::cout << "Please enter your addr:" << std::endl;
    std::cout << addHexPrefix(strFromAddr) << std::endl;
    if (!isValidAddress(strFromAddr))
    {
        ERRORLOG("Input addr error!");
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    std::cout << "Please enter the addr you want to delegate to:" << std::endl;
    std::cout << addHexPrefix(strFromAddr) << std::endl;
    if (!isValidAddress(strFromAddr))
    {
        ERRORLOG("Input addr error!");
        std::cout << "Input addr error!" << std::endl;
        return;
    }
    ca_algorithm::GetAllMappedAssets();
    std::string assetType_MM;
    std::cout << "Please enter the type of asset you want to currency type :" << std::endl;
    std::cin >> assetType_MM;
    

    std::string strDelegatingFee = "700000";
    std::cout << "Please enter the amount to delegate:" << std::endl;
    std::cout << strDelegatingFee << std::endl;
    std::regex pattern("^\\d+(\\.\\d+)?$");
    if (!std::regex_match(strDelegatingFee, pattern))
    {
        ERRORLOG("Input delegating fee error!");
        std::cout << "Input delegate fee error!" << std::endl;
        return;
    }
    
    TxHelper::delegateType delegateType = TxHelper::delegateType::kdelegateType_NetLicence;
    uint64_t delegateAmount = std::stod(strDelegatingFee) * global::ca::kDecimalNum;

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    TxHelper::vrfAgentType needAgent;
    std::pair<std::string,std::string> gasTrade = {};  
    std::string encodedInfo = "";
    int ret = TxHelper::CreateDelegatingTransaction(strFromAddr, assetType_MM, strFromAddr, delegateAmount, top + 1,  delegateType, outTx, 
                                                        outVin, needAgent, gasTrade, 0, false, encodedInfo);
    if (ret != 0)
    {
        ERRORLOG("Failed to create Delegating transaction! The error code is:{}", ret);
        return;
    }

    TxMsgReq txMsg;
    txMsg.set_version(global::GetVersion());
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t txUtxoLocalHeight;
    ret = TxHelper::get_tx_utxo_height(outTx, txUtxoLocalHeight);
    if(ret != 0)
    {
        ERRORLOG("get_tx_utxo_height fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(txUtxoLocalHeight);

    auto msg = std::make_shared<TxMsgReq>(txMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(outTx.identity() == defaultAddr)
    {
        ret = handleTransaction(msg,outTx);
    }
    else
    {
        ret = DropShippingTransaction(msg, outTx, outTx.identity());
    }
    if (ret != 0)
    {
        ret -= 100;
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}



void TestHandleDelegatingMoreToOne(std::string strFromAddr, std::string strToAddr, std::string strDelegatingFee)
{
    TxHelper::delegateType delegateType = TxHelper::delegateType::kdelegateType_NetLicence;
    uint64_t delegateAmount = std::stod(strDelegatingFee) * global::ca::kDecimalNum;

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    CTransaction outTx;
    std::vector<TxHelper::Utxo> outVin;
    TxHelper::vrfAgentType needAgent;
    std::pair<std::string,std::string> gasTrade = {};  
    std::string encodedInfo = "";
    std::string assetType_MM;
    int ret = ca_algorithm::GetCanBeRevokeAssetType(assetType_MM);
    if (ret != 0)
    {
        ERRORLOG("Get Can BeRevoke AssetType fail!");
    }

    ret = TxHelper::CreateDelegatingTransaction(strFromAddr, assetType_MM, strToAddr, delegateAmount, top + 1,  delegateType, outTx, 
                                                        outVin, needAgent, gasTrade, 0, false, encodedInfo);
    if (ret != 0)
    {
        ERRORLOG("Failed to create Delegating transaction! The error code is:{}", ret);
        return;
    }

    TxMsgReq txMsg;
    txMsg.set_version(global::GetVersion());
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t txUtxoLocalHeight;
    ret = TxHelper::get_tx_utxo_height(outTx, txUtxoLocalHeight);
    if(ret != 0)
    {
        ERRORLOG("get_tx_utxo_height fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(txUtxoLocalHeight);

    auto msg = std::make_shared<TxMsgReq>(txMsg);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if(outTx.identity() == defaultAddr)
    {
        ret = handleTransaction(msg,outTx);
    }
    else
    {
        ret = DropShippingTransaction(msg, outTx, outTx.identity());
    }
    if (ret != 0)
    {
        ret -= 100;
    }

    DEBUGLOG("Transaction result,ret:{}  txHash:{}", ret, outTx.hash());
}

void testManToOneDelegate()
{
    uint32_t num = 0;
    std::cout << "plase inter delegate num: " << std::endl;
    std::cin >> num; 

    std::vector<std::string> _list;
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(_list);

    if(num > _list.size())
    {
        std::cout << "error: Account num < " << num << std::endl;
        return;
    }   

    std::string strToAddr;
    std::cout << "Please enter the addr you want to delegate to:" << std::endl;
    std::cin >> strToAddr;
    if (strToAddr.substr(0, 2) == "0x") 
    {
        strToAddr = strToAddr.substr(2);
    }
    if (!isValidAddress(strToAddr))
    {
        ERRORLOG("Input addr error!");
        std::cout << "Input addr error!" << std::endl;
        return;
    }

    std::string strDelegatingFee;
    std::cout << "Please enter the amount to delegate:" << std::endl;
    std::cin >> strDelegatingFee;
    std::regex pattern("^\\d+(\\.\\d+)?$");
    if (!std::regex_match(strDelegatingFee, pattern))
    {
        ERRORLOG("Input delegating fee error!");
        std::cout << "Input delegate fee error!" << std::endl;
        return;
    }

    DBReadWriter dbReader;
    std::set<std::string> pledgeAddr;

    std::vector<std::string> stakeAddr;
    auto status = dbReader.getStakeAddr(stakeAddr);
    if (DBStatus::DB_SUCCESS != status && DBStatus::DB_NOT_FOUND != status)
    {
        std::cout << "getStakeAddr error" << std::endl;
        return;
    }

    for(const auto& addr : stakeAddr)
    {
        if(VerifyBonusAddr(addr) != 0)
        {
            continue;
        }
        pledgeAddr.insert(addr);
    }

    int successNum = 0;
    int testNum = 0;
    for(int i = 0; successNum != num; ++i)
    {
        std::string fromAddr;
        try
        {
            fromAddr = _list.at(i);
        }
        catch (const std::exception&)
        {
            break;
        }

        if (!isValidAddress(fromAddr))
        {
            ERRORLOG("fromAddr addr error!");
            std::cout << "fromAddr addr error! : " << addHexPrefix(fromAddr) << std::endl;
            continue;
        }

        auto it = pledgeAddr.find(fromAddr);
        if(it != pledgeAddr.end())
        {
            ++testNum;
            continue;
        }

        TestHandleDelegatingMoreToOne(fromAddr, strToAddr, strDelegatingFee);  
        ++successNum;
    }

    std::cout << "testNum: " << testNum << std::endl;
}

void OpenLog()
{
    Config::Log log = {};
    MagicSingleton<Config>::GetInstance()->GetLog(log);
    MagicSingleton<Log>::GetInstance()->LogInit(log.path,log.console,"debug");
}

void CloseLog()
{

    Config::Log log = {};
    MagicSingleton<Config>::GetInstance()->GetLog(log);
    MagicSingleton<Log>::GetInstance()->LogDeinit();
    std::string tmpString = "logs";
    if(std::filesystem::remove_all(tmpString))
    {
        std::cout<< "File deleted successfully" <<std::endl;
    }
    else
    {
        std::cout << "Failed to delete the file"<<std::endl;    
    }
}

void TestSign()
{
    Account account(true);
    std::string vinHashSerialized = Getsha256hash("1231231asdfasdf");
	std::string signature;
	std::string pub;

    if(MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(account) == -1)
    {
        std::cout <<"get account error";
    }
	if (account.Sign(vinHashSerialized, signature) == true)
	{
		std::cout << "tx sign true !" << std::endl;
	}
    if(account.Verify(vinHashSerialized,signature) == true)
    {
        std::cout << "tx verify true" << std::endl;
    }
}

void MultiTransaction()
{
    int addrCount = 0;

    std::vector<TxHelper::TransTable> txAsset;
    TxHelper::TransTable successfulTrade;
    std::string type;
    std::cout << "Please enter the type of asset you want to txAsset :" << std::endl;
    std::cin >> type;
    successfulTrade.assetType = remove0xPrefix(type);

    std::cout << "Number of initiator accounts:";
    std::cin >> addrCount;

    std::vector<std::string> fromAddr;
    for (int i = 0; i < addrCount; ++i)
    {
        std::string addr;
        std::cout << "Initiating account" << i + 1 << ": ";
        std::cin >> addr;
        addr = remove0xPrefix(addr);
        successfulTrade.fromAddr.push_back(addr);
    }

    std::cout << "Number of receiver accounts:";
    std::cin >> addrCount;

    std::map<std::string, int64_t> toAddr;
    for (int i = 0; i < addrCount; ++i)
    {
        std::string addr;
        double amt = 0;
        std::cout << "Receiving account" << i + 1 << ": ";
        std::cin >> addr;
        if (addr.substr(0, 2) == "0x")
        {
            addr = addr.substr(2);
        }
        std::cout << "amount : ";
        std::cin >> amt;
        successfulTrade.toAddrAmount.insert(make_pair(remove0xPrefix(addr), amt * global::ca::kDecimalNum));
    }

    txAsset.push_back(successfulTrade);

    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    TxMsgReq txMsg;
    TxHelper::vrfAgentType needAgent;
    CTransaction outTx;
    std::pair<std::string, std::string> gasTrade = {};
    std::string encodedInfo = "";
    int ret = TxHelper::createTransactionRequest(txAsset, gasTrade, 0, encodedInfo, top + 1, outTx, needAgent, false);
    if (ret != 0)
    {
        ERRORLOG("createTransactionRequest error!!");
        return;
    }
    uint64_t txUtxoHeight;
    ret = TxHelper::get_tx_utxo_height(outTx, txUtxoHeight);

    if (ret != 0)
    {
        ERRORLOG("get_tx_utxo_height fail!!! ret = {}", ret);
        return;
    }

    txMsg.set_version(global::GetVersion());
    TxMsgInfo *txMsgInfo = txMsg.mutable_txmsginfo();
    txMsgInfo->set_type(0);
    txMsgInfo->set_tx(outTx.SerializeAsString());
    txMsgInfo->set_nodeheight(top);

    uint64_t txUtxoLocalHeight;
    ret = TxHelper::get_tx_utxo_height(outTx, txUtxoLocalHeight);
    if (ret != 0)
    {
        ERRORLOG("get_tx_utxo_height fail!!! ret = {}", ret);
        return;
    }

    txMsgInfo->set_txutxoheight(txUtxoLocalHeight);

    auto msg = std::make_shared<TxMsgReq>(txMsg);

    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if (outTx.identity() == defaultAddr)
    {
        ret = handleTransaction(msg, outTx);
    }
    else
    {
        ret = DropShippingTransaction(msg, outTx, outTx.identity());
    }
    DEBUGLOG("Transaction result, ret:{}  txHash: {}", ret, outTx.hash());
}
