#include "./rpc_tx.h"
#include <nlohmann/json.hpp>
#include "utils/tmp_log.h"


//parse params
#define PARSE_PARAMS(value)\
  if(jsObject.contains(#value)){\
        if(jsObject[#value].is_string()) { \
              try { \
                jsParams = nlohmann::json::parse(jsObject[#value].get<std::string>()); \
              } catch (const nlohmann::json::parse_error& e) { \
                errorL("Parse error for key %s: %s", std::string(#value).c_str(), e.what()); \
                return Sutil::Format("parse fail for key %s: %s", std::string(#value).c_str(), e.what()); \
              } \
        } else if(jsObject[#value].is_object()) { \
              jsParams = jsObject[#value].get<nlohmann::json>(); \
            } else { \
              errorL("Unsupported type for key %s", std::string(#value).c_str()); \
              return Sutil::Format("unsupported type for key %s", std::string(#value).c_str()); \
            } \
  }else{\
            if(!jsParams.contains("isFindUtxo")){\
                jsParams[#value] = false;\
            }else if(!jsParams.contains("txInfo")){\
                jsParams[#value] = "";\
            }else{\
                errorL("not found key:%s" ,std::string(#value))                             \
            return Sutil::Format("not found key:%s",std::string(#value));\
            }\
  }       

//pasrse usual params (id jsonrpc method)
#define PARSE_STANDARD_FORMAT_JSON(value)                                                      \
  if(jsObject.contains(#value)){\
		jsObject[#value].get_to(this->value);\
	}else{\
		if(!jsObject.contains("isFindUtxo")){\
			jsObject[#value] = false;\
		}else if(!jsObject.contains("txInfo")){\
			jsObject[#value] = "";\
		}else{\
			  errorL("not found key:%s" ,std::string(#value))                             \
        return Sutil::Format("not found key:%s",std::string(#value));\
		}\
	}

//parse special params
#define PARSE_JSON(value)\
  if(jsParams.contains(#value)){\
		jsParams[#value].get_to(this->value);\
	}else{\
		if(!jsParams.contains("isFindUtxo")){\
			jsParams[#value] = false;\
		}else if(!jsParams.contains("txInfo")){\
			jsParams[#value] = "";\
		}else{\
			  errorL("not found key:%s" ,std::string(#value))                             \
        return Sutil::Format("not found key:%s",std::string(#value));\
		}\
	}

//parse usual params to json
#define TO_JSON(value) jsObject[#value] = this->value;
//put special params to params
#define TO_RESULT(value) jsResult[#value] = this->value;
//put special params to result
#define TO_PARAMS(value) jsParams[#value] = this->value;

//parse request parse params in struct
#define PARSE_REQ(sname)                                                     \
  std::string sname::_parseFromJson(const std::string& json) {                  \
    nlohmann::json jsObject;                                                   \
    try {                                                                      \
        jsObject = nlohmann::json::parse(json);                                \
        nlohmann::json jsParams;\
        if (!jsObject.contains("jsonrpc")) {                                   \
            return "{\"error\":{\"code\":-32600,\"message\":\"Missing 'jsonrpc' field\"}}"; \
        }                                                                      \
        if (!jsObject.contains("id")) {                                        \
            return "{\"error\":{\"code\":-32600,\"message\":\"Missing 'id' field\"}}"; \
        }                                                                      \
        /* Verify the validity of the field values */                                                \
        if (jsObject["jsonrpc"] != "2.0") {                                    \
            return "{\"error\":{\"code\":-32600,\"message\":\"Invalid JSON-RPC version\"}}"; \
        }                                 \  
        PARSE_STANDARD_FORMAT_JSON(id)\
        PARSE_STANDARD_FORMAT_JSON(jsonrpc)\
        PARSE_PARAMS(params)
        
//Parse the ack request. Parse the "result" field into the structure.
#define PARSE_ACK(sname)                                                     \
  std::string sname::_parseFromJson(const std::string& json) {                   \
    nlohmann::json jsObject;                                                   \
    try {                                                                      \
        jsObject = nlohmann::json::parse(json);                                \
        nlohmann::json jsParams;\
        PARSE_STANDARD_FORMAT_JSON(id)\
        PARSE_STANDARD_FORMAT_JSON(method)\
        PARSE_STANDARD_FORMAT_JSON(jsonrpc)\
        PARSE_PARAMS(result)

//Serialize the req request and place the values in the params.
#define DUMP_REQ(sname)                                                     \
  std::string sname::_parseToString() {                                          \
    nlohmann::json jsObject;                                                   \
    nlohmann::json jsParams;                                                   \
    try {                                                                      \
    TO_JSON(id)\
    TO_JSON(jsonrpc)\

//Serialize the ack request and place the values in the structure in the "result" variable.
#define DUMP_ACK(sname)                                                     \
  std::string sname::_parseToString() {                                          \
    nlohmann::json jsObject;                                                   \
    nlohmann::json jsResult;                                                   \
    try {                                                                      \
      TO_JSON(id)\
      TO_JSON(method)\
      TO_JSON(jsonrpc)

//parse end
#define PARSE_END \
    }                                                                            \
    catch (std::exception & e) {                                                 \
        errorL("error:%s" , e.what());                                              \
        return e.what();                                                              \
    }                                                                            \
  return "OK";                                                                 \
  }

//REQ parse end
#define REQ_DUMP_END                                                                \
      jsObject["params"] = jsParams;\
      }                                                                            \
    catch (std::exception & e) {                                                 \
      errorL("error:%s",e.what());                                              \
      return std::string();                                                      \
    }                                                                            \
  return jsObject.dump();                                                      \
  }

//ACK parse end
#define DUMP_ACK_END                                                                \
        TO_RESULT(code)\
        TO_RESULT(message)\
        jsObject["result"] = jsResult;\
      }                                                                            \
    catch (std::exception & e) {                                                 \
      errorL("error:%s",e.what());                                              \
      return std::string();                                                      \
    }                                                                            \
  return jsObject.dump();                                                      \
}

//------------------------deploy_contract_req
PARSE_REQ(deploy_contract_req)
PARSE_JSON(addr)
PARSE_JSON(nContractType)
PARSE_JSON(info)
PARSE_JSON(contract)
PARSE_JSON(data)
PARSE_JSON(pubstr)
PARSE_JSON(gasTrade)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_REQ(deploy_contract_req)
TO_PARAMS(addr)
TO_PARAMS(nContractType)
TO_PARAMS(info)
TO_PARAMS(contract)
TO_PARAMS(data)
TO_PARAMS(pubstr)
TO_PARAMS(gasTrade) 
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(sleeptime)
REQ_DUMP_END

//------------------------call_contract_req
PARSE_REQ(call_contract_req)
PARSE_JSON(addr)
PARSE_JSON(deployer)
PARSE_JSON(deployutxo)
PARSE_JSON(args)
PARSE_JSON(pubstr)
PARSE_JSON(money)
PARSE_JSON(gasTrade)
PARSE_JSON(istochain)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(contractAddress)
PARSE_JSON(sleeptime)
PARSE_JSON(isGasTrade)
PARSE_END

DUMP_REQ(call_contract_req)
TO_PARAMS(addr)
TO_PARAMS(deployer)
TO_PARAMS(deployutxo)
TO_PARAMS(args)
TO_PARAMS(pubstr)
TO_PARAMS(money)
TO_PARAMS(gasTrade)
TO_PARAMS(istochain)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(contractAddress)
TO_PARAMS(sleeptime)
TO_PARAMS(isGasTrade)
REQ_DUMP_END

//------------------------tx_req
PARSE_REQ(tx_req)
PARSE_JSON(isGasTrade)
if(jsObject.contains("txAsset"))
{
  auto stu_ = jsObject["txAsset"];
  for (auto iter = stu_.begin(); iter != stu_.end(); iter++)
  {
    TxHelper::TransTable successfulTrade;
    auto obj_a = iter.value();
    if(obj_a.contains("assetType")){
      obj_a["assetType"].get_to(successfulTrade.assetType);
    }else{
      errorL("not found key addr");
    }
    if(obj_a.contains("fromAddr")){
      obj_a["fromAddr"].get_to(successfulTrade.fromAddr);
    }else{
      errorL("not found key addr");
    }

    if(obj_a.contains("toAddrAmount")){
      auto obj_b = obj_a["toAddrAmount"];

      for(auto ptr = obj_b.begin(); ptr != obj_b.end(); ptr++)
      {
        auto obj_c = ptr.value();
        std::string addr_t;
        int64_t value_t ;
        if (obj_c.contains("addr")) {
          obj_c["addr"].get_to(addr_t);
        } else {
          errorL("not found key addr");
        }
        if (obj_c.contains("value")) {
          obj_c["value"].get_to(value_t);
        } else {
          errorL("not found key value");
        }

        successfulTrade.toAddrAmount[addr_t] = value_t;
      }

    }else{
      errorL("not found key addr");
    }
    txAsset.push_back(successfulTrade);
  }
}
if(jsObject.contains("gasTrade"))
{
  auto stu_ = jsObject["gasTrade"];
  if(stu_.contains("gasFromAddr")){
    stu_["gasFromAddr"].get_to(gasTrade.first);
  }else{
    errorL("not found key addr");
  }

  if(stu_.contains("gasType")){
    stu_["gasType"].get_to(gasTrade.second);
  }else{
    errorL("not found key addr");
  }
}
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_REQ(tx_req)
TO_PARAMS(isGasTrade)
nlohmann::json tradeVec;
for(auto iter = txAsset.begin(); iter != txAsset.end(); iter++)
{
  nlohmann::json tradeJs;
  tradeJs["fromAddr"] = iter->fromAddr;
  tradeJs["assetType"] = iter->assetType;

  nlohmann::json to_addrs;
  for (auto ptr = iter->toAddrAmount.begin(); ptr != iter->toAddrAmount.end(); ptr++) {
    nlohmann::json to_addr;
    to_addr["addr"] = ptr->first;
    to_addr["value"] = ptr->second;
    to_addrs.push_back(to_addr);
  }
  tradeJs["toAddrAmount"] = to_addrs;
  tradeVec.push_back(tradeJs);
}
nlohmann::json Jsgastrade;
Jsgastrade["gasFromAddr"] = gasTrade.first;
Jsgastrade["gasType"] = gasTrade.second;

jsObject["txAsset"] = tradeVec;
jsObject["gasTrade"] = Jsgastrade;
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(sleeptime)
REQ_DUMP_END

//------------------------getStakeReq
PARSE_REQ(getStakeReq)
PARSE_JSON(fromAddr)
PARSE_JSON(stakeAmount)
PARSE_JSON(StakeType)
PARSE_JSON(commissionRate)
PARSE_JSON(isGasTrade)
PARSE_JSON(gasTrade)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_REQ(getStakeReq)
TO_PARAMS(fromAddr)
TO_PARAMS(stakeAmount)
TO_PARAMS(StakeType)
TO_PARAMS(commissionRate)
TO_PARAMS(isGasTrade)
TO_PARAMS(gasTrade)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(sleeptime)
REQ_DUMP_END

//------------------------get_unstake_req
PARSE_REQ(getUnStakeReq)
PARSE_JSON(fromAddr)
PARSE_JSON(utxoHash)
PARSE_JSON(isGasTrade)
PARSE_JSON(gasTrade)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_REQ(getUnStakeReq)
TO_PARAMS(fromAddr)
TO_PARAMS(utxoHash)
TO_PARAMS(isGasTrade)
TO_PARAMS(gasTrade)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(sleeptime)
REQ_DUMP_END

//------------------------get_delegating_req
PARSE_REQ(getDelegateReq)
PARSE_JSON(fromAddr)
PARSE_JSON(assetType)
PARSE_JSON(toAddr)
PARSE_JSON(delegateAmount)
PARSE_JSON(delegateType)
PARSE_JSON(isGasTrade)
PARSE_JSON(gasTrade)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_REQ(getDelegateReq)
TO_JSON(fromAddr)
TO_JSON(assetType)
TO_JSON(toAddr)
TO_JSON(delegateAmount)
TO_JSON(delegateType)
TO_JSON(isGasTrade)
TO_JSON(gasTrade)
TO_JSON(isFindUtxo)
TO_JSON(txInfo)
TO_JSON(sleeptime)
REQ_DUMP_END

//------------------------getUndelegatereq
PARSE_REQ(getUndelegatereq)
PARSE_JSON(fromAddr)
PARSE_JSON(assetType)
PARSE_JSON(toAddr)
PARSE_JSON(utxoHash)
PARSE_JSON(isGasTrade)
PARSE_JSON(gasTrade)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_REQ(getUndelegatereq)
TO_PARAMS(fromAddr)
TO_PARAMS(assetType)
TO_PARAMS(toAddr)
TO_PARAMS(utxoHash)
TO_PARAMS(isGasTrade)
TO_PARAMS(gasTrade)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(sleeptime)
REQ_DUMP_END

//------------------------getBonusReq
PARSE_REQ(getBonusReq)
PARSE_JSON(addr)
PARSE_JSON(isGasTrade)
PARSE_JSON(gasTrade)
PARSE_JSON(isFindUtxo)
PARSE_JSON(assetType)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_REQ(getBonusReq)
TO_JSON(addr)
TO_JSON(isGasTrade)
TO_JSON(gasTrade)
TO_JSON(isFindUtxo)
TO_JSON(assetType)
TO_PARAMS(txInfo)
TO_PARAMS(sleeptime)
REQ_DUMP_END

PARSE_REQ(GetYieldInfoReq)
PARSE_END

DUMP_REQ(GetYieldInfoReq)
REQ_DUMP_END

PARSE_ACK(GetYieldInfoAck)
PARSE_JSON(annualizedRate)
PARSE_END

DUMP_ACK(GetYieldInfoAck)
TO_RESULT(annualizedRate)
DUMP_ACK_END

//------------------------get_stakeutxo_req
PARSE_REQ(get_stakeutxo_req)
PARSE_JSON(fromAddr)
PARSE_END

DUMP_REQ(get_stakeutxo_req)
TO_PARAMS(fromAddr)
REQ_DUMP_END


DUMP_ACK(get_stakeutxo_ack)
TO_RESULT(utxos)
DUMP_ACK_END

//------------------------get_UndelegateUtxo_req
PARSE_REQ(get_UndelegateUtxo_req)
PARSE_JSON(fromAddr)
PARSE_JSON(toAddr)
PARSE_JSON(assetType)
PARSE_END

DUMP_REQ(get_UndelegateUtxo_req)
TO_PARAMS(fromAddr)
TO_PARAMS(toAddr)
TO_PARAMS(assetType)
REQ_DUMP_END

DUMP_ACK(get_UndelegateUtxo_ack)
TO_RESULT(utxos)
DUMP_ACK_END



//------------------------confirm_transaction_req
PARSE_REQ(confirm_transaction_req)
PARSE_JSON(txhash)
PARSE_JSON(height)
PARSE_END

DUMP_REQ(confirm_transaction_req)
TO_PARAMS(txhash)
TO_PARAMS(height)
REQ_DUMP_END

DUMP_ACK(confirm_transaction_ack)
TO_RESULT(txhash)
TO_RESULT(percent)
TO_RESULT(sendsize)
TO_RESULT(receivedsize)
TO_RESULT(tx)
DUMP_ACK_END

//------------------------get_tx_info_req
PARSE_REQ(get_tx_info_req)
PARSE_JSON(txhash)
PARSE_END

DUMP_REQ(get_tx_info_req)
TO_JSON(txhash)
REQ_DUMP_END

PARSE_ACK(get_tx_info_ack)
PARSE_JSON(tx)
PARSE_JSON(blockhash)
PARSE_JSON(blockheight)
PARSE_END

DUMP_ACK(get_tx_info_ack)
TO_RESULT(tx)
TO_RESULT(blockhash)
TO_RESULT(blockheight)
DUMP_ACK_END

PARSE_REQ(getAllbonusInfoReq)
PARSE_END

DUMP_REQ(getAllbonusInfoReq)
REQ_DUMP_END

DUMP_ACK(getAllbonusInfoAck)
TO_RESULT(info)
DUMP_ACK_END

PARSE_REQ(get_all_stake_node_list_req)
PARSE_END

DUMP_REQ(get_all_stake_node_list_req)
REQ_DUMP_END

PARSE_ACK(get_all_stake_node_list_ack)
PARSE_END

DUMP_ACK(get_all_stake_node_list_ack)
TO_RESULT(list)
DUMP_ACK_END

//-------------------getblockheightrReq
PARSE_REQ(getblockheightrReq)
PARSE_END

DUMP_REQ(getblockheightrReq)
REQ_DUMP_END


PARSE_ACK(getblockheightrAck)
PARSE_JSON(height)
PARSE_END

DUMP_ACK(getblockheightrAck)
TO_RESULT(height)
DUMP_ACK_END

//-------------------getweb3clientversion
PARSE_REQ(getversionReq)
PARSE_END

DUMP_REQ(getversionReq)
REQ_DUMP_END

PARSE_ACK(getversionAck)
PARSE_JSON(netVersion)
PARSE_JSON(clientVersion)
PARSE_JSON(configVersion)
PARSE_JSON(dbVersion)
PARSE_END

DUMP_ACK(getversionAck)
TO_RESULT(netVersion)
TO_RESULT(clientVersion)
TO_RESULT(configVersion)
TO_RESULT(dbVersion)
DUMP_ACK_END

//------------------------getbalance
PARSE_REQ(balanceReq)
PARSE_JSON(addr)
PARSE_JSON(assetType)
PARSE_END

DUMP_REQ(balanceReq)
TO_PARAMS(addr)
TO_PARAMS(assetType)
REQ_DUMP_END

PARSE_ACK(balanceAck)
PARSE_JSON(balance)
PARSE_JSON(addr)
PARSE_JSON(assetType)
PARSE_END

DUMP_ACK(balanceAck)
TO_RESULT(balance)
TO_RESULT(addr)
TO_RESULT(assetType)
DUMP_ACK_END

//------------------------getblocktransactioncountbyhash
PARSE_REQ(getblocktransactioncountReq)
PARSE_JSON(blockHash)
PARSE_END

DUMP_REQ(getblocktransactioncountReq)
TO_PARAMS(blockHash)
REQ_DUMP_END

PARSE_ACK(getblocktransactioncountAck)
PARSE_JSON(txCount)
PARSE_END

DUMP_ACK(getblocktransactioncountAck)
TO_RESULT(txCount)
DUMP_ACK_END

//------------------------getaccounts
PARSE_REQ(getaccountsReq)
PARSE_END

DUMP_REQ(getaccountsReq)
REQ_DUMP_END

PARSE_ACK(getaccountsAck)
PARSE_JSON(acccountlist)
PARSE_END

DUMP_ACK(getaccountsAck)
TO_RESULT(acccountlist)
DUMP_ACK_END

//------------------------getchainid
PARSE_REQ(getchainidReq)
PARSE_END

DUMP_REQ(getchainidReq)
REQ_DUMP_END

PARSE_ACK(getchainidAck)
PARSE_JSON(chainId)
PARSE_END

DUMP_ACK(getchainidAck)
TO_RESULT(chainId)
DUMP_ACK_END

//------------------------getpeerlist
PARSE_REQ(getpeerlistReq)
PARSE_END

DUMP_REQ(getpeerlistReq)
REQ_DUMP_END

DUMP_ACK(getpeerlistAck)
TO_RESULT(nodeList)
TO_RESULT(size)
DUMP_ACK_END


PARSE_REQ(getTransactionInfoReq)
PARSE_JSON(txHash)
PARSE_END

DUMP_REQ(getTransactionInfoReq)
TO_JSON(txHash)
REQ_DUMP_END

DUMP_ACK(getTransactionInfoAck)
TO_RESULT(tx)
DUMP_ACK_END


PARSE_REQ(getBlockInfoByTxHashReq)
PARSE_JSON(txHash)
PARSE_END

DUMP_REQ(getBlockInfoByTxHashReq)
TO_JSON(txHash)
REQ_DUMP_END

DUMP_ACK(getBlockInfoByTxHashAck)
TO_RESULT(blockInfo)
DUMP_ACK_END


PARSE_REQ(getBlockInfoByHashReq)
PARSE_JSON(blockHash)
PARSE_END

DUMP_REQ(getBlockInfoByHashReq)
TO_JSON(blockHash)
REQ_DUMP_END

DUMP_ACK(getBlockInfoByHashAck)
TO_RESULT(blockInfo)
DUMP_ACK_END

PARSE_REQ(getBlockInfoByHeightReq)
PARSE_JSON(beginHeight)
PARSE_JSON(endHeight)
PARSE_END

DUMP_REQ(getBlockInfoByHeightReq)
TO_JSON(beginHeight)
TO_JSON(endHeight)
REQ_DUMP_END

DUMP_ACK(getBlockInfoByHeightAck)
TO_RESULT(blocks)
DUMP_ACK_END

PARSE_ACK(rpcAck)
PARSE_JSON(txHash)
PARSE_END

DUMP_ACK(rpcAck)
TO_RESULT(txHash)
TO_RESULT(percent)
TO_RESULT(sendsize)
TO_RESULT(receivedsize)
TO_RESULT(tx)
DUMP_ACK_END

PARSE_ACK(txAck)
PARSE_JSON(txJson)
PARSE_JSON(height)
PARSE_JSON(vrfJson)
PARSE_JSON(txType)
PARSE_JSON(time)
PARSE_JSON(gas)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_ACK(txAck)
TO_RESULT(txJson)
TO_RESULT(height)
TO_RESULT(vrfJson)
TO_RESULT(txType)
TO_RESULT(time)
TO_RESULT(gas)
TO_RESULT(sleeptime)
DUMP_ACK_END


PARSE_ACK(contractAck)
PARSE_JSON(contractJs)
PARSE_JSON(txJson)
PARSE_JSON(height)
PARSE_JSON(sleeptime)
PARSE_END


DUMP_ACK(contractAck)
TO_RESULT(contractJs)
TO_RESULT(txJson)
TO_RESULT(height)
TO_RESULT(sleeptime)
DUMP_ACK_END


PARSE_REQ(GetDelegateReq)
PARSE_JSON(addr)
PARSE_END

DUMP_REQ(GetDelegateReq)
TO_JSON(addr)
REQ_DUMP_END

PARSE_ACK(GetDelegateAck)
PARSE_JSON(info)
PARSE_END

DUMP_ACK(GetDelegateAck)
TO_RESULT(info)
DUMP_ACK_END


//------------------------getLockReq
PARSE_REQ(getLockReq)
PARSE_JSON(fromAddr)
PARSE_JSON(lockAmount)
PARSE_JSON(LockType)
PARSE_JSON(isGasTrade)
PARSE_JSON(gasTrade)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_REQ(getLockReq)
TO_PARAMS(fromAddr)
TO_PARAMS(lockAmount)
TO_PARAMS(LockType)
TO_PARAMS(isGasTrade)
TO_PARAMS(gasTrade)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(sleeptime)
REQ_DUMP_END

//------------------------getLockutxoReq
PARSE_REQ(getLockutxoReq)
PARSE_JSON(fromAddr)
PARSE_END

DUMP_REQ(getLockutxoReq)
TO_PARAMS(fromAddr)
REQ_DUMP_END


DUMP_ACK(getLockutxoAck)
TO_RESULT(utxos)
DUMP_ACK_END

//------------------------getUnLockReq
PARSE_REQ(getUnLockReq)
PARSE_JSON(fromAddr)
PARSE_JSON(utxoHash)
PARSE_JSON(isGasTrade)
PARSE_JSON(gasTrade)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_REQ(getUnLockReq)
TO_PARAMS(fromAddr)
TO_PARAMS(utxoHash)
TO_PARAMS(isGasTrade)
TO_PARAMS(gasTrade)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(sleeptime)
REQ_DUMP_END

PARSE_REQ(GetProposalReq)
PARSE_JSON(fromAddr)
PARSE_JSON(assetName)
PARSE_JSON(exchangeRate)
PARSE_JSON(duration)
PARSE_JSON(contractAddr)
PARSE_JSON(minVoteNum)
PARSE_JSON(title)
PARSE_JSON(Identifier)
PARSE_JSON(contractAddr)
PARSE_JSON(gasTrade)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END


DUMP_REQ(GetProposalReq)
TO_PARAMS(fromAddr)
TO_PARAMS(assetName)
TO_PARAMS(exchangeRate)
TO_PARAMS(duration)
TO_PARAMS(minVoteNum)
TO_PARAMS(title)
TO_PARAMS(Identifier)
TO_PARAMS(contractAddr)
TO_PARAMS(contractAddr)
TO_PARAMS(gasTrade)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(sleeptime)
REQ_DUMP_END


PARSE_REQ(getRevokeProposalReq)
PARSE_JSON(fromAddr)
PARSE_JSON(proposalHash)
PARSE_JSON(duration)
PARSE_JSON(minVoteNum)
PARSE_JSON(gasTrade)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_REQ(getRevokeProposalReq)
TO_PARAMS(fromAddr)
TO_PARAMS(proposalHash)
TO_PARAMS(duration)
TO_PARAMS(minVoteNum)
TO_PARAMS(gasTrade)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(sleeptime)
REQ_DUMP_END

PARSE_REQ(getFundReq)
PARSE_JSON(fromAddr)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_REQ(getFundReq)
TO_PARAMS(fromAddr)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(sleeptime)
REQ_DUMP_END

PARSE_REQ(getVoteReq)
PARSE_JSON(fromAddr)
PARSE_JSON(voteHash)
PARSE_JSON(voteTxType)
PARSE_JSON(pollType)
PARSE_JSON(gasTrade)
PARSE_JSON(isFindUtxo)
PARSE_JSON(txInfo)
PARSE_JSON(sleeptime)
PARSE_END

DUMP_REQ(getVoteReq)
TO_PARAMS(fromAddr)
TO_PARAMS(voteHash)
TO_PARAMS(voteTxType)
TO_PARAMS(pollType)
TO_PARAMS(gasTrade)
TO_PARAMS(isFindUtxo)
TO_PARAMS(txInfo)
TO_PARAMS(sleeptime)
REQ_DUMP_END


PARSE_REQ(getAssetTypeReq)
PARSE_END

DUMP_REQ(getAssetTypeReq)
REQ_DUMP_END

PARSE_ACK(getAssetTypeAck)
PARSE_JSON(assertType)
PARSE_END

DUMP_ACK(getAssetTypeAck)
TO_RESULT(assertType)
DUMP_ACK_END



PARSE_REQ(getAssetTypeInfoReq)
PARSE_JSON(assetType)
PARSE_END

DUMP_REQ(getAssetTypeInfoReq)
TO_PARAMS(assetType)
REQ_DUMP_END

PARSE_ACK(getAssetTypeInfoAck)
PARSE_JSON(proposalInfo)
PARSE_JSON(revokeProposalInfo)
PARSE_END

DUMP_ACK(getAssetTypeInfoAck)
TO_RESULT(proposalInfo)
TO_RESULT(revokeProposalInfo)
DUMP_ACK_END



PARSE_REQ(getVoteAddrsReq)
PARSE_JSON(hash)
PARSE_END

DUMP_REQ(getVoteAddrsReq)
TO_PARAMS(hash)
REQ_DUMP_END

PARSE_ACK(getVoteAddrsAck)
PARSE_JSON(approveAddrs)
PARSE_JSON(againstAddrs)
PARSE_END

DUMP_ACK(getVoteAddrsAck)
TO_RESULT(approveAddrs)
TO_RESULT(againstAddrs)
DUMP_ACK_END


PARSE_REQ(getVoteTxHashReq)
PARSE_JSON(hash)
PARSE_END

DUMP_REQ(getVoteTxHashReq)
TO_PARAMS(hash)
REQ_DUMP_END

PARSE_ACK(getVoteTxHashAck)
PARSE_JSON(txHashs)
PARSE_END

DUMP_ACK(getVoteTxHashAck)
TO_RESULT(txHashs)
DUMP_ACK_END


PARSE_REQ(getAddrTypeReq)
PARSE_JSON(addr)
PARSE_END

DUMP_REQ(getAddrTypeReq)
TO_PARAMS(addr)
REQ_DUMP_END

PARSE_ACK(getAddrTypeAck)
PARSE_JSON(isStaked)
PARSE_JSON(isLocked)
PARSE_JSON(isDelegated)
PARSE_JSON(isBonus)
PARSE_JSON(isQualified)
PARSE_JSON(isPackaged)
PARSE_END

DUMP_ACK(getAddrTypeAck)
TO_RESULT(isStaked)
TO_RESULT(isLocked)
TO_RESULT(isDelegated)
TO_RESULT(isBonus)
TO_RESULT(isQualified)
TO_RESULT(isPackaged)
DUMP_ACK_END


PARSE_REQ(getBonusAddrByDelegateAddrReq)
PARSE_JSON(addr)
PARSE_END

DUMP_REQ(getBonusAddrByDelegateAddrReq)
TO_PARAMS(addr)
REQ_DUMP_END

PARSE_ACK(getBonusAddrByDelegateAddrAck)
PARSE_JSON(assetType)
PARSE_JSON(bonusAddr)
PARSE_END

DUMP_ACK(getBonusAddrByDelegateAddrAck)
TO_RESULT(assetType)
TO_RESULT(bonusAddr)
DUMP_ACK_END