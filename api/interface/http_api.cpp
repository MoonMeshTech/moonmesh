#include "./http_api.h"
#include "./rpc_tx.h"
#include <netdb.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <sys/types.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <cctype>
#include <ctime>
#include <exception>
#include <functional>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "utils/base64.h"
#include "api/rpc_error.h"
#include "api/interface/rpc_tx.h"
#include <boost/math/constants/constants.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include "ca/ca.h"
#include "ca/advanced_menu.h"
#include "ca/global.h"
#include "ca/transaction.h"
#include "ca/txhelper.h"
#include "ca/double_spend_cache.h"
#include "ca/contract_transaction_cache.h"
#include "ca/dispatchtx.h"

#include "common/global.h"
#include "db/db_api.h"
#include "interface.pb.h"
#include "logging.h"
#include "utils/account_manager.h"
#include "utils/magic_singleton.h"
#include "utils/tmp_log.h"
#include "net/test.hpp"
#include "ca/test.h"
#include <utils/contract_utils.h>
#include <google/protobuf/util/json_util.h>
#include "google/protobuf/stubs/status.h"
#include "include/scope_guard.h"
#include "net/interface.h"
#include "net/global.h"
#include "net/httplib.h"
#include "net/api.h"
#include "net/peer_node.h"
#include <nlohmann/json.hpp>
#include "utils/string_util.h"
#include "net/unregister_node.h"
#include "ca/algorithm.h"
#include "./rpc_tx.h"
#include "block.pb.h"
#include "ca_protomsg.pb.h"
#include "transaction.pb.h"
#include "utils/envelop.h"
#include "ca/interface.h"
#include "ca/vrf_consensus.h"
#include "rpc_create_transaction.h"
#include "ca/block_helper.h"
#include "common/global_data.h"

#define VALIDATE_PARSINREQUEST                                             \
    std::string ParseRet = req_t._parseFromJson(req.body);            \
    if (ParseRet != "OK")                                            \
    {                                                               \
        errorL("bad error parse fail");                              \
        ack_t.code = 9090;                                          \
        ack_t.message = Sutil::Format("parse fail:%s", ParseRet);     \
        res.set_content(ack_t._parseToString(), "application/json"); \
        return;                                                     \
    }

#define CHECK_VALUE(value)\
        std::regex pattern("(^[1-9]\\d*\\.\\d+$|^0\\.\\d+$|^[1-9]\\d*$|^0$)");\
        if (!std::regex_match(value, pattern))\
        {\
            ack_t.code=-3004;\
            ack_t.message=Sutil::Format("input value error:",value);\
            res.set_content(ack_t._parseToString(), "application/json");\
            return;\
        }


// base64 picture
extern std::string MOONMESHCHAIN_LOGO_BASE64 = "";

void subscribeToHttpCallbacks() {
    HttpServer::RegisterCallback("/", apiJsonRpcRequest);
    HttpServer::RegisterCallback("/GetPublicIp", getRequesterIp);
    HttpServer::RegisterCallback("/GetTxInfo", getTransactionInfo);
    HttpServer::RegisterCallback("/GetUndelegatingUtxo", _GetUndelegatingUtxo);
    HttpServer::RegisterCallback("/GetStakingUtxo", retrieveStakeUtxo);
    HttpServer::RegisterCallback("/GetLockUtxo",_GetLockUtxo);
    HttpServer::RegisterCallback("/getAllAssetType",_ApiGetAssetType);
    HttpServer::RegisterCallback("/GetAssetTypeInfo",_ApiGetAssetTypeInfo);
    HttpServer::RegisterCallback("/GetVoteAddrs",_ApiGetVoteAddrsByHash);
    HttpServer::RegisterCallback("/GetVoteTxHash",_ApiGetVoteTxHashByAssetType);

    HttpServer::RegisterCallback("/CreateTransaction", _GetTransaction);
    HttpServer::RegisterCallback("/CreateStakingTransaction", _GetStake);
    HttpServer::RegisterCallback("/CreateUnstakingTransaction", getUnstake);
    HttpServer::RegisterCallback("/CreateDelegatingTransaction", _GetDelegating);
    HttpServer::RegisterCallback("/CreateUndelegatingTransaction", retrieveDelegationStatus);
    HttpServer::RegisterCallback("/CreateBonusTransaction", _GetBonus);
    HttpServer::RegisterCallback("/CreateCallContractTransaction", _CallContract);
    HttpServer::RegisterCallback("/CreateDeployContractTransaction", _DeployContract);
    HttpServer::RegisterCallback("/SendMessage", sendApiMessageRequest);
    HttpServer::RegisterCallback("/SendContractMessage", sendMessageRequest);
    HttpServer::RegisterCallback("/ConfirmTransaction",_ConfirmTransaction);
    HttpServer::RegisterCallback("/CreateLockTransaction", _GetLock); 
    HttpServer::RegisterCallback("/CreateUnLockTransaction",_GetUnLock);
    HttpServer::RegisterCallback("/CreateProposalTransaction", _GetProposal);
    HttpServer::RegisterCallback("/CreateRevokeProposalTransaction",_GetRevokeProposal);
    HttpServer::RegisterCallback("/CreateVoteTransaction", _GetVote);
    HttpServer::RegisterCallback("/CreateFundTransaction", _GetFund);

    HttpServer::RegisterCallback("/GetYieldInfo", _ApiGetYieldInfo);
    HttpServer::RegisterCallback("/GetAllStakeNodeList",allStakeNodeList);
    HttpServer::RegisterCallback("/GetBonusInfo",fetchAllRewardDetails);
    HttpServer::RegisterCallback("/GetBlockHeight",_GetBlockHeight);
    HttpServer::RegisterCallback("/GetVersion", _GetVersion);
    HttpServer::RegisterCallback("/GetBalance", _GetBalance);
    HttpServer::RegisterCallback("/GetBlockTransactionCountByHash", _GetBlockTransactionCountByHash);
    HttpServer::RegisterCallback("/GetAccounts", _GetAccounts);
    HttpServer::RegisterCallback("/GetChainId", _GetChainId);
    HttpServer::RegisterCallback("/GetNodeList", _GetPeerList);
    HttpServer::RegisterCallback("/GetAddrType", _GetAddrType);
    HttpServer::RegisterCallback("/getBonusAddrByDelegatingAddr", _GetBonusAddrByDelegatingAddr);

    HttpServer::RegisterCallback("/getTransactionByHash", get_transaction_info);
    HttpServer::RegisterCallback("/GetBlockByTransactionHash", transactionHashBlockRequest);
    HttpServer::RegisterCallback("/GetBlockByHash", get_block_by_hash);
    HttpServer::RegisterCallback("/GetBlockByHeight", fetchBlockInfoByLevel);
    HttpServer::RegisterCallback("/GetDelegatingAddrsByBonusAddr", getDelegateInfoRequest);

    HttpServer::RegisterCallback("/block", apiPrintBlock);
    HttpServer::RegisterCallback("/get_block", getBlockRequest);
    HttpServer::RegisterCallback("/pub", _ApiPub);
    HttpServer::RegisterCallback("/account", _ApiAccount);
    HttpServer::RegisterCallback("/Node",_ApiNode);
    HttpServer::RegisterCallback("/ShowVRFInfo", _ShowValidatedVRFs);
    HttpServer::RegisterCallback("/ShowNewVRFInfo", _ShowNewValidatedVRFs);
    HttpServer::RegisterCallback("/printCalcHash", _ApiPrintCalc1000SumHash);
    HttpServer::RegisterCallback("/printhundredhash", _ApiPrintHundredSumHash);
    HttpServer::RegisterCallback("/printblock", _ApiPrintAllBlocks);
    HttpServer::RegisterCallback("/SystemInfo", systemInfo);

    //vote ===========================================
    HttpServer::RegisterCallback("/printVoteInfo", _ApiPrintVoteInfo);
    HttpServer::RegisterCallback("/printLockAddrs", _ApiPrintLockAddrs);
    HttpServer::RegisterCallback("/printProposalInfoByHash", _ApiPrintProposalInfo);
    HttpServer::RegisterCallback("/printAvailableAssetType", _ApiPrintAvailableAssetType);
    HttpServer::RegisterCallback("/printAsseTypeByContractAddr", _ApiGetAsseTypeByContractAddr);  
    //===================================================
    HttpServer::Start();
}


void apiJsonRpcRequest(const Request &req, Response &res) 
{
    nlohmann::json ret;
    ret["jsonrpc"] = "2.0";
    try {
        auto json = nlohmann::json::parse(req.body);

        std::string method = json["method"];

        auto p = HttpServer::rpcCbs.find(method);
        if (p == HttpServer::rpcCbs.end()) 
        {
            ret["error"]["code"] = -32601;
            ret["error"]["message"] = "Method not found";
            ret["id"] = "";
        } 
        else 
        {
            auto params = json["params"];
            ret = HttpServer::rpcCbs[method](params);
            try {
                ret["id"] = json["id"].get<int>();
            } 
            catch (const std::exception &e) 
            {
                ret["id"] = json["id"].get<std::string>();
            }
            ret["jsonrpc"] = "2.0";
        }
    } 
    catch (const std::exception &e) 
    {
        ret["error"]["code"] = -32700;
        ret["error"]["message"] = "Internal error";
        ret["id"] = "";
    }
    res.set_content(ret.dump(4), "application/json");
}


void getTransactionInfo(const Request &req, Response &res) 
{
    
    get_tx_info_req req_t;
    get_tx_info_ack ack_t;
    VALIDATE_PARSINREQUEST

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "getTxInfo";
    DBReader dbReader;
    std::string BlockHash;
    std::string strHeader;
    unsigned int BlockHeight;
    if (DBStatus::DB_SUCCESS !=
        dbReader.getTransactionByHash(req_t.txhash, strHeader)) 
    {
        ack_t.code = -2;
        ack_t.message = "txhash error";
        
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    if (DBStatus::DB_SUCCESS !=
        dbReader.getBlockHashByTransactionHash(req_t.txhash, BlockHash)) 
    {
        ack_t.code = -3;
        ack_t.message = "Block error";
        
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    if (DBStatus::DB_SUCCESS !=
        dbReader.getBlockHeightByBlockHash(BlockHash, BlockHeight)) {
        ack_t.code = -4;
        ack_t.message = "Block error";

        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    CTransaction tx;
    if (!tx.ParseFromString(strHeader)) {
        ack_t.code = -5;
        ack_t.message = "tx ParseFromString error";
        
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    ack_t.code = 0;
    ack_t.message = "success";
    ack_t.tx = transactionInvestment(tx);
    ack_t.blockhash = BlockHash;
    ack_t.blockheight = BlockHeight;
    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetStake(const Request &req, Response &res) 
{
    getStakeReq req_t;
    txAck ack_t;
    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "CreateStakingTransaction";
    ack_t.sleeptime = req_t.sleeptime;

    std::string fromAddr = remove0xPrefix(req_t.fromAddr);

    CHECK_VALUE(req_t.stakeAmount);
    uint64_t stake_amount =
        (std::stod(req_t.stakeAmount) + global::ca::MIN_DOUBLE_CONSTANT_PRECISION) *
        global::ca::kDecimalNum;
    int32_t stakeType = std::stoll(req_t.StakeType);

    std::regex bonus("^(5|6|7|8|9|1[0-9]|20)$"); // 5 - 20 
    if(!std::regex_match(req_t.commissionRate,bonus))
    {
        ack_t.code=-1;
        ack_t.message = "input pumping percentage error:" + req_t.commissionRate;
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
    double commissionRate = std::stod(req_t.commissionRate) / 100;

    bool is_find_utxo_flag = req_t.isFindUtxo;
    std::string encodedInfo = Base64Encode(req_t.txInfo);
    std::pair<std::string,std::string> gasTrade = req_t.gasTrade ;
    gasTrade.first = remove0xPrefix(gasTrade.first);
    gasTrade.second = remove0xPrefix(gasTrade.second);
    bool isGasTrade = req_t.isGasTrade ; 
    

    stakeTransactionUpdater(
        fromAddr, stake_amount, stakeType,gasTrade,isGasTrade,&ack_t, commissionRate, is_find_utxo_flag, encodedInfo);

    res.set_content(ack_t._parseToString(), "application/json");
}

void getUnstake(const Request &req, Response &res) 
{
    getUnStakeReq req_t;
    txAck ack_t;
    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "CreateUnstakingTransaction";
    ack_t.sleeptime = req_t.sleeptime;

    std::string fromAddr = remove0xPrefix(req_t.fromAddr);
    
    std::string utxoHash = remove0xPrefix(req_t.utxoHash);

    bool is_find_utxo_flag = req_t.isFindUtxo;
    std::string encodedInfo = Base64Encode(req_t.txInfo);
    std::pair<std::string,std::string> gasTrade = req_t.gasTrade ;
    gasTrade.first = remove0xPrefix(gasTrade.first);
    gasTrade.second = remove0xPrefix(gasTrade.second);
    bool isGasTrade = req_t.isGasTrade ; 
    

    ReplaceCreateUnstakeTransaction(fromAddr, utxoHash,gasTrade,isGasTrade,is_find_utxo_flag,encodedInfo,&ack_t);

    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetDelegating(const Request &req, Response &res) {
    getDelegateReq req_t;
    txAck ack_t;

    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "CreateDelegatingTransaction";
    ack_t.sleeptime = req_t.sleeptime;

       std::string fromAddr = remove0xPrefix(req_t.fromAddr);
    std::string toAddr = remove0xPrefix(req_t.toAddr);
    std::string assetType = remove0xPrefix(req_t.assetType);
    if (fromAddr.substr(0, 2) == "0x") 
    {
        fromAddr = fromAddr.substr(2);
    }
    
    if (toAddr.substr(0, 2) == "0x") 
    {
        toAddr = toAddr.substr(2);
    }
    CHECK_VALUE(req_t.delegateAmount);
    long double value = std::stold(req_t.delegateAmount) * 10000;
    value = value * 10000;
    uint64_t delegatingAmout =(std::stod(req_t.delegateAmount) + global::ca::MIN_DOUBLE_CONSTANT_PRECISION) *global::ca::kDecimalNum;
    int32_t delegateType = std::stoll(req_t.delegateType);

    bool is_find_utxo_flag = req_t.isFindUtxo;
    std::string encodedInfo = Base64Encode(req_t.txInfo);

    std::pair<std::string,std::string> gasTrade = req_t.gasTrade ;
    gasTrade.first = remove0xPrefix(gasTrade.first);
    gasTrade.second = remove0xPrefix(gasTrade.second);
    bool isGasTrade = req_t.isGasTrade ; 
    ReplaceCreateDelegatingTransaction(
        fromAddr, assetType, toAddr, delegatingAmout, delegateType,gasTrade,isGasTrade,is_find_utxo_flag,encodedInfo,&ack_t);
    
    res.set_content(ack_t._parseToString(), "application/json");
}

void retrieveDelegationStatus(const Request &req, Response &res) 
{
    getUndelegatereq req_t;
    txAck ack_t;

    VALIDATE_PARSINREQUEST    
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "CreateUndelegatingTransaction";
    ack_t.sleeptime = req_t.sleeptime;

    std::string fromAddr = remove0xPrefix(req_t.fromAddr);
    std::string assetType = remove0xPrefix(req_t.assetType);
    std::string toAddr = remove0xPrefix(req_t.toAddr);

    std::string utxoHash = remove0xPrefix(req_t.utxoHash);

    bool is_find_utxo_flag = req_t.isFindUtxo;
    std::string encodedInfo = Base64Encode(req_t.txInfo);

    std::pair<std::string,std::string> gasTrade = req_t.gasTrade ;
    gasTrade.first = remove0xPrefix(gasTrade.first);
    gasTrade.second = remove0xPrefix(gasTrade.second);
    bool isGasTrade = req_t.isGasTrade ; 

    ReplaceCreateUndelegatingTransaction(
        fromAddr, assetType, toAddr, utxoHash,gasTrade,isGasTrade,is_find_utxo_flag,encodedInfo,&ack_t);

    res.set_content(ack_t._parseToString(), "application/json");
}

void _ApiGetYieldInfo(const Request &req, Response &res) 
{
    GetYieldInfoReq req_t;
    GetYieldInfoAck ack_t;
    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetYieldInfo";

    uint64_t curTime =
        MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();

    double annualizedRate = 0.0;

    ca_algorithm::GetAnnualizedRate(curTime, annualizedRate);
    ack_t.annualizedRate = std::to_string(annualizedRate);
    ack_t.code = 0;
    ack_t.message = "success";

    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetBonus(const Request &req, Response &res) 
{
    getBonusReq req_t;
    txAck ack_t;
    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "CreateBonusTransaction";
    ack_t.sleeptime = req_t.sleeptime;
    std::string addr = remove0xPrefix(req_t.addr);

    bool is_find_utxo_flag = req_t.isFindUtxo;
    std::string encodedInfo = Base64Encode(req_t.txInfo);
    req_t.gasTrade.first = remove0xPrefix(req_t.gasTrade.first);
    req_t.gasTrade.second = remove0xPrefix(req_t.gasTrade.second);
    updateTransactionRequest(addr, req_t.assetType, req_t.gasTrade, req_t.isGasTrade, is_find_utxo_flag, encodedInfo, &ack_t);
    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetUndelegatingUtxo(const Request &req, Response &res) {
    get_UndelegateUtxo_ack ack_t;
    get_UndelegateUtxo_req req_t;

    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetUndelegatingUtxo";

    std::string toAddr = remove0xPrefix(req_t.toAddr);

    std::string fromAddr = remove0xPrefix(req_t.fromAddr);

    DBReader dbReader;
    nlohmann::json resultJs;
    std::vector<std::string> utxoList;
    
    auto ret = dbReader.getBonusAddrDelegatingAddrUtxoByBonusAddr(toAddr, fromAddr, remove0xPrefix(req_t.assetType), utxoList);
    if(ret!= DBStatus::DB_SUCCESS)
    {
        ack_t.code = -1;
        ack_t.message = "The address has no Delegating in anyone";
    }


    std::reverse(utxoList.begin(), utxoList.end());

    for(auto &utxo : utxoList)
    {
        resultJs["utxo"].push_back(utxo);
    }
    ack_t.code = 0;
    ack_t.message = "success";
    ack_t.utxos = resultJs;
    res.set_content(ack_t._parseToString(), "application/json");
    DEBUGLOG("http_api.cpp:GetUndelegatingUtxo ack_T.parseToString{}", ack_t._parseToString());
}


void retrieveStakeUtxo(const Request &req, Response &res) {
    get_stakeutxo_ack ack_t;
    get_stakeutxo_req req_t;

    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetStakingUtxo";

    std::string strFromAddr =  remove0xPrefix(req_t.fromAddr);

    DBReader dbReader;
    std::vector<std::string> utxos;
    std::string assetType;
    int ret = ca_algorithm::GetCanBeRevokeAssetType(assetType);
    if(ret != 0){
        ack_t.code = -2;
        ack_t.message = "Get CanBeRevoke AssetType fail!";
    }

    ret = dbReader.getStakeAddrUtxo(strFromAddr, assetType, utxos);
    if(ret != DBStatus::DB_SUCCESS)
    {
        ack_t.code = -1;
        ack_t.message = "fromaddr not stake!";
    }

    std::reverse(utxos.begin(), utxos.end());

    nlohmann::json outPut;
    for (auto &utxo : utxos) {
        std::string txRaw;
        dbReader.getTransactionByHash(utxo, txRaw);
        CTransaction tx;
        tx.ParseFromString(txRaw);
        uint64_t value = 0;
        for(const auto & txUtxo : tx.utxos())
        {
            for (auto &vout : txUtxo.vout()) {
                if (vout.addr() == global::ca::kTargetAddress) {
                    value = vout.value();
                }
                outPut[utxo] = value;
            }
        }
    }

    ack_t.code = 0;
    ack_t.message = "success";
    ack_t.utxos = outPut;
    res.set_content(ack_t._parseToString(), "application/json");
    DEBUGLOG("http_api.cpp:GetStakingUtxo ack_T.parseToString{}",ack_t._parseToString());
}


void _GetTransaction(const Request &req, Response &res) 
{
    txAck ack_t;
    tx_req req_t;

    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "CreateTransaction";
    ack_t.sleeptime = req_t.sleeptime;

    for (auto &t : req_t.txAsset)
    {
        t.assetType = remove0xPrefix(t.assetType);
        for(auto& fromAddr: t.fromAddr)
        {
            fromAddr = remove0xPrefix(fromAddr);
        }
        std::map<std::string, int64_t> to_addr_amount;
        for(auto& toAddr: t.toAddrAmount)
        {
            to_addr_amount.insert(std::make_pair(remove0xPrefix(toAddr.first), toAddr.second));
        }
        t.toAddrAmount = std::move(to_addr_amount);
    } 

    bool is_find_utxo_flag = req_t.isFindUtxo;
    std::string encodedInfo = Base64Encode(req_t.txInfo);
    req_t.gasTrade.first = remove0xPrefix(req_t.gasTrade.first);
    req_t.gasTrade.second = remove0xPrefix(req_t.gasTrade.second);

    createOrReplaceTransaction(req_t.txAsset,req_t.gasTrade,req_t.isGasTrade,is_find_utxo_flag,encodedInfo,&ack_t);
    
    res.set_content(ack_t._parseToString(), "application/json");
}



void _DeployContract(const Request &req, Response &res) {
    
    deploy_contract_req req_t;
    contractAck ack_t;
    VALIDATE_PARSINREQUEST

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "CreateDeployContractTransaction";
    ack_t.sleeptime = req_t.sleeptime;

    ack_t.code = 0;
    ack_t.message = "success";

    std::string ret = deployContractRequest((void *)&req_t, &ack_t);

    if (ack_t.code == -2300)
    {
        auto rpcError=GetRpcError();
        ack_t.code = std::atoi(rpcError.first.c_str());
        ack_t.message = rpcError.second;
    }
    
    
    res.set_content(ack_t._parseToString(), "application/json");
}



void _CallContract(const Request &req, Response &res) 
{
    RpcErrorClear();
    call_contract_req req_t;
    contractAck ack_t;
    VALIDATE_PARSINREQUEST;

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "CreateCallContractTransaction";
    ack_t.sleeptime = req_t.sleeptime;

    ack_t.code = 0;
    ack_t.message = "success";

    std::string ret = rpcCallRequest((void *)&req_t, &ack_t);

    if(ack_t.code == -2300)
    {
        auto rpcError=GetRpcError();
        ack_t.message = rpcError.second;
        ack_t.code = std::atoi(rpcError.first.c_str());
    }

    res.set_content(ack_t._parseToString(), "application/json");
}



void fetchAllRewardDetails(const Request &req,Response &res)
{
    getAllbonusInfoReq req_t;
    getAllbonusInfoAck ack_t;
    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetBonusInfo";

    nlohmann::json address_count_time;
    std::map<std::string, double> addr_percent;
    std::unordered_map<std::string, uint64_t> addrSignCount;
    uint64_t curTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    uint64_t morningTime = MagicSingleton<TimeUtil>::GetInstance()->GetMorningTime(curTime)*1000000;

    auto ret = ca_algorithm::fetchAbnormalSignAddrListByPeriod(curTime, addr_percent, addrSignCount);
    if(ret < 0) 
    {   
        ack_t.code = -1;
        ack_t.message = "DB get sign addr failed!";
        ERRORLOG("DB get sign addr failed!");
    }   

    address_count_time["time"] = std::to_string(morningTime); 
    for(auto &it : addrSignCount)
    {
        nlohmann::json addr_count;  
        addr_count["address"] = addHexPrefix(it.first);
        addr_count["count"] = it.second;
        address_count_time["addr_count"].push_back(addr_count);
    }

    ack_t.code = 0;
    ack_t.message = "message";
    ack_t.info = address_count_time;
    res.set_content(ack_t._parseToString(), "application/json");
}


void allStakeNodeList(const Request & req,Response & res){
 
    get_all_stake_node_list_req req_t;
    get_all_stake_node_list_ack ack_t;
    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetAllStakeNodeList";

    std::shared_ptr<GetAllStakeNodeListReq> p_req;
    GetAllStakeNodeListAck  p_ack;
    int ret = stakeNodeListRequestImpl(p_req, p_ack);
    if(ret!=0){
        p_ack.set_code(ret);
    }

    std::string jsonstr;
    google::protobuf::util::Status status =
        google::protobuf::util::MessageToJsonString(p_ack, &jsonstr);
       if(!status.ok()){
            errorL("protobuff to json fail");
            jsonstr="protobuff to json fail";
       }
    ack_t.code = p_ack.code();
    ack_t.message = p_ack.message();
    ack_t.list = nlohmann::json::parse(jsonstr);
    res.set_content(ack_t._parseToString(),"application/json");
}


void _ConfirmTransaction(const Request &req, Response &res) 
{
    confirm_transaction_req req_t;
    confirm_transaction_ack ack_t;
    VALIDATE_PARSINREQUEST

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "ConfirmTransaction";
    uint64_t height = std::stoll(req_t.height);
    ConfirmTransactionAck ack;
    std::shared_ptr<ConfirmTransactionReq> req_p = std::make_shared<ConfirmTransactionReq>();
    std::string txHash = req_t.txhash;
    if (txHash.substr(0, 2) == "0x") 
    {
        txHash = txHash.substr(2);
    }

    req_p->add_txhash(txHash);
    req_p->set_version(global::GetVersion());
    req_p->set_height(height);
    auto currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    req_p->set_time(currentTime);

    int ret = 0;
    ret = sendConfirmationTransactionRequest(req_p, ack);
    ack_t.code = ret;
    ack_t.message = "success";
    if(ret != 0)
    {
        ERRORLOG("sussize is empty{}",ret);
        ack_t.code = ret;
        ack_t.message = ack.message();
        res.set_content(ack_t._parseToString(),"application/json");
        return;
    }
    std::string debugValue;
    google::protobuf::util::Status status =
        google::protobuf::util::MessageToJsonString(ack, &debugValue);
     DEBUGLOG("http_api.cpp:ConfirmTransaction ack_t.parseToString {}",debugValue);

   
    auto sus = ack.percentage();
    auto susSize = sus.size();
    if(susSize == 0)
    {
        ERRORLOG("sussize is empty{}",susSize);
        ack_t.message = "susSize node list is empty";
        ack_t.code = -6;
        res.set_content(ack_t._parseToString(),"application/json");
        return;
    }
    std::string received_size = std::to_string(ack.received_size());
    int receivedSize = stoi(received_size);

    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    int sendsize = nodelist.size();
    if(receivedSize < sendsize * 0.5)
    {
      ack_t.code = -7;
      ack_t.message = "The amount received was too small to verify transaction on-chain";
      res.set_content(ack_t._parseToString(),"application/json");
      return;
    }


    auto rate = sus.at(0);
    ack_t.txhash = addHexPrefix(rate.hash());
    ack_t.percent = std::to_string(rate.rate());
    ack_t.receivedsize = std::to_string(ack.received_size());
    ack_t.sendsize = std::to_string(ack.send_size());

    CTransaction tx;
    if (!tx.ParseFromString(ack.tx())) {
        ack_t.code = -8;
        ack_t.message = "tx ParseFromString error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
    ack_t.tx = nlohmann::json::parse(transactionInvestment(tx));

    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetBlockHeight(const Request &req, Response &res){
    getblockheightrReq req_t;
    getblockheightrAck ack_t;
    VALIDATE_PARSINREQUEST
    ack_t.id=req_t.id;
    ack_t.method = "GetBlockHeight";
    ack_t.jsonrpc=req_t.jsonrpc;
    DBReader dbReader;
    uint64_t top = 0;

    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top)) {
        ack_t.code = -1;
        ack_t.message = "getBlockTop error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    ack_t.height = std::to_string(top);
    ack_t.code = 0;
    ack_t.message = "success";
    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetVersion(const Request &req, Response &res) {
    getversionReq req_t;
    getversionAck ack_t;
    VALIDATE_PARSINREQUEST
	ack_t.id=req_t.id;
    ack_t.method="GetVersion";
    ack_t.jsonrpc=req_t.jsonrpc;

    ack_t.clientVersion =global::GetVersion();
    ack_t.netVersion =global::kNetVersion;
    ack_t.configVersion= MagicSingleton<Config>::GetInstance()->GetVersion();
    ack_t.dbVersion=global::GetVersion();
    ack_t.code = 0;
    ack_t.message = "success";
    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetBalance(const Request &req, Response &res) {
    balanceReq req_t;
    balanceAck ack_t;
    VALIDATE_PARSINREQUEST
	ack_t.id=req_t.id;
    ack_t.method="GetBalance";
    ack_t.jsonrpc=req_t.jsonrpc;
    std::string address = req_t.addr;
    std::string assetType = req_t.assetType;
    if (address.substr(0, 2) == "0x") 
    {
        address = address.substr(2);
    }
    assetType = remove0xPrefix(assetType);
    if (!isValidAddress(address))
    {
        ack_t.code = -1;
        ack_t.message = "address is invalid";
        ack_t.addr = addHexPrefix(address);
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    int64_t balance = 0;
    DBReader dbReader;
    auto ret = dbReader.getBalanceByAddr(address, assetType ,balance); 
    if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND) 
    {
        ack_t.addr = addHexPrefix(address);
        ack_t.code = -2;
        ack_t.message = "search balance failed";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
    ack_t.assetType = assetType;
    ack_t.addr = addHexPrefix(address);
    ack_t.balance=std::to_string(balance);
    ack_t.code = 0;
    ack_t.message = "success";
    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetBlockTransactionCountByHash(const Request &req, Response &res){
	getblocktransactioncountReq req_t;
	getblocktransactioncountAck ack_t;
	VALIDATE_PARSINREQUEST
    std::string blockStr;
	DBReader dbReader;
	ack_t.id=req_t.id;
    ack_t.method="GetBlockTransactionCountByHash";
    ack_t.jsonrpc=req_t.jsonrpc;
	std::string blockHash = req_t.blockHash;
    if (blockHash.substr(0, 2) == "0x") 
    {
        blockHash = blockHash.substr(2);
    }
	if (DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(blockHash, blockStr)){
        ack_t.code = -1;
        ack_t.message = "getBlockByBlockHash error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
	CBlock block;
    if (!block.ParseFromString(blockStr))
	{
        ack_t.code = -2;
        ack_t.message = "block parse string fail";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
	}
    uint64_t nums = 0;
    nums=block.txs().size();
    ack_t.txCount=std::to_string(nums);
	ack_t.code = 0;
    ack_t.message = "success";
	res.set_content(ack_t._parseToString(), "application/json");
}

void _GetAccounts(const Request &req, Response &res){
    getaccountsReq req_t;
    getaccountsAck ack_t;

    VALIDATE_PARSINREQUEST
	ack_t.id=req_t.id;
    ack_t.method="GetAccounts";
    ack_t.jsonrpc=req_t.jsonrpc;
    DBReader dbReader;

    std::vector<std::string> list;
    std::vector<std::string> endlist;
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(list);
    auto it = std::find(list.begin(), list.end(), MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr());
    if (it != list.end()) {
        std::rotate(list.begin(), it, it + 1);
    }
      for (auto &i : list) {
        endlist.push_back("0x"+i); 
    }
    ack_t.acccountlist=endlist;
    ack_t.code = 0;
    ack_t.message = "success";
    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetChainId(const Request &req, Response &res){
    getchainidReq req_t;
    getchainidAck ack_t;

    VALIDATE_PARSINREQUEST
	ack_t.id=req_t.id;
    ack_t.method="GetChainId";
    ack_t.jsonrpc=req_t.jsonrpc;

    std::string blockHash;
    DBReader dbReader;
    if(DBStatus::DB_SUCCESS != dbReader.getBlockHashByBlockHeight(0, blockHash))
    {
        ack_t.message = "Get block hash error";
        ack_t.code = -1;
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
    
    blockHash = blockHash.substr(0,8);
    ack_t.chainId= addHexPrefix(blockHash);
    ack_t.code = 0;
    ack_t.message = "success";
    res.set_content(ack_t._parseToString(), "application/json");
}

std::vector<std::string> splitString(const std::string& str) {
    std::vector<std::string> result;
    std::regex rgx(R"(addr\(0x([a-fA-F0-9]+)\)\s+ip\((\d+\.\d+\.\d+\.\d+)\)\s+port\((\d+)\)\s+kind\((\d+)\)\s+fd\((\d+)\)\s+pulse\((\d+)\)\s+height\(\s*(\d+)\s*\)\s+name\(([^)]*)\)\s+version\((\d+_\d+\.\d+\.\d+_[ptd])\)\s+logo\(([^)]*)\))");
    std::sregex_iterator iter(str.begin(), str.end(), rgx);
    std::sregex_iterator end;

    while (iter != end) {
        result.push_back(iter->str());
        ++iter; 
    }

    return result;
}

nlohmann::json parseEntry(const std::string& entry) {
    std::regex rgx(R"(addr\(0x([a-fA-F0-9]+)\)\s+ip\((\d+\.\d+\.\d+\.\d+)\)\s+port\((\d+)\)\s+kind\((\d+)\)\s+fd\((\d+)\)\s+pulse\((\d+)\)\s+height\(\s*(\d+)\s*\)\s+name\(([^)]*)\)\s+version\((\d+_\d+\.\d+\.\d+_[ptd])\)\s+logo\(([^)]*)\))");
    std::smatch match;
    nlohmann::json j;

    if (std::regex_search(entry, match, rgx)) {
        j["addr"] = "0x"+match[1].str();
        j["ip"] = match[2].str();
        j["port"] = match[3].str();
        j["kind"] = match[4].str();
        j["fd"] = match[5].str();
        j["pulse"] = match[6].str();
        j["height"] = match[7].str();
        j["name"] = match[8].str();
        j["version"] = match[9].str();
        j["logo"] =  Base64Encode(match[10].str()); 
    }

    return j;
}


void _GetPeerList(const Request &req, Response &res) 
{
    getpeerlistReq req_t;
    getpeerlistAck ack_t;

    nlohmann::json infoList;
    std::ostringstream oss;
    VALIDATE_PARSINREQUEST
	ack_t.id=req_t.id;
    ack_t.method="GetNodeList";
    ack_t.jsonrpc=req_t.jsonrpc;


    std::vector<std::string> baseList;
    std::vector<Node> nodeList =MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::string strlist=MagicSingleton<PeerNode>::GetInstance()->NodelistInfo(nodeList);

    std::vector<std::string> result = splitString(strlist);
    nlohmann::json j_array =  nlohmann::json::array();
    for (const auto& entry : result) {
        j_array.push_back(parseEntry(entry));
    }
    ack_t.nodeList=j_array;
    ack_t.size=nodeList.size();
    ack_t.code = 0;
    ack_t.message = "success";
    res.set_content(ack_t._parseToString(), "application/json");
}


void get_transaction_info(const Request &req,Response &res)
{
    getTransactionInfoReq req_t;
    getTransactionInfoAck ack_t;
    VALIDATE_PARSINREQUEST

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "getTransactionByHash";

    DBReader dbReader;
	std::string strTx;
	if (DBStatus::DB_SUCCESS != dbReader.getTransactionByHash(remove0xPrefix(req_t.txHash), strTx))
	{
        ack_t.code = -1;
        ack_t.message = "Tx hash error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
	}

	CTransaction tx;
	if (!tx.ParseFromString(strTx))
	{
        ack_t.code = -2;
        ack_t.message = "Failed to parse transaction body";
        res.set_content(ack_t._parseToString(), "application/json");
		return;
	}

    ack_t.code = 0;
    ack_t.message = "success";
    ack_t.tx = nlohmann::json::parse(transactionInvestment(tx));
    
    res.set_content(ack_t._parseToString(), "application/json");
}


void transactionHashBlockRequest(const Request & req, Response & res)
{
    getBlockInfoByTxHashReq req_t;
    getBlockInfoByTxHashAck ack_t;
    VALIDATE_PARSINREQUEST

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetBlockByTransactionHash";

    DBReader dbReader;
    std::string blockHash;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockHashByTransactionHash(remove0xPrefix(req_t.txHash), blockHash))
	{
        ack_t.code = -1;
        ack_t.message = "Tx hash error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
	}

	std::string strBlock;
	if (DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(blockHash, strBlock))
	{
        ack_t.code = -2;
        ack_t.message = "Block hash error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
	}

    nlohmann::json block;
    BlockInvert(strBlock, block);
    if(block.empty())
    {
        ack_t.code = -3;
        ack_t.message = "Block invert error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    ack_t.code = 0;
    ack_t.message = "success";
    ack_t.blockInfo = nlohmann::json::parse(block.dump());
    res.set_content(ack_t._parseToString(), "application/json");
}



void get_block_by_hash(const Request &req,Response &res)
{
    getBlockInfoByHashReq req_t;
    getBlockInfoByHashAck ack_t;
    VALIDATE_PARSINREQUEST

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetBlockByHash";

    DBReader dbReader;
	std::string strBlock;
	if (DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(remove0xPrefix(req_t.blockHash), strBlock))
	{
        ack_t.code = -1;
        ack_t.message = "Block hash error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
	}

    nlohmann::json block;
    BlockInvert(strBlock, block);

    ack_t.code = 0;
    ack_t.message = "success";
    ack_t.blockInfo = nlohmann::json::parse(block.dump());
    res.set_content(ack_t._parseToString(), "application/json");
}


void fetchBlockInfoByLevel(const Request &req,Response &res)
{
    getBlockInfoByHeightReq req_t;
    getBlockInfoByHeightAck ack_t;
    VALIDATE_PARSINREQUEST

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetBlockByHeight";

    DBReader dbReader;
    uint64_t blockHeight;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(blockHeight))
    {
        ack_t.code = -5;
        ack_t.message = "Database abnormal, Get block top error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    uint64_t beginHeight = std::stoull(req_t.beginHeight);
    uint64_t endHeight = std::stoull(req_t.endHeight);
    if(endHeight > blockHeight)
    {
        endHeight = blockHeight;
    }
    if(beginHeight > blockHeight)
    {
        beginHeight = blockHeight;
    }

    if(beginHeight > endHeight)
    {
        ack_t.code = -1;
        ack_t.message = "Block height error, beginHeight < endHeight";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    if(endHeight - beginHeight > 100)
    {
        ack_t.code = -2;
        ack_t.message = "The height of the request does not exceed 100";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    std::vector<std::string> blockHashes;
	if (DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(beginHeight, endHeight, blockHashes))
	{
        ack_t.code = -3;
        ack_t.message = "Database abnormal, Get block hashes by block height error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
	}

    std::string strBlock;
    for(const auto& t : blockHashes)
    {
        if (DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(t, strBlock))
        {
            ack_t.code = -4;
            ack_t.message = "Database abnormal, Get block by block hash error, block hash: " + t;
            res.set_content(ack_t._parseToString(), "application/json");
            ack_t.blocks.clear();
            return;
        }
        
        nlohmann::json block;
        BlockInvert(strBlock, block);
        ack_t.blocks.emplace_back(std::move(nlohmann::json::parse(block.dump())));
    }

    ack_t.code = 0;
    ack_t.message = "success";
    res.set_content(ack_t._parseToString(), "application/json");
}

void sendApiMessageRequest(const Request &req, Response &res) 
{
    rpcAck ack_t;
    txAck req_t;
    VALIDATE_PARSINREQUEST;

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "SendMessage";

    CTransaction tx;
    Vrf info;
    int height;
    TxHelper::vrfAgentType type;
    google::protobuf::util::Status status =
    google::protobuf::util::JsonStringToMessage(req_t.txJson, &tx);
    status = google::protobuf::util::JsonStringToMessage(req_t.vrfJson, &info);

    {
        doubleSpendCache::doubleSpendSuccess used;
        std::string fromaddr = "";
        for(const auto & utxo : tx.utxos()){
            if(utxo.owner_size() != 0){
                fromaddr = utxo.owner(0);
            }
            for(const auto & vin : utxo.vin()){
                for(const auto & prevout : vin.prevout()){
                    used.utxoVector.push_back(utxo.assettype() + "_" + prevout.hash());
                }
            }		
        }

        used.time = tx.time();
        if (MagicSingleton<doubleSpendCache>::GetInstance()->AddFromAddr(std::make_pair(fromaddr, used)))
        {
            ERRORLOG("utxo is using!");
            ack_t.code = -16;
            ack_t.message = "utxo is using!";
            res.set_content(ack_t._parseToString(), "application/json");
            return;
        }
    }


    height = std::stoi(req_t.height);
    type = (TxHelper::vrfAgentType)std::stoi(req_t.txType);
    std::string txHash = Getsha256hash(tx.SerializeAsString());
    ack_t.txHash = addHexPrefix(txHash);
    int ret = SendMessage(tx, height, info, type);
    DEBUGLOG("*** rpc send tx hash: {}", txHash);
    if(ret!= 0)
    {
        ERRORLOG("SendMessage error!{}",ret);
        return;
    }
    if(0 < std::stoi(req_t.sleeptime) && std::stoi(req_t.sleeptime) <= 10){
        std::this_thread::sleep_for(std::chrono::seconds(std::stoi(req_t.sleeptime)));
        ConfirmTransactionAck ack;
        std::shared_ptr<ConfirmTransactionReq> req_p = std::make_shared<ConfirmTransactionReq>();
        req_p->add_txhash(txHash);
        req_p->set_version(global::GetVersion());
        req_p->set_height(height);
        auto currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        req_p->set_time(currentTime);
        ret = sendConfirmationTransactionRequest(req_p, ack);
        if(ret != 0)
        {
            ERRORLOG("sussize is empty{}",ret);
            ack_t.code = ret;
            ack_t.message = ack.message();
            res.set_content(ack_t._parseToString(),"application/json");
            return;
        }

        std::string debugValue;
        google::protobuf::util::Status status =
            google::protobuf::util::MessageToJsonString(ack, &debugValue);
        DEBUGLOG("http_api.cpp:ConfirmTransaction ack_t.parseToString {}",debugValue);

        auto sus = ack.percentage();
        auto susSize = sus.size();
        if(susSize == 0)
        {
            ERRORLOG("sussize is empty{}",susSize);
            ack_t.message = "susSize node list is empty";
            ack_t.code = -6;
            res.set_content(ack_t._parseToString(),"application/json");
            return;
        }
        std::string received_size = std::to_string(ack.received_size());
        int receivedSize = stoi(received_size);

        std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
        int sendsize = nodelist.size();
        if(receivedSize < sendsize * 0.5)
        {
            ack_t.code = -7;
            ack_t.message = "The amount received was too small to verify transaction on-chain";
            res.set_content(ack_t._parseToString(),"application/json");
            return;
        }

        auto rate = sus.at(0);
        ack_t.percent = std::to_string(rate.rate());
        ack_t.receivedsize = std::to_string(ack.received_size());
        ack_t.sendsize = std::to_string(ack.send_size());

        CTransaction tx;
        if (!tx.ParseFromString(ack.tx())) {
            ack_t.code = -8;
            ack_t.message = "tx ParseFromString error";
            res.set_content(ack_t._parseToString(), "application/json");
            return;
        }
        ack_t.tx = transactionInvestment(tx);
    }

    ack_t.code = ret;
    ret == 0 ? ack_t.message = "success" : ack_t.message = "TxHelper::SendMessage error";

    std::string back = ack_t._parseToString();
    res.set_content(back, "application/json");
}

void sendMessageRequest(const Request & req,Response & res){
    contractAck ack_p;
    rpcAck ack_t;
    if(ack_p._parseFromJson(req.body)!="OK"){
        errorL("parse fail");
        return;
    }

    ack_t.id = ack_p.id;
    ack_t.jsonrpc = ack_p.jsonrpc;
    ack_t.method = "SendContractMessage"; 

    ContractTxMsgReq ContractMsg;
    CTransaction tx;
    google::protobuf::util::JsonStringToMessage(ack_p.contractJs, &ContractMsg);
    google::protobuf::util::JsonStringToMessage(ack_p.txJson, &tx);


    {
        doubleSpendCache::doubleSpendSuccess used;
        std::string fromaddr = "";
        std::string assettype = "";
        for(const auto & utxo : tx.utxos()){
            if(utxo.owner_size() != 0){
                fromaddr = utxo.owner(0);
            }
            assettype = utxo.assettype();
            for(const auto & vin : utxo.vin()){
                for(const auto & prevout : vin.prevout()){
                    used.utxoVector.push_back(assettype + "_" + prevout.hash());
                }
            }		
        }

        used.time = tx.time();
        std::string usedKey = fromaddr + assettype;
        if (MagicSingleton<doubleSpendCache>::GetInstance()->AddFromAddr(std::make_pair(usedKey, used)))
        {
            ERRORLOG("utxo is using!");
            ack_t.code = -16;
            ack_t.message = "utxo is using!";
            res.set_content(ack_t._parseToString(), "application/json");
            return;
        }
    }


    std::string txHash = Getsha256hash(tx.SerializeAsString());
    tx.set_hash(txHash);
    DEBUGLOG("*** rpc send contract tx hash: {}", txHash);
    
    ack_t.txHash = addHexPrefix(txHash);
    ack_t.code = 0;
    ack_t.message = "success";

    TxMsgReq txReq= ContractMsg.txmsgreq();
    TxMsgInfo info=txReq.txmsginfo();
    info.set_tx(tx.SerializeAsString());
    txReq.clear_txmsginfo();
    TxMsgInfo *info_p=txReq.mutable_txmsginfo();
    info_p->CopyFrom(info);
    ContractMsg.clear_txmsgreq();
    TxMsgReq * txReq_p=ContractMsg.mutable_txmsgreq();
    txReq_p->CopyFrom(txReq);
    auto msg = std::make_shared<ContractTxMsgReq>(ContractMsg);

    // Check if contract transaction can be sent
    if (!MagicSingleton<DependencyManager>::GetInstance()->CanSendTransaction(ContractMsg))
    {
        // Add to contract transaction cache if not sendable
        int cacheRet = MagicSingleton<ContractTransactionCache>::GetInstance()->AddFailedTransaction(tx, ContractMsg);
        if (cacheRet != 0)
        {
            ERRORLOG("Failed to add contract transaction to cache, ret: {}", cacheRet);
            std::cout << "Failed to cache contract transaction" << std::endl;
            return;
        }
        
        DEBUGLOG("Contract transaction {} added to cache due to sendability check", tx.hash());
        std::cout << "Contract transaction cached for later sending" << std::endl;
        return;
    }

    int ret = dropCallShippingRequest(msg,tx);

    if(0 < std::stoi(ack_p.sleeptime) && std::stoi(ack_p.sleeptime) <= 10){
        std::this_thread::sleep_for(std::chrono::seconds(std::stoi(ack_p.sleeptime)));
        ConfirmTransactionAck ack;
        std::shared_ptr<ConfirmTransactionReq> req_p = std::make_shared<ConfirmTransactionReq>();
        req_p->add_txhash(txHash);
        req_p->set_version(global::GetVersion());
        req_p->set_height(std::stoi(ack_p.height));
        auto currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        req_p->set_time(currentTime);
        int ret = sendConfirmationTransactionRequest(req_p, ack);
        if(ret != 0)
        {
            ERRORLOG("sussize is empty{}",ret);
            ack_t.code = ret;
            ack_t.message = ack.message();
            res.set_content(ack_t._parseToString(),"application/json");
            return;
        }

        std::string debugValue;
        google::protobuf::util::Status status =
            google::protobuf::util::MessageToJsonString(ack, &debugValue);
        DEBUGLOG("http_api.cpp:ConfirmTransaction ack_t.parseToString {}",debugValue);

        auto sus = ack.percentage();
        auto susSize = sus.size();
        if(susSize == 0)
        {
            ERRORLOG("sussize is empty{}",susSize);
            ack_t.message = "susSize node list is empty";
            ack_t.code = -6;
            res.set_content(ack_t._parseToString(),"application/json");
            return;
        }
        std::string received_size = std::to_string(ack.received_size());
        int receivedSize = stoi(received_size);

        std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
        int sendsize = nodelist.size();
        if(receivedSize < sendsize * 0.5)
        {
            ack_t.code = -7;
            ack_t.message = "The amount received was too small to verify transaction on-chain";
            res.set_content(ack_t._parseToString(),"application/json");
            return;
        }

        auto rate = sus.at(0);
        ack_t.percent = std::to_string(rate.rate());
        ack_t.receivedsize = std::to_string(ack.received_size());
        ack_t.sendsize = std::to_string(ack.send_size());

        CTransaction tx;
        if (!tx.ParseFromString(ack.tx())) {
            ack_t.code = -8;
            ack_t.message = "tx ParseFromString error";
            res.set_content(ack_t._parseToString(), "application/json");
            return;
        }
        ack_t.tx = transactionInvestment(tx);
    }

    ack_t.code = ret;
    ret == 0 ? ack_t.message = "success" : ack_t.message = "TxHelper::SendMessage error";

    res.set_content(ack_t._parseToString(), "application/json");
}

void getDelegateInfoRequest(const Request &req, Response &res)
{
    GetDelegateReq req_t;
    GetDelegateAck ack_t;
    VALIDATE_PARSINREQUEST;

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetDelegatingAddrsByBonusAddr";

    std::string addr = req_t.addr;
    if (addr.substr(0, 2) == "0x") 
    {
        addr = addr.substr(2);
    }

    int ret = 0;
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
                ERRORLOG("getBonusAddrDelegatingAddrUtxoByBonusAddr failed!");
                errMessage = "Get bonus addr delegating addr utxo failed!";
                ret = -3;
                break;
            }

            for (const auto &utxo : utxos)
            {
                std::string strTx;
                if (dbReader.getTransactionByHash(utxo, strTx) != DBStatus::DB_SUCCESS)
                {
                    ERRORLOG("getTransactionByHash failed!");
                    errMessage = "Get transaction failed!";
                    ret = -4;
                    break;
                }

                CTransaction tx;
                if (!tx.ParseFromString(strTx))
                {
                    ERRORLOG("Failed to parse transaction body!");
                    errMessage = "Failed to parse transaction body!";
                    ret = -5;
                    break;
                }
                for(auto &txUtxo : tx.utxos())
                {
                    for(auto &vout: txUtxo.vout()){
                        if (vout.addr() == global::ca::kVirtualDelegatingAddr)
                        {
                            ack_t.info[delegatingAddr].emplace_back(std::make_pair(assetType, std::to_string(vout.value())));
                            break;
                        }
                    }
                }
            }
        }
    } while(0);
    
    ack_t.code = ret;
    ack_t.message = errMessage;
    if(ret != 0)
    {
        ack_t.info.clear();
    }
    res.set_content(ack_t._parseToString(), "application/json");
}

void apiPrintBlock(const Request &req, Response &res) 
{
    int num = 100;
    if (req.has_param("num")) {
        num = atol(req.get_param_value("num").c_str());
    }
    int startNum = 0;
    if (req.has_param("height")) {
        startNum = atol(req.get_param_value("height").c_str());
    }
    int hash = 0;
    if (req.has_param("hash")) {
        hash = atol(req.get_param_value("hash").c_str());
    }

    std::string str;
    bool html_format = req.has_param("html") && req.get_param_value("html") == "true";

    if (html_format) {
        str = "<!DOCTYPE html>\n<html>\n<head>\n";
        str += "<meta charset='UTF-8'>\n";
        str += "<title>MoonMesh Block List</title>\n";
        str += "<style>\n";
        str += "body { background: #000; color: #fff; font-family: Arial, sans-serif; margin: 0; padding: 0; }\n";
        str += ".header { display: flex; align-items: center; padding: 30px 0 0 60px; }\n";
        str += ".logo-img { height: 48px; }\n";
        str += ".footer { position: fixed; left: 0; bottom: 0; width: 100%; height: 80px; background: #111; border-top: 2px solid #222; display: flex; align-items: center; z-index: 10; }\n";
        str += ".footer-logo-img { height: 48px; margin-left: auto; margin-right: 20%; }\n";
        str += ".main-title { margin-left: 60px; margin-top: 20px; font-size: 22px; color: #fff; font-weight: 500; text-align: left; font-style: italic; }\n";
        str += ".divider { width: 60%; min-width: 400px; margin: 18px 0 0 60px; border-bottom: 2px solid #222; text-align: left; }\n";
        str += ".block-table { width: 60%; min-width: 400px; margin: 20px 0 80px 60px; border-collapse: separate; border-spacing: 0 10px; text-align: left; }\n";
        str += ".block-table th { font-size: 20px; color: #fff; background: none; border: none; text-align: left; padding-bottom: 10px; font-style: italic; }\n";
        str += ".block-table td { font-size: 22px; color: #fff; background: none; border: none; padding: 6px 0; vertical-align: middle; text-align: left; font-style: normal; }\n";
        str += ".block-table .height { font-weight: bold; color: #bdbdbd; width: 80px; }\n";
        str += ".block-table .blockhash {margin-right: 18px; font-style: normal; }\n";
        str += ".block-table .green { color: #4efcbf; }\n";
        str += ".block-table .blue { color: #4ecbfc; }\n";
        str += ".block-table .vline { display: inline-block; width: 2px; height: 28px; background: #444; margin: 0 8px; vertical-align: middle; border-radius: 2px; }\n";
        str += "</style>\n</head>\n<body>\n";
        // str += "<div class='header'><img class='logo-img' src='/MoonMesh_logo.png' alt='MoonMesh Logo'></div>\n";
        str += "<div class='header'><img class='logo-img' src='" + MOONMESHCHAIN_LOGO_BASE64 + "' alt='MoonMesh Logo'></div>\n";
        str += "<div class='main-title'>Block List</div>\n";
        str += "<div class='divider'></div>\n";
        str += "<table class='block-table'>\n<thead><tr><th style='width:120px;'>Height</th><th></th><th>Blockhash</th></tr></thead>\n<tbody>\n";

        DBReader dbReader;
        uint64_t top = 0;
        if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top)) {
            str += "<tr><td colspan='3'>Get block height failed</td></tr>\n";
        } else {
            int count = 0;
            for (uint64_t i = top; i > 0 && count < num; i--) {
                std::vector<std::string> blockHashs;
                if (dbReader.getBlockHashsByBlockHeight(i, blockHashs) != DBStatus::DB_SUCCESS) {
                    continue;
                }
                if (blockHashs.empty()) {
                    continue;
                }
                str += "<tr>";
                str += "<td class='height'>" + std::to_string(i) + "</td>";
                str += "<td><span class='vline'></span></td>";
                str += "<td>";
                for (size_t j = 0; j < blockHashs.size(); ++j) {
                    std::string strHeader;
                    if (dbReader.getBlockByBlockHash(blockHashs[j], strHeader) != DBStatus::DB_SUCCESS) {
                        continue;
                    }
                    CBlock block;
                    if (!block.ParseFromString(strHeader)) {
                        continue;
                    }
                    // Determine if contract block
                    bool isContractualBlock = false;
                    for (const auto &tx : block.txs()) {
                        if ((global::ca::TxType)tx.txtype() == global::ca::TxType::kTransactionTypeDeploy || 
                            (global::ca::TxType)tx.txtype() == global::ca::TxType::TX_TYPE_INVOKE_CONTRACT) {
                            isContractualBlock = true;
                            break;
                        }
                    }
                    std::string colorClass = isContractualBlock ? "blue" : "green";
                    str += "<span class='blockhash " + colorClass + "'>" + blockHashs[j].substr(0, 8) + "</span> ";
                }
                str += "</td></tr>\n";
                count++;
                if (count >= num) {
                    break;
                }
            }
        }
        str += "</tbody></table>\n";
        str += "<div class='footer'><img class='footer-logo-img' src='" + MOONMESHCHAIN_LOGO_BASE64 + "' alt='MoonMesh Logo'></div>\n";
        str += "</body>\n</html>";
        res.set_content(str, "text/html");
        return;
    }

    DBReader dbReader;
    
    if (hash) {
        if (html_format) {
            // print_blocks_hash's HTML version
            uint64_t top = 0;
            if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top)) {
                str += "<tr><td colspan='2' class='error'>Get block height failed</td></tr>\n";
            } else {
                int count = 0;
                for (uint64_t i = top; i > 0 && count < num; i--) {
                    std::vector<std::string> blockHashs;
                    if (dbReader.getBlockHashsByBlockHeight(i, blockHashs) != DBStatus::DB_SUCCESS) {
                        continue;
                    }

                    if (blockHashs.empty()) {
                        continue;
                    }

                    // Add height cell when displaying the first block hash of each height
                    str += "<tr>\n";
                    str += "<td class='height-cell'>" + std::to_string(i) + "</td>\n";
                    str += "<td><div class='hash-container'>";
                    
                    for (const auto &blockHash : blockHashs) {
                        std::string strHeader;
                        bool isContractualBlock = false;
                        
                        if (dbReader.getBlockByBlockHash(blockHash, strHeader) == DBStatus::DB_SUCCESS) {
                            CBlock block;
                            if (block.ParseFromString(strHeader)) {
                                for (const auto &tx : block.txs()) {
                                    if ((global::ca::TxType)tx.txtype() == global::ca::TxType::kTransactionTypeDeploy || 
                                        (global::ca::TxType)tx.txtype() == global::ca::TxType::TX_TYPE_INVOKE_CONTRACT) {
                                        isContractualBlock = true;
                                        break;
                                    }
                                }
                                
                                str += "<span class='hash-cell ";
                                str += (isContractualBlock ? "contract-block" : "normal-block");
                                str += "'>" + blockHash.substr(0, 8) + "</span>";
                                
                                if (req.has_param("pre_hash_flag")) {
                                    str += "<span class='hash-cell'>(" + block.prevhash().substr(0, 8) + ")</span>";
                                }
                            }
                        }
                    }
                    
                    str += "</div></td>\n";
                    str += "</tr>\n";
                    
                    count++;
                    if (count >= num) {
                        break;
                    }
                }
            }
            
            str += "</tbody>\n</table>\n</div>\n</body>\n</html>";
            res.set_content(str, "text/html");
        } else {
            str = print_blocks_hash(num, req.has_param("pre_hash_flag"));
            res.set_content(str, "text/plain");
        }
        return;
    }

    if (startNum == 0) {
        if (html_format) {
            // DisplayContractSections's HTML version
            uint64_t top = 0;
            if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top)) {
                str += "<tr><td colspan='2' class='error'>Get block height failed</td></tr>\n";
            } else {
                int count = 0;
                for (uint64_t i = top; i > 0 && count < num; i--) {
                    std::vector<std::string> blockHashs;
                    if (dbReader.getBlockHashsByBlockHeight(i, blockHashs) != DBStatus::DB_SUCCESS) {
                        continue;
                    }

                    if (blockHashs.empty()) {
                        continue;
                    }

                    // Add height cell when displaying the first block hash of each height
                    str += "<tr>\n";
                    str += "<td class='height-cell'>" + std::to_string(i) + "</td>\n";
                    str += "<td><div class='hash-container'>";
                    
                    for (const auto &blockHash : blockHashs) {
                        std::string strHeader;
                        if (dbReader.getBlockByBlockHash(blockHash, strHeader) != DBStatus::DB_SUCCESS) {
                            continue;
                        }

                        CBlock block;
                        if (!block.ParseFromString(strHeader)) {
                            continue;
                        }

                        bool isContractualBlock = false;
                        for (const auto &tx : block.txs()) {
                            if ((global::ca::TxType)tx.txtype() == global::ca::TxType::kTransactionTypeDeploy || 
                                (global::ca::TxType)tx.txtype() == global::ca::TxType::TX_TYPE_INVOKE_CONTRACT) {
                                isContractualBlock = true;
                                break;
                            }
                        }

                        str += "<span class='hash-cell ";
                        str += (isContractualBlock ? "contract-block" : "normal-block");
                        str += "'>" + blockHash.substr(0, 8) + "</span>";
                        
                        if (req.has_param("pre_hash_flag")) {
                            str += "<span class='hash-cell'>(" + block.prevhash().substr(0, 8) + ")</span>";
                        }
                    }
                    
                    str += "</div></td>\n";
                    str += "</tr>\n";
                    
                    count++;
                    if (count >= num) {
                        break;
                    }
                }
            }
            
            str += "</tbody>\n</table>\n</div>\n</body>\n</html>";
            res.set_content(str, "text/html");
        } else {
            str = DisplayContractSections(num, req.has_param("pre_hash_flag"));
            res.set_content(str, "text/plain");
        }
    } else {
        if (html_format) {
            // print_range_contract_blocks's HTML version
            uint64_t top = 0;
            if (startNum <= 0) {
                str += "<tr><td colspan='2' class='error'>The starting block height must be greater than 0</td></tr>\n";
            } else if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top)) {
                str += "<tr><td colspan='2' class='error'>Get block height failed</td></tr>\n";
            } else if (startNum > top) {
                str += "<tr><td colspan='2' class='error'>The starting block height exceeds the current highest block</td></tr>\n";
            } else {
                int count = 0;
                for (uint64_t i = startNum; i <= top && count < num; i++) {
                    std::vector<std::string> blockHashs;
                    if (dbReader.getBlockHashsByBlockHeight(i, blockHashs) != DBStatus::DB_SUCCESS) {
                        continue;
                    }

                    if (blockHashs.empty()) {
                        continue;
                    }

                    // Add height cell when displaying the first block hash of each height
                    str += "<tr>\n";
                    str += "<td class='height-cell'>" + std::to_string(i) + "</td>\n";
                    str += "<td><div class='hash-container'>";
                    
                    for (const auto &blockHash : blockHashs) {
                        std::string strHeader;
                        if (dbReader.getBlockByBlockHash(blockHash, strHeader) != DBStatus::DB_SUCCESS) {
                            continue;
                        }

                        CBlock block;
                        if (!block.ParseFromString(strHeader)) {
                            continue;
                        }

                        bool isContractualBlock = false;
                        for (const auto &tx : block.txs()) {
                            if ((global::ca::TxType)tx.txtype() == global::ca::TxType::kTransactionTypeDeploy || 
                                (global::ca::TxType)tx.txtype() == global::ca::TxType::TX_TYPE_INVOKE_CONTRACT) {
                                isContractualBlock = true;
                                break;
                            }
                        }

                        str += "<span class='hash-cell ";
                        str += (isContractualBlock ? "contract-block" : "normal-block");
                        str += "'>" + blockHash.substr(0, 8) + "</span>";
                        
                        if (req.has_param("pre_hash_flag")) {
                            str += "<span class='hash-cell'>(" + block.prevhash().substr(0, 8) + ")</span>";
                        }
                    }
                    
                    str += "</div></td>\n";
                    str += "</tr>\n";
                    
                    count++;
                    if (count >= num) {
                        break;
                    }
                }
            }
            
            str += "</tbody>\n</table>\n</div>\n</body>\n</html>";
            res.set_content(str, "text/html");
        } else {
            str = print_range_contract_blocks(startNum, num, req.has_param("pre_hash_flag"));
            res.set_content(str, "text/plain");
        }
    }
}

void ApiInfo(const Request &req, Response &res) 
{
    std::ostringstream oss;

    oss << "queue:" << std::endl;
    oss << "queueReader:" << global::queueReader.msgQueue.size() << std::endl;
    oss << "queue_work:" << global::queue_work.msgQueue.size() << std::endl;
    oss << "queue_write_counter:" << global::queue_write_counter.msgQueue.size() << std::endl;
    oss << "\n" << std::endl;

    oss << "amount:" << std::endl;
    std::vector<std::string> baseList;

    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(baseList);
    for (auto &i : baseList) {
        uint64_t amount = 0;
        get_balance_by_utxo(i, global::ca::ASSET_TYPE_VOTE, amount);
        oss << "0x"+i + ":" + std::to_string(amount) << std::endl;
    }
    oss << "\n" << std::endl;

    std::vector<Node> nodeList =
        MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    oss << "Public PeerNode size is: " << nodeList.size() << std::endl;
    oss << MagicSingleton<PeerNode>::GetInstance()->NodelistInfo(nodeList);

    oss << std::endl << std::endl;

    res.set_content(oss.str(), "text/plain");
}

void getBlockRequest(const Request &req, Response &res) {
    nlohmann::json block;
    nlohmann::json blocks;

    int top = 0;
    if (req.has_param("top")) {
        top = atol(req.get_param_value("top").c_str());
    }
    int num = 0;
    if (req.has_param("num")) {
        num = atol(req.get_param_value("num").c_str());
    }

    num = num > 500 ? 500 : num;

    if (top < 0 || num < 0) {
        ERRORLOG("getBlockRequest top < 0||num < 0");
        return;
    }

    DBReader dbReader;
    uint64_t myTop = 0;
    dbReader.getBlockTop(myTop);
    if (top > (int)myTop) {
        ERRORLOG("getBlockRequest begin > myTop");
        return;
    }
    int k = 0;
    uint64_t countNum = top + num;
    if (countNum > myTop) {
        countNum = myTop;
    }
    for (auto i = top; i <= countNum; i++) {
        std::vector<std::string> blockHashs;

        if (dbReader.getBlockHashsByBlockHeight(i, blockHashs) !=
            DBStatus::DB_SUCCESS) 
        {
            return;
        }

        for (auto hash : blockHashs) 
        {
            std::string strHeader;
            if (dbReader.getBlockByBlockHash(hash, strHeader) !=
                DBStatus::DB_SUCCESS) 
            {
                return;
            }
            BlockInvert(strHeader, block);
            blocks[k++] = block;
        }
    }
    std::string str = blocks.dump();
    res.set_content(str, "application/json");
}

void _ShowValidatedVRFs(const Request &req, Response &res) 
{
    std::string content = MagicSingleton<VRFConsensusNode>::GetInstance()->printfValidatedVRfs();

    res.set_content(content, "text/plain");
}

void _ShowNewValidatedVRFs(const Request &req, Response &res) 
{
    std::string content = MagicSingleton<VRFConsensusNode>::GetInstance()->printfNewValidatedVRfs();

    res.set_content(content, "text/plain");
}

void _ApiPub(const Request &req, Response &res) 
{
    std::string str;
    str = "<!DOCTYPE html>\n<html>\n<head>\n";
    str += "<meta charset='UTF-8'>\n";
    str += "<title>MoonMesh Thread Info</title>\n";
    str += "<style>\n";
    str += "body { background: #000; color: #fff; font-family: 'Arial', sans-serif; margin: 0; padding: 0; }\n";
    str += ".header { display: flex; align-items: center; padding: 30px 0 0 60px; }\n";
    str += ".logo-img { height: 48px; }\n";
    str += ".footer { position: fixed; left: 0; bottom: 0; width: 100%; height: 80px; background: #111; border-top: 2px solid #222; display: flex; align-items: center; z-index: 10; }\n";
    str += ".footer-logo-img { height: 48px; margin-left: auto; margin-right: 20%; }\n";
    str += ".main-title { margin-left: 60px; margin-top: 20px; font-size: 22px; color: #fff; font-weight: 500; text-align: left; font-style: italic; }\n";
    str += ".divider { width: 90%; min-width: 400px; margin: 18px 0 0 60px; border-bottom: 2px solid #222; text-align: left; }\n";
    
    // Add new styles to beautify the Thread section.
    str += ".task-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap: 20px; width: 90%; margin: 20px 0 30px 60px; }\n";
    str += ".task-card { background: #111; border-radius: 6px; padding: 15px; border-left: 3px solid #444; transition: all 0.3s ease; }\n";
    str += ".task-card:hover { border-left-color: #00A046; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0, 0, 0, 0.5); }\n";
    str += ".task-title { color: #00C853; font-weight: bold; margin-bottom: 8px; font-size: 14px; }\n";
    str += ".task-value { color: #bdbdbd; font-family: monospace; font-size: 13px; }\n";
    str += ".task-zero { color: #555; }\n";
    str += ".task-grid-title { color: #888; margin: 5px 0 15px 60px; font-size: 16px; font-style: italic; }\n";
    str += ".system-info { background: #111; border-radius: 6px; padding: 15px; margin: 0 0 20px 60px; width: 90%; max-width: 1200px; }\n";
    str += ".info-row { display: flex; margin-bottom: 10px; }\n";
    str += ".info-label { color: #00C853; width: 130px; font-weight: bold; }\n";
    str += ".info-value { color: #bdbdbd; font-family: monospace; flex-grow: 1; }\n";
    str += ".section-gap { margin-top: 30px; }\n";
    
    // Data table style
    str += ".data-table { width: 90%; min-width: 400px; margin: 20px 0 80px 60px; border-collapse: separate; border-spacing: 0 10px; text-align: left; }\n";
    str += ".data-table th { font-size: 16px; color: #00C853; background: none; border: none; text-align: left; padding-bottom: 10px; font-style: italic; }\n";
    str += ".data-table td { font-size: 14px; color: #bdbdbd; background: #111; border: none; padding: 12px 20px; vertical-align: middle; text-align: left; }\n";
    str += ".data-table tr td:first-child { border-top-left-radius: 4px; border-bottom-left-radius: 4px; }\n";
    str += ".data-table tr td:last-child { border-top-right-radius: 4px; border-bottom-right-radius: 4px; }\n";
    str += ".data-line { font-family: monospace; margin: 10px 0 10px 60px; color: #bdbdbd; background: #111; padding: 10px 15px; border-radius: 4px; display: inline-block; }\n";
    str += "</style>\n</head>\n<body>\n";
    str += "<div class='header'><img class='logo-img' src='" + MOONMESHCHAIN_LOGO_BASE64 + "' alt='MoonMesh Logo'></div>\n";
    
    // Thread Section
    str += "<div class='main-title'>Thread</div>\n";
    str += "<div class='divider'></div>\n";
    
    // Get file information
    const int MaxInformationSize = 256;
    char buff[MaxInformationSize] = {};
    std::string fileName = "Unknown";
    FILE *f = fopen("/proc/self/cmdline", "r");
    if (f != NULL) {
        char readc;
        int i = 0;
        while (((readc = fgetc(f)) != EOF)) {
            if (readc == '\0') {
                buff[i++] = ' ';
            } else {
                buff[i++] = readc;
            }
        }
        fclose(f);
        fileName = strtok(buff, "\n");
    }
    
    // Show program information card
    str += "<div class='system-info'>\n";
    str += "  <div class='info-row'>\n";
    str += "    <div class='info-label'>Program Name:</div>\n";
    str += "    <div class='info-value'>" + fileName + "</div>\n";
    str += "  </div>\n";
    str += "</div>\n";
    
    // Obtain thread task information
    std::ostringstream taskOss;
    MagicSingleton<ProtobufDispatcher>::GetInstance()->TaskInfo(taskOss);
    std::string taskInfo = taskOss.str();
    
    // Handle queue information
    std::vector<std::pair<std::string, size_t>> queueInfos = {
        {"Read Queue", global::queueReader.msgQueue.size()},
        {"Work Queue", global::queue_work.msgQueue.size()},
        {"Write Queue", global::queue_write_counter.msgQueue.size()}
    };
    
    // Create a task card grid
    str += "<div class='task-grid-title'>Thread Task Statistics</div>\n";
    str += "<div class='task-grid'>\n";
    
    // Store data of different task categories
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> taskGroups;
    
    // Analyze the task information and group them accordingly
    std::istringstream taskStream(taskInfo);
    std::string line;
    std::string currentCategory = "";
    
    while (std::getline(taskStream, line)) {
        if (line.empty() || line.find("===") != std::string::npos) {
            continue; // Remove delimiters and blank lines
        }
        
        // Extract the task name and value from the row.
        size_t colonPos = line.find(":");
        if (colonPos != std::string::npos) {
            std::string taskName = line.substr(0, colonPos);
            std::string taskValue = line.substr(colonPos + 1);
            
            // Extract the task category (such as ca, net, broadcast, etc.)
            size_t underscorePos = taskName.find("_");
            if (underscorePos != std::string::npos) {
                std::string category = taskName.substr(0, underscorePos);
                
                // Determine the task type (active or pending)
                std::string taskType;
                if (taskName.find("active") != std::string::npos) {
                    taskType = "Active Tasks";
                } else if (taskName.find("pending") != std::string::npos) {
                    taskType = "Pending Tasks";
                } else {
                    taskType = "Other";
                }
                
                // Add the task to the corresponding group
                taskGroups[category].push_back({taskType, taskValue});
            }
        }
    }
    
    // Generate module task cards
    for (const auto& group : taskGroups) {
        str += "<div class='task-card'>\n";
        str += "  <div class='task-title'>" + group.first + " Module</div>\n";
        
        for (const auto& task : group.second) {
            str += "  <div class='task-value'>";
            str += task.first + ": ";
            if (task.second == "0") {
                str += "<span class='task-zero'>" + task.second + "</span>";
            } else {
                str += "<strong>" + task.second + "</strong>";
            }
            str += "</div>\n";
        }
        
        str += "</div>\n";
    }
    
    // Add queue status card
    str += "<div class='task-card'>\n";
    str += "  <div class='task-title'>Queue Status</div>\n";
    
    for (const auto& queue : queueInfos) {
        str += "  <div class='task-value'>";
        str += queue.first + ": ";
        if (queue.second == 0) {
            str += "<span class='task-zero'>" + std::to_string(queue.second) + "</span>";
        } else {
            str += "<strong>" + std::to_string(queue.second) + "</strong>";
        }
        str += "</div>\n";
    }
    
    str += "</div>\n";
    str += "</div>\n"; // close task-grid
    
    // Request section
    str += "<div class='main-title'>Request</div>\n";
    str += "<div class='divider'></div>\n";
    
    // Add table titles and headers
    str += "<div class='task-grid-title'>Request Statistics</div>\n";
    str += "<table class='data-table'>\n";
    str += "<thead>\n";
    str += "<tr><th>Request Type</th><th>Count</th><th>Data Size</th></tr>\n";
    str += "</thead>\n<tbody>\n";
    
    double total = .0f;
    uint64_t n64Count = 0;
    
    for (auto &item : global::requestCountMap) {
        total += (double)item.second.second; // data size
        str += "<tr>";
        str += "<td>" + item.first + "</td>";
        str += "<td>" + std::to_string(item.second.first) + "</td>";
        
        std::ostringstream sizeStream;
        sizeStream.precision(3);
        sizeStream << (double)item.second.second / 1024 / 1024 << " MB";
        str += "<td>" + sizeStream.str() + "</td>";
        str += "</tr>\n";
        
        n64Count += item.second.first;
    }
    str += "</tbody></table>\n";
    
    // Display total information
    str += "<div class='system-info' style='margin-top: 15px;'>\n";
    str += "  <div class='info-row'>\n";
    str += "    <div class='info-label'>Total Requests:</div>\n";
    str += "    <div class='info-value'>" + std::to_string(n64Count) + "</div>\n";
    str += "  </div>\n";
    str += "  <div class='info-row'>\n";
    str += "    <div class='info-label'>Total Data Size:</div>\n";
    
    std::ostringstream totalSizeStream;
    totalSizeStream.precision(3);
    totalSizeStream << (double)total / 1024 / 1024 << " MB";
    
    str += "    <div class='info-value'>" + totalSizeStream.str() + "</div>\n";
    str += "  </div>\n";
    str += "</div>\n";
    
    str += "<div class='footer'><img class='footer-logo-img' src='" + MOONMESHCHAIN_LOGO_BASE64 + "' alt='MoonMesh Logo'></div>\n";
    str += "</body>\n</html>";
    res.set_content(str, "text/html");
}

// ... existing code ...
// New function: Account Information Display Page
void _ApiAccount(const Request &req, Response &res)
{
    std::string str;
    str = "<!DOCTYPE html>\n<html>\n<head>\n";
    str += "<meta charset='UTF-8'>\n";
    str += "<title>MoonMesh Account Info</title>\n";
    str += "<style>\n";
    str += "body { background: #000; color: #fff; font-family: 'Arial', sans-serif; margin: 0; padding: 0; }\n";
    str += ".header { display: flex; align-items: center; padding: 30px 0 0 60px; }\n";
    str += ".logo-img { height: 48px; }\n";
    str += ".footer { position: fixed; left: 0; bottom: 0; width: 100%; height: 80px; background: #111; border-top: 2px solid #222; display: flex; align-items: center; z-index: 10; }\n";
    str += ".footer-logo-img { height: 48px; margin-left: auto; margin-right: 20%; }\n";
    str += ".main-title { margin-left: 60px; margin-top: 20px; font-size: 22px; color: #fff; font-weight: 500; text-align: left; font-style: italic; }\n";
    str += ".divider { width: 90%; min-width: 400px; margin: 18px 0 0 60px; border-bottom: 2px solid #222; text-align: left; }\n";
    
    // Add new styles to beautify the Account section
    str += ".task-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap: 20px; width: 90%; margin: 20px 0 30px 60px; }\n";
    str += ".task-card { background: #111; border-radius: 6px; padding: 15px; border-left: 3px solid #444; transition: all 0.3s ease; }\n";
    str += ".task-card:hover { border-left-color: #00A046; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0, 0, 0, 0.5); }\n";
    str += ".task-title { color: #00C853; font-weight: bold; margin-bottom: 8px; font-size: 14px; }\n";
    str += ".task-value { color: #bdbdbd; font-family: monospace; font-size: 13px; }\n";
    str += ".task-zero { color: #555; }\n";
    str += ".task-grid-title { color: #888; margin: 5px 0 15px 60px; font-size: 16px; font-style: italic; }\n";
    str += ".system-info { background: #111; border-radius: 6px; padding: 15px; margin: 0 0 20px 60px; width: 90%; max-width: 1200px; }\n";
    str += ".info-row { display: flex; margin-bottom: 10px; }\n";
    str += ".info-label { color: #00C853; width: 130px; font-weight: bold; }\n";
    str += ".info-value { color: #bdbdbd; font-family: monospace; flex-grow: 1; }\n";
    str += ".section-gap { margin-top: 30px; }\n";
    
    // Data table format
    str += ".data-table { width: 90%; min-width: 400px; margin: 20px 0 80px 60px; border-collapse: separate; border-spacing: 0 10px; text-align: left; }\n";
    str += ".data-table th { font-size: 16px; color: #00C853; background: none; border: none; text-align: left; padding-bottom: 10px; font-style: italic; }\n";
    str += ".data-table td { font-size: 14px; color: #bdbdbd; background: #111; border: none; padding: 12px 20px; vertical-align: middle; text-align: left; }\n";
    str += ".data-table tr td:first-child { border-top-left-radius: 4px; border-bottom-left-radius: 4px; }\n";
    str += ".data-table tr td:last-child { border-top-right-radius: 4px; border-bottom-right-radius: 4px; }\n";
    str += ".data-line { font-family: monospace; margin: 10px 0 10px 60px; color: #bdbdbd; background: #111; padding: 10px 15px; border-radius: 4px; display: inline-block; }\n";
    str += "</style>\n</head>\n<body>\n";
    str += "<div class='header'><img class='logo-img' src='" + MOONMESHCHAIN_LOGO_BASE64 + "' alt='MoonMesh Logo'></div>\n";
    
    // Account Section
    str += "<div class='main-title'>Account</div>\n";
    str += "<div class='divider'></div>\n";
    
    // The variable used for calculating the total asset amount
    uint64_t totalVoteAssets = 0;
    uint64_t totalMM = 0;
    
    // Account information statistics
    str += "<div class='task-grid-title'>Account Information</div>\n";
    str += "<table class='data-table'>\n";
    str += "<thead>\n";
    str += "<tr><th>Account Address</th><th>Assets</th></tr>\n";
    str += "</thead>\n<tbody>\n";
    
    std::vector<std::string> baseList;
    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(baseList);
    
    if (baseList.empty()) {
        // If there is no account, display a prompt message.
        str += "<tr><td colspan='2' style='text-align:center;'>No account data available</td></tr>\n";
    } else {
        // Display all account information
        for (auto &i : baseList) {
            int64_t voteAmount = 0;
            int64_t mmAmount = 0;
            
            std::string assetType;
            int ret = ca_algorithm::GetCanBeRevokeAssetType(assetType);
            if(ret != 0){
                ERRORLOG("Get Can BeRevoke AssetType fail!");
            }

            DBReader dbReader;
            dbReader.getBalanceByAddr(i, global::ca::ASSET_TYPE_VOTE, voteAmount);
            double v = voteAmount / double(100000000);
            dbReader.getBalanceByAddr(i, assetType, mmAmount);
            double m = mmAmount / double(100000000);

            uint64_t blockHeight = 0;
            dbReader.getBlockTop(blockHeight);

            totalVoteAssets += voteAmount;
            totalMM += mmAmount;

             
            str += "<tr>";
            str += "<td><span style='font-family:monospace;'>0x" + i + "</span></td>";
            str += "<td>\n";
            str += "  <div style='display:flex; flex-direction:column; gap:8px;'>\n";
            str += "    <div><span style='color:#00C853; font-weight:500;'>Vote Assets:</span> " + std::to_string(v) + "</div>\n";
            str += "    <div><span style='color:#00C853; font-weight:500;'>MM Balance:</span> " + std::to_string(m) + "</div>\n";
            // More asset types can be added here.
            // str += "    <div><span style='color:#00C853; font-weight:500;'>Asset Type X:</span> value</div>\n";
            // str += "    <div><span style='color:#00C853; font-weight:500;'>Asset Type Y:</span> value</div>\n";
            str += "  </div>\n";
            str += "</td>";
            str += "</tr>\n";
        }
    }
    
    str += "</tbody></table>\n";
    
    // Account statistics information
    str += "<div class='system-info' style='margin-top: 15px;'>\n";
    str += "  <div class='info-row'>\n";
    str += "    <div class='info-label'>Total Accounts:</div>\n";
    str += "    <div class='info-value'>" + std::to_string(baseList.size()) + "</div>\n";
    str += "  </div>\n";
    str += "</div>\n";
    
    str += "<div class='footer'><img class='footer-logo-img' src='" + MOONMESHCHAIN_LOGO_BASE64 + "' alt='MoonMesh Logo'></div>\n";
    str += "</body>\n</html>";
    res.set_content(str, "text/html");
}

void _ApiNode(const Request &req, Response &res)
{
    std::vector<Node> pubNodeList_ = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::string str;
    str = "<!DOCTYPE html>\n<html>\n<head>\n";
    str += "<meta charset='UTF-8'>\n";
    str += "<title>MoonMesh Node List</title>\n";
    str += "<style>\n";
    str += "body { background: #000; color: #fff; font-family: Arial, sans-serif; margin: 0; padding: 0; }\n";
    str += ".header { display: flex; align-items: center; padding: 30px 0 0 60px; }\n";
    str += ".logo-img { height: 48px; }\n";
    str += ".footer { position: fixed; left: 0; bottom: 0; width: 100%; height: 80px; background: #111; border-top: 2px solid #222; display: flex; align-items: center; z-index: 10; }\n";
    str += ".footer-logo-img { height: 48px; margin-left: auto; margin-right: 20%; }\n";
    str += ".main-title { margin-left: 60px; margin-top: 20px; font-size: 22px; color: #fff; font-weight: 500; text-align: left; font-style: italic; }\n";
    str += ".divider { width: 90%; min-width: 400px; margin: 18px 0 0 60px; border-bottom: 2px solid #222; text-align: left; }\n";
    str += ".node-table { width: 90%; min-width: 400px; margin: 20px 0 80px 60px; border-collapse: separate; border-spacing: 0 10px; text-align: left; }\n";
    str += ".node-table th { font-size: 18px; color: #fff; background: none; border: none; text-align: left; padding-bottom: 10px; font-style: italic; }\n";
    str += ".node-table th:nth-child(1) { width: 350px; }\n"; 
    str += ".node-table { table-layout: fixed; }\n";
    str += ".node-table th { padding-left: 40px; }\n";
    str += ".node-table th:first-child { padding-left: 0; }\n";
    str += ".node-table td { font-size: 16px; color: #fff; background: none; border: none; padding: 6px 0 6px 40px; vertical-align: middle; text-align: left; font-style: normal; }\n";
    str += ".node-table td:first-child { padding-left: 0; width: 350px; }\n";
    str += ".node-table .addr { font-size: 14px; color: #bdbdbd; }\n";
    str += ".node-table .logo-cell { height: 32px; }\n";
    str += ".node-table .logo-img-cell { height: 32px; }\n";
    str += "</style>\n</head>\n<body>\n";
    str += "<div class='header'><img class='logo-img' src='" + MOONMESHCHAIN_LOGO_BASE64 + "' alt='MoonMesh Logo'></div>\n";
    str += "<div class='main-title'>Node List</div>\n";
    str += "<div class='divider'></div>\n";
    str += "<div style='margin-left:60px;margin-top:10px;font-size:16px;'>Public PeerNode size is:" + std::to_string(pubNodeList_.size()) + "</div>\n";
    str += "<table class='node-table'>\n<thead><tr>";
    str += "<th>Addr</th><th>Ip</th><th>Height</th><th>Port</th><th>Kind</th><th>Fd</th><th>Pulse</th><th>Version</th>";
    str += "</tr></thead>\n<tbody>\n";
    for (const auto& node : pubNodeList_) {
        str += "<tr>";
        str += "<td class='addr'>0x" + node.address + "</td>";
        str += "<td>" + std::string(IpPort::IpSz(node.publicIp)) + "</td>";
        str += "<td>" + std::to_string(node.height) + "</td>";
        str += "<td>" + std::to_string(node.publicPort) + "</td>";
        str += "<td>" + std::to_string(node.connKind) + "</td>";
        str += "<td>" + std::to_string(node.fd) + "</td>";
        str += "<td>" + std::to_string(node.pulse) + "</td>";
        str += "<td>" + node.ver + "</td>";
        str += "</tr>\n";
    }
    str += "</tbody></table>\n";
    str += "<div class='footer'><img class='footer-logo-img' src='" + MOONMESHCHAIN_LOGO_BASE64 + "' alt='MoonMesh Logo'></div>\n";
    str += "</body>\n</html>";
    res.set_content(str, "text/html");
}
// ... existing code ... 

void getRequesterIp(const Request &req, Response & res){
    std::ostringstream oss;
    std::string ip = req.remote_addr;
    oss << ip;
    res.set_content(oss.str(), "application/json");
}


void _ApiPrintCalc1000SumHash(const Request &req,Response &res)
{
    int startHeight = 1000;
    if (req.has_param("startHeight")) {
        startHeight = atol(req.get_param_value("startHeight").c_str());
    }
    if(startHeight <= 0 || startHeight % 1000 != 0)
    {
        res.set_content("startHeight error", "text/plain");
        return;
    }

    int endHeight = 0;
    if (req.has_param("endHeight")) {
        endHeight = atol(req.get_param_value("endHeight").c_str());
    }

    if(endHeight < startHeight || endHeight % 1000 != 0)
    {
        res.set_content("endHeight error", "text/plain");
        return;
    }

    uint64_t max_height = 0;
    DBReader dbReader;
    if(DBStatus::DB_SUCCESS != dbReader.getTopThousandSumHash(max_height))
    {
        res.set_content("GetBlockComHashHeight error", "text/plain");
        return;
    }

    std::ostringstream oss;
    if(max_height < endHeight)
    {
        endHeight = max_height;
        oss << "max_height = " << max_height << std::endl;
    }


    for(int i = startHeight; i <= endHeight; i += 1000)
    {
        std::string sumHash;
        auto ret = dbReader.getCheckBlockHashsByBlockHeight(i, sumHash);
        if(ret == DBStatus::DB_SUCCESS)
        {
            oss << "blockHeight: " << i << "\t sumHash: " << sumHash << std::endl;
        }
        else 
        {
            oss << "getCheckBlockHashsByBlockHeight error" << std::endl;
        }
        
    }


    res.set_content(oss.str(), "text/plain");

}

void _ApiPrintHundredSumHash(const Request & req, Response & res)
{
    int startHeight = 100;
    if (req.has_param("startHeight")) {
        startHeight = atol(req.get_param_value("startHeight").c_str());
    }
    if(startHeight <= 0 || startHeight % 100 != 0)
    {
        res.set_content("startHeight error", "text/plain");
        return;
    }

    int endHeight = 0;
    if (req.has_param("endHeight")) {
        endHeight = atol(req.get_param_value("endHeight").c_str());
    }

    if(endHeight < startHeight || endHeight % 100 != 0)
    {
        res.set_content("endHeight error", "text/plain");
        return;
    }

    DBReader dbReader;
    std::ostringstream oss;

    for(int i = startHeight; i <= endHeight; i += 100)
    {
        std::string sumHash;
        auto ret = dbReader.getSumHashByHeight(i, sumHash);
        if(ret == DBStatus::DB_SUCCESS)
        {
            oss << "blockHeight: " << i << "\t sumHash: " << sumHash << std::endl;
        }
        else 
        {
            oss << "getSumHashByHeight error, error height: " << i << std::endl;
        }
        
    }
    
    res.set_content(oss.str(), "text/plain");
}

void _ApiPrintAllBlocks(const Request &req,Response &res)
{
    int startHeight = 1;
    if (req.has_param("startHeight")) {
        startHeight = atol(req.get_param_value("startHeight").c_str());
    }
    if(startHeight <= 0)
    {
        res.set_content("error startHeight <= 0", "text/plain");
        return;
    }

    int endHeight = 0;
    if (req.has_param("endHeight")) {
        endHeight = atol(req.get_param_value("endHeight").c_str());
    }

    uint64_t nodeSelfHeight = 0;
    DBReader dbReader;
    auto status = dbReader.getBlockTop(nodeSelfHeight);
    if (DBStatus::DB_SUCCESS != status)
    {
        res.set_content("getBlockTop error", "text/plain");
        return;
    }

    if(endHeight > nodeSelfHeight)
    {
        res.set_content("endHeight > nodeSelfHeight", "text/plain");
        return;
    }

    std::stringstream oss;
    oss << "block_hash_" << startHeight << "_" << endHeight << ".txt";
    std::ofstream fout(oss.str(), std::ios::out);
    for(int i = startHeight; i <= endHeight; ++i)
    {
        std::vector<std::string> selfBlockHashes_;
        if (DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(i, i, selfBlockHashes_))
        {
            res.set_content("getBlockHashesByBlockHeight error", "text/plain");
            return;
        }
        std::sort(selfBlockHashes_.begin(), selfBlockHashes_.end());
        fout << "block height: " << i << "\tblock size: " << selfBlockHashes_.size() << std::endl; 
        for(const auto& hash: selfBlockHashes_)
        {
            std::string strHeader;
            if (DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(hash, strHeader)) 
            {
                res.set_content("getBlockByBlockHash error", "text/plain");
                return;
            }

            CBlock block;
            if(!block.ParseFromString(strHeader))
            {
                res.set_content("ParseFromString error", "text/plain");
                return;
            }
            fout << block.hash() << std::endl;
        }
        fout << "==============================================\n\n";

    }

    res.set_content("print success", "text/plain");

}

int PrintProposalInfo(DBReadWriter &db, const std::string &type, std::ostringstream &oss)
{
    // Helper function for field rows
    auto add_field = [&](const std::string &label, const auto &value)
    {
        oss
            << "<div class=\"field-row\">"
            << "<span class=\"field-label\">" << label << "</span>"
            << "<span class=\"field-value\">" << value << "</span>"
            << "</div>";
    };

    // Helper function for address groups
    auto add_address_group = [&](const std::set<std::string> &addresses,
                                 const std::string &asserType,
                                 const std::string &type,
                                 const std::string &title)
    {
        oss << "<div class=\"address-group " << type << "\">"
            << "<div class=\"group-header\">"
            << "<span>" << title << "</span>"
            << "<span class=\"count\">" << addresses.size() << "</span>"
            << "</div>" // group-header
            << "<div class=\"address-list\">";

        for (const auto &addr : addresses)
        {
            uint64_t voteNum = 0;
            if(DBStatus::DB_SUCCESS != db.getVoteNumByAddr(addr, asserType, voteNum))
            {
                ERRORLOG("addVoteCount setVoteNumByAssetHash error, addr:{}, asserType:{}", addr, asserType);
                voteNum = UINT64_MAX;
            }

            oss << "<span class=\"address-item\">" << addr.substr(0, 6) << "-" << voteNum << "</span>";
        }

        oss << "</div></div>"; // address-list and address-group
    };

    // Main proposal card
    oss << "<div class=\"proposal-card\">"
        << "<div class=\"proposal-header\">"
        << "<div class=\"section-title\">Proposal Transaction</div>"
        << "<div class=\"hash-display\">"
        << "<span class=\"hash-label\">Proposal Hash:</span>"
        << "<span class=\"hash-value\">" << addHexPrefix(type) << "</span>"
        << "</div></div>" // hash-display and proposal-header
        << "<div class=\"proposal-body\">"
        << "<div class=\"info-section\">"
        << "<div class=\"section-title\">Basic Information</div>";

    // Basic info fields
    Base64 base;
    std::string info;
    if (auto dbRet = db.getAssetInfobyAssetType(type, info); dbRet == DBStatus::DB_SUCCESS)
    {
        std::string decode = base.Decode(info.c_str(), info.size());
        auto dataJson = nlohmann::json::parse(decode);
        std::string decodeName = dataJson["Name"].get<std::string>();
        std::string decodeTitle = dataJson["Title"].get<std::string>();
        std::string decodeIdentifier = dataJson["Identifier"].get<std::string>();
        add_field("Asset Name:", base.Decode(decodeName.c_str(), decodeName.size()));
        add_field("Version:", dataJson["Version"].get<int>());
        add_field("BeginTime:", ca_algorithm::ConversionTime(dataJson["BeginTime"].get<uint64_t>()));
        add_field("EndTime:", ca_algorithm::ConversionTime(dataJson["EndTime"].get<uint64_t>()));
        add_field("ExpirationDate:", dataJson["ExpirationDate"].get<uint64_t>());
        add_field("ExchangeRate:", dataJson["ExchangeRate"].get<std::string>());
        add_field("ContractAddress:", addHexPrefix(dataJson["ContractAddr"].get<std::string>()));
        add_field("canBeStake:", dataJson["canBeStake"].get<uint64_t>());
        add_field("MIN_VOTE_NUM:", dataJson["MIN_VOTE_NUM"].get<uint64_t>());
        add_field("Title:", base.Decode(decodeTitle.c_str(), decodeTitle.size()));
        add_field("Identifier:", base.Decode(decodeIdentifier.c_str(), decodeIdentifier.size()));
    }
    else
    {
        ERRORLOG("getAssetInfobyAssetType error: {}", dbRet);
        return -1;
    }

    oss << "</div>" // Close the previous container
        << "<div class=\"voting-section\">"
        << "<div class=\"section-title\">Voting Status</div>";

    // Voting info
    std::string voteCount;
    if (auto dbRet = db.getVoteNumByAssetHash(type, voteCount); dbRet == DBStatus::DB_SUCCESS)
    {
        add_field("CurrentVotes:", voteCount);
    }
    else
    {
        ERRORLOG("getVoteNumByAssetHash error: {}", dbRet);
        return -2;
    }

    uint64_t totalNumberOfVoters = 0;
    auto dbRet = db.getTotalNumberOfVotersByAssetHash(type, totalNumberOfVoters);
    if(dbRet == DBStatus::DB_NOT_FOUND)
    {
        add_field("TotalNumberOfVoters:", "None");
    }
    else if(dbRet != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("getTotalNumberOfVotersByAssetHash error {}", dbRet);
        return -10;
    }
    else 
    {
        add_field("TotalNumberOfVoters:", std::to_string(totalNumberOfVoters));
    }

    // Address groups
    std::set<std::string> addresses;
    if (auto dbRet = db.getApproveAddrsByAssetHash(type, addresses);
        dbRet == DBStatus::DB_SUCCESS || dbRet == DBStatus::DB_NOT_FOUND)
    {
        add_address_group(addresses, type, "approve", "Approved Addresses:");
    }
    else
    {
        ERRORLOG("GetApproveAddrs error: {}", dbRet);
        return -3;
    }

    addresses.clear();
    if (auto dbRet = db.getAgainstAddrsByAssetHash(type, addresses);
        dbRet == DBStatus::DB_SUCCESS || dbRet == DBStatus::DB_NOT_FOUND)
    {
        add_address_group(addresses, type, "against", "Against Addresses:");
    }
    else
    {
        ERRORLOG("GetAgainstAddrs error: {}", dbRet);
        return -4;
    }

    // Close the previous container
    oss << "</div></div></div>";

    // Cancel the transaction block
    std::vector<std::string> revokeHashes;
    if (auto dbRet = db.getRevokeTxHashByAssetType(type, revokeHashes); dbRet == DBStatus::DB_SUCCESS)
    {
        oss << "<div class='proposal-card revoked-card'>"
            << "<div class='revoked-header'>"
            << "<div class=\"section-title\">Revoke Proposal Transactions</div>"
            << "<div class='revoked-section'>"
            << "<div class='hash-display'>"
            << "</div>";                          // hash-display

        for (const auto &hash : revokeHashes)
        {                                             // revoked-header
            if (std::string revokeInfo; db.getRevokeProposalInfobyTxHash(hash, revokeInfo) == DBStatus::DB_SUCCESS)
            {
                auto decoded = base.Decode(revokeInfo.c_str(), revokeInfo.size());
                auto revokeJson = nlohmann::json::parse(decoded);

                oss << "<div class='revoked-details'>"
                    << "<div class='detail-group'>"
                    << "<div class='hash-display'>"
                    << "<span class='hash-label'>RevokeProposalHash:</span>"
                    << "<span class='hash-value'>" << addHexPrefix(hash) << "</span>"
                    << "</div>" // hash-display
                    << "<div class='section-title'>Basic Information</div>";

                add_field("OriginalProposal:", addHexPrefix(revokeJson["ProposalHash"].get<std::string>()));
                add_field("Version:", revokeJson["Version"].get<int>());
                add_field("BeginTime:", ca_algorithm::ConversionTime(revokeJson["BeginTime"].get<uint64_t>()));
                add_field("EndTime:", ca_algorithm::ConversionTime(revokeJson["EndTime"].get<uint64_t>()));
                add_field("MIN_VOTE_NUM:", revokeJson["MIN_VOTE_NUM"].get<uint64_t>());

                oss << "</div></div>" // detail-group and revoked-details
                    << "<div class='voting-section'>"
                    << "<div class='section-title'>Voting Status</div>"
                    << "</div>"; // voting-section
            }

            std::string voteCount;
            if (auto dbRet = db.getVoteNumByAssetHash(hash, voteCount); dbRet == DBStatus::DB_SUCCESS)
            {
                add_field("CurrentVotes:", voteCount);
            }
            else
            {
                ERRORLOG("getVoteNumByAssetHash error: {}", dbRet);
                return -3;
            }

            uint64_t totalNumberOfVoters = 0;
            auto dbRet = db.getTotalNumberOfVotersByAssetHash(hash, totalNumberOfVoters);
            if(dbRet == DBStatus::DB_NOT_FOUND)
            {
                add_field("TotalNumberOfVoters:", "None");
            }
            else if(dbRet != DBStatus::DB_SUCCESS)
            {
                ERRORLOG("getTotalNumberOfVotersByAssetHash error {}", dbRet);
                return -10;
            }
            else 
            {
                add_field("TotalNumberOfVoters:", std::to_string(totalNumberOfVoters));
            }

            // Address groups
            std::set<std::string> addresses;
            if (auto dbRet = db.getApproveAddrsByAssetHash(hash, addresses);
                dbRet == DBStatus::DB_SUCCESS || dbRet == DBStatus::DB_NOT_FOUND)
            {
                add_address_group(addresses, hash, "approve", "Approved Addresses:");
            }
            else
            {
                ERRORLOG("GetApproveAddrs error: {}", dbRet);
                return -4;
            }

            addresses.clear();
            if (auto dbRet = db.getAgainstAddrsByAssetHash(hash, addresses);
                dbRet == DBStatus::DB_SUCCESS || dbRet == DBStatus::DB_NOT_FOUND)
            {
                add_address_group(addresses, hash, "against", "Against Addresses:");
            }
            else
            {
                ERRORLOG("GetAgainstAddrs error: {}", dbRet);
                return -5;
            }
        }
        oss << "</div></div></div>"; // revoked-section, revoked-header, proposal-card
        oss << "</div>";             // revoked-item;
    }

    // Close main containers
    oss << "</div></div></div>";

    return 0;
}

void _ApiPrintVoteInfo(const Request &req,Response &res)
{
    DBReadWriter db;
    std::ostringstream error;
    std::vector<std::string> assetType;
    auto dbRet = db.getAllAssetType(assetType);
    if(DBStatus::DB_SUCCESS != dbRet)
    {
        error << "getAllAssetType error, error num: " << dbRet << std::endl;
        res.set_content(error.str(), "text/plain");
        return;
    }
    std::ostringstream oss;

    // HTML Document Structure
    oss << "<!DOCTYPE html>\n"
        << "<html>\n<head>\n"
        << "<meta charset=\"UTF-8\">\n"
        << "<title>Proposal Details</title>\n"
        << "<style>\n";

    // CSS Variables
    oss << ":root {\n"
        << "  --primary: #2ECC71;\n"
        << "  --text-label: #FFFFFF;\n"
        << "  --text-key: #FFFFFF;\n"
        << "  --text-value: #FFFFFF;\n"
        << "  --card-bg: #1E1E1E;\n"
        << "  --border-radius: 8px;\n"
        << "}\n\n";

    // Base Styles
    oss << "body {\n"
        << "  background: #121212;\n"
        << "  color: white;\n"
        << "  font-family: 'Segoe UI', sans-serif;\n"
        << "  margin: 0;\n"
        << "  padding: 2rem;\n"
        << "}\n\n";

    // Brand Header
    oss << ".brand-header {\n"
        << "  display: flex;\n"
        << "  align-items: center;\n"
        << "  margin-bottom: 2.5rem;\n"
        << "  width: 200px;\n"
        << "  height: 40px;\n"
        << "  background-image: url('" << MOONMESHCHAIN_LOGO_BASE64 << "');\n"
        << "  background-size: contain;\n"
        << "  background-repeat: no-repeat;\n"
        << "}\n\n";

    // Proposal Card
    oss << ".proposal-card {\n"
        << "  background: var(--card-bg);\n"
        << "  border-radius: var(--border-radius);\n"
        << "  padding: 1.5rem;\n"
        << "  margin-bottom: 1.5rem;\n"
        << "  box-shadow: 0 4px 6px rgba(0,0,0,0.1);\n"
        << "}\n\n";

    // Field Layout
    oss << ".field-row {\n"
        << "  display: grid;\n"
        << "  grid-template-columns: 160px 1fr;\n"
        << "  align-items: baseline;\n"
        << "  margin: 8px 0;\n"
        << "}\n\n"
        << ".field-label {\n"
        << "  color: var(--text-label);\n"
        << "  font-family: 'Segoe UI', sans-serif;\n"
        << "  font-size: 14px;\n"
        << "  text-align: left;\n"
        << "  padding-right: 12px;\n"
        << "}\n\n"
        << ".field-value {\n"
        << "  color: var(--text-value);\n"
        << "  font-family: 'Segoe UI', sans-serif;\n"
        << "  font-size: 14px;\n"
        << "  font-weight: 500;\n"
        << "  word-break: break-word;\n"
        << "  padding-left: 2px;\n"
        << "}\n\n";

    // Address Groups
    oss << ".address-group {\n"
        << "  margin: 0.8rem 0;\n"
        << "}\n\n"
        << ".group-header {\n"
        << "  display: grid;\n"
        << "  grid-template-columns: 160px 1fr;\n"
        << "  align-items: baseline;\n"
        << "}\n\n"
        << ".group-header > span:first-child {\n"
        << "  color: #FFF;\n"
        << "  font-family: 'Segoe UI', sans-serif;\n"
        << "  font-size: 14px;\n"
        << "  font-weight: 500;\n"
        << "  padding-right: 12px;\n"
        << "}\n\n"
        << ".count {\n"
        << "  color: #FFF;\n"
        << "  font-family: 'Segoe UI', sans-serif;\n"
        << "  font-size: 14px;\n"
        << "  font-weight: 500;\n"
        << "}\n\n"
        << ".address-list {\n"
        << "  margin-left: 160px;\n"
        << "  display: flex;\n"
        << "  gap: 8px;\n"
        << "  flex-wrap: wrap;\n"
        << "  margin-top: 4px;\n"
        << "}\n\n"
        << ".address-item {\n"
        << "  color: #FFF;\n"
        << "  font-family: monospace;\n"
        << "  background: rgba(46, 204, 113, 0.1);\n"
        << "  padding: 2px 8px;\n"
        << "  border-radius: 4px;\n"
        << "  border: 1px solid #2ECC71;\n"
        << "}\n\n";

    // Special Sections
    oss << ".voting-section .section-title {\n"
        << "  color: var(--text-label);\n"
        << "  font-size: 16px;\n"
        << "  margin: 20px 0 12px;\n"
        << "}\n\n"
        << ".timestamp {\n"
        << "  color: var(--primary);\n"
        << "  font-family: monospace;\n"
        << "}\n\n";

    // Close Styles
    oss << "</style>\n</head>\n<body>\n"
        << "<div class=\"brand-header\">\n"
        << "  <div class=\"logo-icon\"></div>\n"
        << "</div>\n"
        << "<h2>Proposal Details</h2>\n";
    for (const auto &type : assetType)
    {
        //oss << "=====================================================================\n";
        int ret = PrintProposalInfo(db, type, oss);
        if(ret != 0)
        {
            error << "PrintProposalInfo error, error num: " << ret << std::endl;
            res.set_content(error.str(), "text/plain");
            return;
        }
    }

    
    res.set_content(oss.str(), "text/html");
}

void _ApiPrintLockAddrs(const Request &req,Response &res)
{
    DBReadWriter db;
    std::ostringstream oss;

    std::vector<std::string> addresses;
    auto dbRet = db.getLockAddr(addresses);
    if(dbRet != DB_SUCCESS)
    {
        oss << "getLockAddr error, error num: " << dbRet << std::endl;
        res.set_content(oss.str(), "text/plain");
        return;
    }

    oss << addresses.size() << std::endl;
    for(const auto& t: addresses)
    {
        oss << "addr: " << t;
        std::vector<std::string> utxos;
        dbRet = db.getLockAddrUtxo(t, global::ca::ASSET_TYPE_VOTE, utxos);
        if(dbRet != DB_SUCCESS)
        {
            oss << "\nGetLockAddressUtxo error, error num: " << dbRet << "error addr: " << t << std::endl;
            res.set_content(oss.str(), "text/plain");
            return;
        }
        for(const auto& utxo: utxos)
        {
            oss << " - " << utxo;
        }
        oss << "\n";
    }

    res.set_content(oss.str(), "text/plain");
}

void _ApiPrintProposalInfo(const Request &req,Response &res)
{
    std::string hash;
    if (req.has_param("hash")) {
        hash = req.get_param_value("hash").c_str();
    }
    if (hash.empty()) {
        res.set_content("get hash error", "text/plain");
        return;
    }

    DBReadWriter db;
    std::ostringstream error;
    std::ostringstream oss;
    int ret = PrintProposalInfo(db, hash, oss);
    if(ret != 0)
    {
        error << "PrintProposalInfo error, error num: " << ret << std::endl;
        res.set_content(error.str(), "text/plain");
        return;
    }

    res.set_content(oss.str(), "text/plain");
}

void _ApiPrintAvailableAssetType(const Request &req,Response &res)
{
    std::map<std::string, TxHelper::ProposalInfo> assetMap;
    int ret = ca_algorithm::fetchAvailableAssetType(assetMap);

    if(ret != 0)
    {
        ERRORLOG("fetchAvailableAssetType Error");
        return;
    }
    if(assetMap.empty())
    {
        res.set_content("Available asset type is empty", "text/plain");
        return;
    }

    std::ostringstream html;
    html << "<!DOCTYPE html>"
         << "<html>"
         << "<head>"
         << "<style>"
         << "body {"
         << "  background: #121212;"
         << "  color: white;"
         << "  font-family: 'Segoe UI', sans-serif;"
         << "  margin: 0;"
         << "  padding: 2rem;"
         << "}"
         << ".brand-header {"
         << "  display: flex;"
         << "  align-items: center;"
         << "  margin-bottom: 2.5rem;"
         << "  width: 200px;"
         << "  height: 40px;"
         << "  background-image: url('" << MOONMESHCHAIN_LOGO_BASE64 << "');"
         << "  background-size: contain;"
         << "  background-repeat: no-repeat;"
         << "}"
         << ".logo-icon {"
         << "  width: 36px;"
         << "  height: 36px;"
         << "  background: var(--primary);"
         << "  clip-path: path('M12 2 C18 2 24 8 24 14 C24 20 18 26 12 26 C6 26 0 20 0 14 C0 8 6 2 12 2 Z');"
         << "  margin-right: 12px;"
         << "}"
         << ".proposal-card {"
         << "  background: #1E1E1E;"
         << "  border-radius: 8px;"
         << "  padding: 1.5rem;"
         << "  margin-bottom: 1.5rem;"
         << "  box-shadow: 0 4px 6px rgba(0,0,0,0.1);"
         << "}"
         << ".detail-row {"
         << "  display: grid;"
         << "  grid-template-columns: 160px 1fr;"
         << "  gap: 1rem;"
         << "  margin: 0.8rem 0;"
         << "}"
         << ".field-label {"
         << "  color: #FFFFFF;"
         << "  text-align: right;"
         << "  font-size: 0.95rem;"
         << "}"
         << ".field-value {"
         << "  color: #FFF;"
         << "  word-break: break-all;"
         << "  font-weight: 500;"
         << "}"
         << ".field-label {"
         << "  color: #FFF;"
         << "  text-align: right;"
         << "  font-size: 0.95rem;"
         << "  font-weight: 500;"
         << "}"
         << ".timestamp {"
         << "  color: var(--primary);"
         << "  font-family: monospace;"
         << "}"
         << "</style>"
         << "</head>"
         << "<body>"
         << "<div class=\"brand-header\">"
         << "  <div class=\"logo-icon\"></div>"
         << "</div>"
         << "<h2>Proposal Details</h2>";

    if (assetMap.empty())
    {
        html << R"(<div class="proposal-card"><h3 style="color:#FF5555">No available assets</h3></div>)";
    }
    else
    {
        for (const auto &t : assetMap)
        {
            html << R"(<div class="proposal-card"><div class="detail-row">)";
            {
                std::ostringstream oss;
                const auto &proposal = t.second;
                using Field = std::tuple<std::string, std::string, bool>;
                const std::vector<Field> fields = {
                    {"Hash:", addHexPrefix(t.first), false},
                    {"Proposal Type:", proposal.Name, false},
                    {"Exchange Rate:", (oss << proposal.ExchangeRate, oss.str()), false},
                    {"Begin Time:", (oss.str(""), oss << ca_algorithm::ConversionTime(proposal.BeginTime) << "(" << proposal.BeginTime << ")", oss.str()), true},
                    {"End Time:", (oss.str(""), oss << ca_algorithm::ConversionTime(proposal.EndTime) << "(" << proposal.EndTime << ")", oss.str()), true},
                    {"Contract Address:", addHexPrefix(proposal.ContractAddr), false},
                    {"canBeStake:", (oss.str(""), oss << std::boolalpha << proposal.canBeStake, oss.str()), false},
                    {"MIN_VOTE_NUM:", (oss.str(""), oss << proposal.MIN_VOTE_NUM, oss.str()), false}};

                for (const auto &[label, value, space] : fields)
                {
                    html << R"(<div class="field-label">)" << label
                         << R"(</div><div class="field-value)"
                         << (space ? " " : "") << R"(">)" << value << R"(</div>)";
                }
            }
            html << R"(</div></div>)";
        }
    }
    html << R"(</body></html>)";

    res.set_content(html.str(), "text/html");
}

void _ApiGetAsseTypeByContractAddr(const Request &req,Response &res)
{
    std::string contractAddr;
    if (req.has_param("contractAddr")) 
    {
        contractAddr = req.get_param_value("contractAddr").c_str();
    }
    if (contractAddr.empty()) 
    {
        res.set_content("get contractAddr error", "text/plain");
        return;
    }

    std::string assetType;
    int ret = ca_algorithm::GetAsseTypeByContractAddr(contractAddr, assetType);
    if(ret == 0)
    {
        res.set_content(assetType, "text/plain");
    }
    else
    {
        std::ostringstream oss;
        oss << "GetContractAddrByAsseType error, error num: " << ret;
        res.set_content(oss.str(), "text/plain");
    }

    return;
}

void _GetLock(const Request &req,Response &res)
{
    getLockReq req_t;
    txAck ack_t;

    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "createLockTransaction";
    ack_t.sleeptime = req_t.sleeptime;

    std::string fromAddr = req_t.fromAddr;
    if (fromAddr.substr(0, 2) == "0x") 
    {
        fromAddr = fromAddr.substr(2);
    }

    int32_t lockType = std::stoll(req_t.LockType);

    // std::regex pattern("^[1-9]\\d*$"); 
    // if(!std::regex_match(req_t.lockAmount, pattern))
    // {
    //     ack_t.code = -18;
    //     ack_t.message = "LockAmount format error";
    //     return res.set_content(ack_t._parseToString(), "application/json");;
    // }
    CHECK_VALUE(req_t.lockAmount);
    uint64_t lockAmount = (std::stod(req_t.lockAmount) + global::ca::MIN_DOUBLE_CONSTANT_PRECISION) * global::ca::kDecimalNum;

    bool is_find_utxo_flag = req_t.isFindUtxo;
    std::string encodedInfo = Base64Encode(req_t.txInfo);
    std::pair<std::string,std::string> gasTrade = req_t.gasTrade ;
    gasTrade.first = remove0xPrefix(gasTrade.first);
    gasTrade.second = remove0xPrefix(gasTrade.second);
    bool isGasTrade = req_t.isGasTrade ; 

    ReplaceCreateLockTransaction(fromAddr, lockAmount, lockType, gasTrade,isGasTrade, is_find_utxo_flag, encodedInfo, &ack_t);

    res.set_content(ack_t._parseToString(), "application/json");
}


void _GetLockUtxo(const Request & req, Response & res)
{
    getLockutxoAck ack_t;
    getLockutxoReq req_t;

    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetLockUtxo";

    std::string strFromAddr = req_t.fromAddr;
    if (strFromAddr.substr(0, 2) == "0x") 
    {
        strFromAddr = strFromAddr.substr(2);
    }

    DBReader dbReader;
    std::vector<std::string> utxos;
    auto ret = dbReader.getLockAddrUtxo(strFromAddr, global::ca::ASSET_TYPE_VOTE ,utxos);
    if(ret != DBStatus::DB_SUCCESS)
    {
        ack_t.code = -1;
        ack_t.message = "fromaddr not lock!";
    }

    for (auto &utxo : utxos) {
        
        std::string txRaw;
        if(DBStatus::DB_SUCCESS != dbReader.getTransactionByHash(utxo, txRaw))
        {
            ack_t.code = -2;
            ack_t.message = "get transaction error";
        }
        CTransaction tx;
        if(!tx.ParseFromString(txRaw))
        {
            ack_t.code = -3;
            ack_t.message = "parse transaction error";
        }
        uint64_t value = 0;
        for(const auto & txUtxo : tx.utxos())
        {
            for (auto &vout : txUtxo.vout()) 
            {
                if (vout.addr() == global::ca::VIRTUAL_LOCK_ADDRESS) 
                {
                    value = vout.value();
                }
                ack_t.utxos.first = utxo;
                ack_t.utxos.second = value;
            }
        }
    }

    ack_t.code = 0;
    ack_t.message = "success";
    res.set_content(ack_t._parseToString(), "application/json");
    DEBUGLOG("http_api.cpp:GetStakingUtxo ack_T.parseToString{}",ack_t._parseToString());
}
void _GetUnLock(const Request & req, Response & res)
{
    getUnLockReq req_t;
    txAck ack_t;
    VALIDATE_PARSINREQUEST
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "createUnlockTransactionRequest";
    ack_t.sleeptime = req_t.sleeptime;

    std::string fromAddr = req_t.fromAddr;
    if (fromAddr.substr(0, 2) == "0x") 
    {
        fromAddr = fromAddr.substr(2);
    }
    
    std::string utxoHash = remove0xPrefix(req_t.utxoHash);

    bool is_find_utxo_flag = req_t.isFindUtxo;
    std::string encodedInfo = Base64Encode(req_t.txInfo);
    std::pair<std::string,std::string> gasTrade = req_t.gasTrade ;
    gasTrade.first = remove0xPrefix(gasTrade.first);
    gasTrade.second = remove0xPrefix(gasTrade.second);
    bool isGasTrade = req_t.isGasTrade ; 
    ReplaceCreateUnLockTransaction(fromAddr, utxoHash,gasTrade,isGasTrade, is_find_utxo_flag, encodedInfo, &ack_t);
    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetProposal(const Request & req, Response & res)
{
    GetProposalReq req_t;
    txAck ack_t;
    VALIDATE_PARSINREQUEST

    std::string fromAddr = remove0xPrefix(req_t.fromAddr);
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "CreateProposalTransaction";
    ack_t.sleeptime = req_t.sleeptime;

    const std::regex pattern(R"(-?\d+(\.\d)?\b)");
    if (!std::regex_match(req_t.duration, pattern))
    {
        ERRORLOG("GetProposal input duration error");
        ack_t.code = -100;
        ack_t.message = "input duration error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
    double time = 0;
    try 
    {
        time = std::stod(req_t.duration);
    } 
    catch (const std::invalid_argument& e) 
    {
        ERRORLOG("GetProposal duration string to double error");
        ack_t.code = -101;
        ack_t.message = "duration string to double error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
    
    bool is_find_utxo_flag = req_t.isFindUtxo;
    std::string encodedInfo = Base64Encode(req_t.txInfo);
    req_t.gasTrade.first = remove0xPrefix(req_t.gasTrade.first);
    req_t.gasTrade.second = remove0xPrefix(req_t.gasTrade.second);

    uint64_t  beginTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    uint64_t  endTime = beginTime + time * 3600 * 1000000; 

    std::regex pattern2("^[1-9]\\d*$"); 
    if(!std::regex_match(req_t.minVoteNum, pattern2))
    {
        ERRORLOG("Input minVoteNum error");
        ack_t.code = -102;
        ack_t.message = "Input minVoteNum error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
    uint64_t minVote = std::stoull(req_t.minVoteNum);

    ReplaceCreateProposalTransaction(fromAddr, req_t.gasTrade, req_t.Identifier, req_t.title, beginTime, endTime, req_t.assetName, req_t.exchangeRate, remove0xPrefix(req_t.contractAddr), minVote, is_find_utxo_flag, encodedInfo, &ack_t);

    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetRevokeProposal(const Request & req, Response & res)
{
    getRevokeProposalReq req_t;
    txAck ack_t;
    VALIDATE_PARSINREQUEST

    std::string fromAddr = remove0xPrefix(req_t.fromAddr);
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "CreateRevokeProposalTransaction";
    ack_t.sleeptime = req_t.sleeptime;

    const std::regex pattern(R"(-?\d+(\.\d)?\b)");
    if (!std::regex_match(req_t.duration, pattern))
    {
        ERRORLOG("GetProposal input duration error");
        ack_t.code = -100;
        ack_t.message = "input duration error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
    double time = 0;
    try 
    {
        time = std::stod(req_t.duration);
    } 
    catch (const std::invalid_argument& e) 
    {
        ERRORLOG("GetProposal duration string to double error");
        ack_t.code = -101;
        ack_t.message = "duration string to double error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    bool is_find_utxo_flag = req_t.isFindUtxo;
    std::string encodedInfo = Base64Encode(req_t.txInfo);
    req_t.gasTrade.first = remove0xPrefix(req_t.gasTrade.first);
    req_t.gasTrade.second = remove0xPrefix(req_t.gasTrade.second);
    uint64_t  beginTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    uint64_t  endTime = beginTime + time * 3600 * 1000000; 

    std::regex pattern2("^[1-9]\\d*$"); 
    if(!std::regex_match(req_t.minVoteNum, pattern2))
    {
        ERRORLOG("Input minVoteNum error");
        ack_t.code = -102;
        ack_t.message = "Input minVoteNum error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
    uint64_t minVote = std::stoull(req_t.minVoteNum);

    if(minVote < 1)
    {
        ERRORLOG("minVote mest more then 1, minVote:{}", minVote);
        ack_t.code = -103;
        ack_t.message = "minVote mest more then 1";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    ReplaceCreateRevokeProposalTransaction(fromAddr, req_t.gasTrade, beginTime, endTime, remove0xPrefix(req_t.proposalHash), minVote, is_find_utxo_flag, encodedInfo, &ack_t);
    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetVote(const Request & req, Response & res)
{
    getVoteReq req_t; 
    txAck ack_t;
    VALIDATE_PARSINREQUEST

    std::string fromAddr = remove0xPrefix(req_t.fromAddr);
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "CreateVoteTransaction";
    ack_t.sleeptime = req_t.sleeptime;

    int32_t pollType = std::stoll(req_t.pollType);
    global::ca::TxType voteTxType = (global::ca::TxType)std::stoll(req_t.voteTxType);

    bool is_find_utxo_flag = req_t.isFindUtxo;
    std::string encodedInfo = Base64Encode(req_t.txInfo);
    req_t.gasTrade.first = remove0xPrefix(req_t.gasTrade.first);
    req_t.gasTrade.second = remove0xPrefix(req_t.gasTrade.second);

    ReplaceCreateVoteTransaction(fromAddr, req_t.gasTrade, remove0xPrefix(req_t.voteHash), pollType, voteTxType, is_find_utxo_flag, encodedInfo, &ack_t);
    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetFund(const Request & req, Response & res){
    getFundReq req_t;
    txAck ack_t;
    
    VALIDATE_PARSINREQUEST
    std::string fromAddr = remove0xPrefix(req_t.fromAddr);
    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "CreateFundTransaction";
    ack_t.sleeptime = req_t.sleeptime;

    bool is_find_utxo_flag = req_t.isFindUtxo;
    std::string encodeInfo = Base64Encode(req_t.txInfo);
    
    //comment: encodeInfo doesn't use
    ReplaceCreateFundTransaction(fromAddr,encodeInfo,&ack_t);
    res.set_content(ack_t._parseToString(),"application/json");
}

void _ApiGetAssetType(const Request & req, Response & res)
{
    getAssetTypeReq req_t;
    getAssetTypeAck ack_t;
    VALIDATE_PARSINREQUEST

    DBReader db;

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "getAllAssetType";
    ack_t.code = 0;
    ack_t.message = "success";
    auto dbRet = db.getAllAssetType(ack_t.assertType);
    if(DBStatus::DB_SUCCESS != dbRet)
    {
        ack_t.code = -1;
        ack_t.message = "Get asset type error";
    }
    for(auto & i: ack_t.assertType)
    {
        i = addHexPrefix(i);
    }

    res.set_content(ack_t._parseToString(), "application/json");
}

void _ApiGetAssetTypeInfo(const Request & req, Response & res)
{
    getAssetTypeInfoReq req_t;
    getAssetTypeInfoAck ack_t;
    VALIDATE_PARSINREQUEST

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetAssetTypeInfo";
    ack_t.code = 0;
    ack_t.message = "success";
    DBReader dbReader;

    std::string info;
    auto dbRet = dbReader.getAssetInfobyAssetType(remove0xPrefix(req_t.assetType), info);
    if(DBStatus::DB_SUCCESS != dbRet)
    {
        ack_t.code = -1;
        ack_t.message = "Get asset info error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
    Base64 base;
    ack_t.proposalInfo = base.Decode(info.c_str(), info.size());

    try
    {
        nlohmann::json proposal_info = nlohmann::json::parse(ack_t.proposalInfo);
        std::string contractAddressTemp_ = proposal_info["ContractAddr"];
        proposal_info["ContractAddr"] = addHexPrefix(contractAddressTemp_);
        ack_t.proposalInfo = proposal_info.dump();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    
    std::vector<std::string> revokeTxHashs;
    dbRet = dbReader.getRevokeTxHashByAssetType(remove0xPrefix(req_t.assetType), revokeTxHashs);
    if(dbRet == DBStatus::DB_NOT_FOUND)
    {
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
    else if(dbRet != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("getRevokeTxHashByAssetType error, error num: {}", dbRet);
        ack_t.code = -2;
        ack_t.message = "Get revoke tx hash error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }
    for(const auto& t: revokeTxHashs)
    {
        std::string revokeInfo;
        dbRet = dbReader.getRevokeProposalInfobyTxHash(t, revokeInfo);
        if(DBStatus::DB_SUCCESS != dbRet)
        {
            ERRORLOG("getRevokeProposalInfobyTxHash error, error num: {}", dbRet);
            ack_t.code = -3;
            ack_t.message = "Get revoke proposal info error";
            res.set_content(ack_t._parseToString(), "application/json");
            return;
        }
        std::string decode = base.Decode(revokeInfo.c_str(), revokeInfo.size());
        nlohmann::json dataJson = nlohmann::json::parse(decode);
        dataJson["RevokeProposalHash"] = addHexPrefix(t);
        try
        {
            std::string tmpProposalHash = dataJson["ProposalHash"];
            dataJson["ProposalHash"] = addHexPrefix(tmpProposalHash);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }

        ack_t.revokeProposalInfo.emplace_back(dataJson.dump());
    }
    res.set_content(ack_t._parseToString(), "application/json");
}

void _ApiGetVoteAddrsByHash(const Request &req, Response &res)
{
    getVoteAddrsReq req_t;
    getVoteAddrsAck ack_t;
    VALIDATE_PARSINREQUEST

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetVoteAddrs";
    ack_t.code = 0;
    ack_t.message = "success";
    DBReader db;

    if (remove0xPrefix(req_t.hash).size() != 64)
    {
        ERRORLOG("getApproveAddrsByAssetHash error, Asset Hash length error hash is{}", req_t.hash);
        ack_t.code = -1;
        ack_t.message = "Asset Hash length error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    std::set<std::string> approveAddrs;
    std::string hash = remove0xPrefix(req_t.hash);
    auto dbRet = db.getApproveAddrsByAssetHash(hash, approveAddrs);
    if (DBStatus::DB_SUCCESS != dbRet && DBStatus::DB_NOT_FOUND != dbRet)
    {
        ERRORLOG("getApproveAddrsByAssetHash error, error num: {}", dbRet);
        ack_t.code = -2;
        ack_t.message = "Get approve addrs error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    for (const auto &addr : approveAddrs)
    {
        uint64_t voteNum = 0;
        dbRet = db.getVoteNumByAddr(addr, hash, voteNum);
        if(DBStatus::DB_SUCCESS != dbRet)
        {
            ERRORLOG("getVoteNumByAddr error, error num: {}", dbRet);
            ack_t.code = -3;
            ack_t.message = "Get approve vote number error";
            res.set_content(ack_t._parseToString(), "application/json");
            return;
        }

        ack_t.approveAddrs.insert(std::make_pair(addHexPrefix(addr), voteNum));
    }

    std::set<std::string> againstAddrs;
    dbRet = db.getAgainstAddrsByAssetHash(hash, againstAddrs);
    if (DBStatus::DB_SUCCESS != dbRet && DBStatus::DB_NOT_FOUND != dbRet)
    {
        ERRORLOG("getAgainstAddrsByAssetHash error, error num: {}", dbRet);
        ack_t.code = -4;
        ack_t.message = "Get against addrs error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    for (const auto &addr : againstAddrs)
    {
        uint64_t voteNum = 0;
        dbRet = db.getVoteNumByAddr(addr, hash, voteNum);
        if(DBStatus::DB_SUCCESS != dbRet)
        {
            ERRORLOG("getVoteNumByAddr error, error num: {}", dbRet);
            ack_t.code = -5;
            ack_t.message = "Get approve vote number error";
            res.set_content(ack_t._parseToString(), "application/json");
            return;
        }
        ack_t.againstAddrs.insert(std::make_pair(addHexPrefix(addr), voteNum));
    }

    res.set_content(ack_t._parseToString(), "application/json");
}

void _ApiGetVoteTxHashByAssetType(const Request & req, Response & res)
{
    getVoteTxHashAck ack_t;
    getVoteTxHashReq req_t;
    VALIDATE_PARSINREQUEST

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetVoteTxHash";
    ack_t.code = 0;
    ack_t.message = "success";

    DBReader dbReader;
    if (DBStatus::DB_SUCCESS != dbReader.GetVoteTxHashByAssetHash(remove0xPrefix(req_t.hash), ack_t.txHashs))
    {
        ack_t.code = -1;
        ack_t.message = "Get vote tx hash error";
    }
    for(auto & i: ack_t.txHashs)
    {
        i = addHexPrefix(i);
    }
    res.set_content(ack_t._parseToString(), "application/json");
}

void _GetAddrType(const Request &req, Response &res)
{
    auto setPackagedStatus = [](getAddrTypeAck &ack_t, DBReader &dbReader, std::string addr)
    {
        uint64_t times = 0, count = 0;
        int ret = VerifyBonusAddr(addr);
        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(addr, global::ca::StakeType::STAKE_TYPE_NODE);
        uint64_t curTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        uint64_t period = MagicSingleton<TimeUtil>::GetInstance()->GetPeriod(curTime);

        ack_t.isPackaged = false;

        if (stakeTime > 0 && ret == 0)
        {
            DEBUGLOG("Meet the pledge qualifications.");
            bool dbSuccess = (dbReader.getPackagerTimesByPeriod(period - 1, addr, times) == DBStatus::DB_SUCCESS &&
                              dbReader.getBlockNumberByPeriod(period - 1, count) == DBStatus::DB_SUCCESS);

            if (dbSuccess && times != 0 && count != 0)
            {
                ack_t.isPackaged = true;
            }
            else if (!dbSuccess)
            {
                ERRORLOG("Get addr package times failed!");
            }
        }
    };

    getAddrTypeReq req_t;
    getAddrTypeAck ack_t;
    VALIDATE_PARSINREQUEST

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "GetAddrType";
    ack_t.code = 0;
    ack_t.message = "success";

    std::string addr = remove0xPrefix(req_t.addr);

    DBReader dbReader;
    std::vector<std::string> utxos;
    DBStatus::DB_SUCCESS == dbReader.getLockAddrUtxo(addr, global::ca::ASSET_TYPE_VOTE, utxos) ? ack_t.isLocked = true : ack_t.isLocked = false;


    utxos.clear();
    std::string assetType;
    int ret = ca_algorithm::GetCanBeRevokeAssetType(assetType);
    if(ret != 0)
    {
        ack_t.isStaked = false;
    }
    else 
    {
        DBStatus::DB_SUCCESS == dbReader.getStakeAddrUtxo(addr, assetType, utxos) ? ack_t.isStaked = true : ack_t.isStaked = false;
    }
    

    utxos.clear();
    DBStatus::DB_SUCCESS == dbReader.getBonusAddrAndAssetTypeByDelegatingAddr(addr, utxos) ? ack_t.isDelegated = true : ack_t.isDelegated = false;

    uint64_t curTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    std::map<std::string, uint64_t> companyDividend;
    ret = ca_algorithm::CalcBonusValue(curTime, addr, companyDividend);


	int ret2 = CheckNodeValidity(addr);

    setPackagedStatus(ack_t, dbReader, addr);
    if(ret != 0 || ret2 != 0)
	{
        ack_t.isBonus = false;
        ack_t.isQualified = false;
        res.set_content(ack_t._parseToString(), "application/json");
        return;
	}

	ret = evaluateBonusEligibility(addr, curTime);
    ret == 0 ? ack_t.isBonus = true : ack_t.isBonus = false;

    ack_t.isQualified = true;
    res.set_content(ack_t._parseToString(), "application/json");
}
void _GetBonusAddrByDelegatingAddr(const Request &req, Response &res)
{
    getBonusAddrByDelegateAddrAck ack_t;
    getBonusAddrByDelegateAddrReq req_t;
    VALIDATE_PARSINREQUEST

    ack_t.id = req_t.id;
    ack_t.jsonrpc = req_t.jsonrpc;
    ack_t.method = "getBonusAddrByDelegatingAddr";
    ack_t.code = 0;
    ack_t.message = "success"; 

    std::string addr = remove0xPrefix(req_t.addr);

    DBReader dbReader;
    std::vector<std::string> bonusAddr;
    if(DBStatus::DB_SUCCESS != dbReader.getBonusAddrAndAssetTypeByDelegatingAddr(addr, bonusAddr))
    {
        ack_t.code = -1;
        ack_t.message = "Get bonus addr error";
        res.set_content(ack_t._parseToString(), "application/json");
        return;
    }

    for(auto & bonus: bonusAddr)
    {
        std::vector<std::string> addrAndAssetType;
        StringUtil::SplitString(bonus, "-", addrAndAssetType);
        if(addrAndAssetType.size() != 2)
        {
            ack_t.code = -2;
            ack_t.message = "Get assetType error";
            res.set_content(ack_t._parseToString(), "application/json");
            return;
        }
        ack_t.bonusAddr = addHexPrefix(addrAndAssetType.at(0));
        ack_t.assetType = addHexPrefix(addrAndAssetType.at(1));
    }

    res.set_content(ack_t._parseToString(), "application/json");
}

struct CPUStat 
{
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
};

static std::vector<CPUStat> GetCpuStats() {
    std::vector<CPUStat> cpuStats;
    std::ifstream statFile("/proc/stat");

    std::string line;
    while (std::getline(statFile, line)) 
    {
        if (line.compare(0, 3, "cpu") == 0) 
        {
            std::istringstream ss(line);

            std::string cpuLabel;
            CPUStat stat;
            ss >> cpuLabel >> stat.user >> stat.nice >> stat.system >>
                stat.idle >> stat.iowait >> stat.irq >> stat.softirq;

            cpuStats.push_back(stat);
        }
    }

    return cpuStats;
}

static double CalculateCpuUsage(const CPUStat &prev, const CPUStat &curr) 
{
    auto prevTotal = prev.user + prev.nice + prev.system + prev.idle +
                      prev.iowait + prev.irq + prev.softirq;
    auto currTotal = curr.user + curr.nice + curr.system + curr.idle +
                      curr.iowait + curr.irq + curr.softirq;

    auto totalDiff = currTotal - prevTotal;
    auto idleDiff = curr.idle - prev.idle;

    return (totalDiff - idleDiff) * 100.0 / totalDiff;
}

static std::string ConvertDoubleToStringWithPrecision(double value, int precision) 
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

// get cpu info
std::string getCpuInfo() 
{
    std::string sum;
    sum =
        "======================================================================"
        "=========";
    sum += "\n";
    sum += "get_cpu_info";
    sum += "\n";
    std::ifstream cpuinfoFile("/proc/cpuinfo");
    std::string line;
    int cpuCores = 0;
    std::string cpuModel;
    double cpuFrequency = 0;

    while (std::getline(cpuinfoFile, line)) 
    {
        if (line.compare(0, 9, "processor") == 0) 
        {
            cpuCores++;
        } else if (line.compare(0, 10, "model name") == 0) 
        {
            cpuModel = line.substr(line.find(":") + 2);
        } else if (line.compare(0, 7, "cpu MHz") == 0) 
        {
            cpuFrequency = std::stod(line.substr(line.find(":") + 2)) / 1000;
        }
    }

    auto prevStats = GetCpuStats();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto currStats = GetCpuStats();

    double totalUsage = 0;
    for (size_t i = 1; i < prevStats.size(); ++i) {
        totalUsage += CalculateCpuUsage(prevStats[i], currStats[i]);
    }

    double avgUsage = totalUsage / (prevStats.size() - 1);
    sum +=
        "CPU Usage: " + ConvertDoubleToStringWithPrecision(avgUsage, 1) + "%" + "\n";
    sum += "CPU Frequency: " + ConvertDoubleToStringWithPrecision(cpuFrequency, 3) +
           " GHZ" + "\n";
    sum += "CPU Model: " + cpuModel + "\n";
    sum += "CPU Cores: " + std::to_string(cpuCores);
    return sum;
}


struct NetStat 
{
    unsigned long long bytesReceived;
    unsigned long long bytesSent;
};

static NetStat GetNetStat(const std::string &interface) {
    NetStat netStat = {0, 0};
    std::ifstream netDevFile("/proc/net/dev");
    std::string line;

    while (std::getline(netDevFile, line)) 
    {
        if (line.find(interface) != std::string::npos) 
        {
            std::istringstream ss(line);
            std::string iface;
            ss >> iface >> netStat.bytesReceived;

            for (int i = 0; i < 7; ++i) 
            {
                unsigned long long tmp;
                ss >> tmp;
            }

            ss >> netStat.bytesSent;
            break;
        }
    }

    return netStat;
}

static std::string formatSpeed(double speed) 
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << speed << " Mbps";
    return ss.str();
}


static std::string GetMacAddress(const std::string &interface) 
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) 
    {
        return "";
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) 
    {
        close(sock);
        return "";
    }

    close(sock);
    char macAddress[18];
    snprintf(macAddress, sizeof(macAddress), "%02x:%02x:%02x:%02x:%02x:%02x",
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[0]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[1]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[2]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[3]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[4]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[5]));

    return macAddress;
}

static std::string retrieveNetworkModelInfo(const std::string &interface) 
{
    std::string modelPath = "/sys/class/net/" + interface + "/device/modalias";
    std::ifstream modelFile(modelPath);
    if (!modelFile.is_open()) 
    {
        return "";
    }

    std::string modelInfo;
    std::getline(modelFile, modelInfo);
    modelFile.close();

    return modelInfo;
}

std::string GetNetRate() 
{
    std::string interface = "eth0";
    auto prevStat = GetNetStat(interface);
    std::string str;
    str =
        "======================================================================"
        "=========";
    str += "\n";
    str += "GetNetRate";
    str += "\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto currStat = GetNetStat(interface);

    double downloadSpeed =
        (currStat.bytesReceived - prevStat.bytesReceived) * 8 / 1000.0 /
        1000.0;
    double uploadSpeed =
        (currStat.bytesSent - prevStat.bytesSent) * 8 / 1000.0 / 1000.0;

    std::string downloadSpeedStr = formatSpeed(downloadSpeed);
    std::string uploadSpeedStr = formatSpeed(uploadSpeed);

    str += "Download speed: " + downloadSpeedStr + "\n";
    str += "Upload speed: " + uploadSpeedStr + "\n";
    str += "Interface: " + interface + "\n";
    str += "MAC Address: " + GetMacAddress(interface);
    str += "Model Info: " + retrieveNetworkModelInfo(interface);
    prevStat = currStat;
    return str;
}

std::string Exec(const char *cmd) 
{
    char buffer[128];
    std::string result = "";
    FILE *pipe = popen(cmd, "r");
    if (!pipe)
        throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}


static std::string GetOsVersion() 
{
    struct utsname buffer;
    std::string osRelease = Exec("cat /etc/os-release");
    std::string str;
    if (uname(&buffer) != 0) {
        return "Error getting OS version";
    }
    str = std::string(buffer.sysname) + " " + std::string(buffer.release) +
          " " + std::string(buffer.version);
    str += osRelease;
    return str;
}

std::string ApiGetSystemInfo() 
{
    std::string str;
    str =
        "======================================================================"
        "=========";
    str += "\n";
    str += "ApiGetSystemInfo";
    str += "\n";
    str += "OS Version: " + GetOsVersion() + "\n";
    return str;
}

std::string ApiTime()
{
    std::string str;
	auto now = std::time(0);
    str += std::ctime(&now);	
   	auto now1 = std::chrono::system_clock::now();
    auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(now1.time_since_epoch()).count();
    auto stamp= std::to_string(nowUs) ;
    str  += stamp +"\n";

    addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    addrinfo *result;
    getaddrinfo(NULL, "0", &hints, &result);

    auto timeMs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    std::string netTime = std::to_string(timeMs) ; 
    str += netTime + "\n";

    if(timeMs >1000 + nowUs)
    {
        std::string cache = std::to_string(timeMs - nowUs); 
        str +=cache +"microsecond"+"slow" + "\n" ;
    }

    if(nowUs  > 1000 + timeMs)
    {
        std::string cache = std::to_string(timeMs - nowUs);
        str +=cache +"microsecond"+"fast" +"\n";
    }
    else 
    {
        str +="normal";
        str +="\n";
    }
    str +="time check======================" ;
    str +="\n";
    return str;
}


std::string GetProcessInfo() 
{
    const int BUFFER_SIZE = 1024;

    std::string str;
    str =
        "======================================================================"
        "=========";
    str += "\n";
    str += "GetProcessInfo";
    str += "\n";

    FILE *pipe = popen("ps -ef", "r");
    if (!pipe) {

        return "-1";
    }
    
    char buffer[BUFFER_SIZE];
    while (fgets(buffer, BUFFER_SIZE, pipe)) 
    {

        str += buffer;
        str += "\n";
    }
    pclose(pipe);
    return str;
}

int GetFileLine() 
{
    FILE *fp;
    int flag = 0, count = 0;
    if ((fp = fopen("/proc/meminfo", "r")) == NULL)
        return -1;
    while (!feof(fp)) 
    {
        flag = fgetc(fp);
        if (flag == '\n')
            count++;
    }
    fclose(fp);
    return count;
}

void GetMemOccupy(int lenNum, std::string &strMem) 
{
    strMem = "";
    FILE *memInfo = fopen("/proc/meminfo", "r");
    if (NULL == memInfo) 
    {
        strMem = "-1 meminfo fopen error";
        return;
    }

    int i = 0;
    int value = 0;
    char name[512];
    char line[512];
    int fieldNumber = 2;
    int total = 0;
    int available = 0;
    while (fgets(line, sizeof(line) - 1, memInfo)) 
    {
        if (sscanf(line, "%s%u", name, &value) != fieldNumber) 
        {
            continue;
        }
        if (0 == strcmp(name, "MemTotal:")) 
        {
            total = value;
            strMem += "MemTotal:\t" + std::to_string(value) + '\n';
        } 
        else if (0 == strcmp(name, "MemFree:")) 
        {
            strMem += "MemFree:\t" + std::to_string(value) + '\n';
        } 
        else if (0 == strcmp(name, "MemAvailable:")) 
        {
            available = value;
            strMem += "MemAvailable:\t" + std::to_string(value) + '\n';
        } 
        else if (0 == strcmp(name, "Buffers:")) 
        {
            strMem += "MemBuffers:\t" + std::to_string(value) + '\n';
        } 
        else if (0 == strcmp(name, "Cached:")) 
        {
            strMem += "MemCached:\t" + std::to_string(value) + '\n';
        }
        else if (0 == strcmp(name, "SwapCached:")) 
        {
            strMem += "SwapCached:\t" + std::to_string(value) + '\n';
        } 
        else if (0 == strcmp(name, "SwapTotal:")) 
        {
            strMem += "SwapTotal:\t" + std::to_string(value) + '\n';
        } 
        else if (0 == strcmp(name, "SwapFree:")) 
        {
            strMem += "SwapFree:\t" + std::to_string(value) + '\n';
        }

        if (++i == lenNum) 
        {
            break;
        }
    }
    strMem += "Memory usage:\t" +
              std::to_string(100.0 * (total - available) / total) + "%\n";
    fclose(memInfo);
}

void GetDiskType(std::string &strMem) {
    std::ifstream rotational("/sys/block/sda/queue/rotational");
    if (rotational.is_open()) {
        int isRotational;
        rotational >> isRotational;
        strMem += "Disk type:\t";
        strMem += (isRotational ? "HDD" : "SSD");
        rotational.close();
    } 
    else 
    {
        strMem += "-1 Disk rotational open error";
    }
}

void systemInfo(const Request &req, Response &res) 
{
    std::string outPut;
    std::string MemStr;
    int lenNum = GetFileLine();
    GetMemOccupy(lenNum, MemStr);
    GetDiskType(MemStr);
    outPut =
        "=================================================================="
        "=============";
    outPut += "\n";
    outPut += "GetMemOccupy";
    outPut += MemStr;
    outPut += "\n";

    outPut += getCpuInfo() + "\n";
    outPut += GetNetRate() + "\n";
    outPut += ApiGetSystemInfo() + "\n";
    outPut += ApiTime() + "\n";
    outPut += GetProcessInfo() + "\n";
    

    res.set_content(outPut, "text/plain");
}
