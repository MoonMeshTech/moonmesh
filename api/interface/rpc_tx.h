#ifndef _RPC_TX_
#define _RPC_TX_
#include <string>
#include <vector>
#include <map>
#include <set>
#include "utils/base64.h"
#include <nlohmann/json.hpp>
#include "ca/txhelper.h"

#define REQ(name)\
	struct name{\
	nlohmann::json _parseToJsonObj(const std::string & json_str);\
	std::string _parseFromJson(const std::string& json);\
	std::string _parseToString();\
	std::string id;\
	std::string jsonrpc;\
	std::string params;

#define ACK(name)\
	struct name{\
	std::string _parseFromJson(const std::string& json);\
	std::string _parseToString();\
	std::string id;\
	std::string method;\
	std::string jsonrpc;\
	int code;\
	std::string message;\
	std::string result;
#define END };



REQ(the_top)
std::string identity; 
std::string height;
END

REQ(balance_req)
std::string addr;
END

ACK(balance_ack)
std::string addr;
std::string balance;
END


REQ(contract_info)
std::string name;
std::string language;
std::string languageVersion;
std::string standard;
std::string logo;
std::string source;
std::string ABI;
std::string userDoc;
std::string developerDoc;
std::string compilerVersion;
std::string compilerOptions;
std::string srcMap;
std::string srcMapRuntime;
std::string metadata;
std::string other;
END



REQ(deploy_contract_req)
std::string addr;
std::string nContractType;
std::string info;
std::string contract;
std::string data;
std::string pubstr;
std::pair<std::string, std::string> gasTrade;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END


REQ(deploy_utxo_req)
std::string addr;
END


ACK(deploy_utxo_ack)
std::vector<std::string> utxos;
END



REQ(call_contract_req)
std::string addr;
std::string deployer;
std::string deployutxo;
std::string args;
std::string pubstr;
std::string money;
std::string istochain;
bool isGasTrade;
std::pair<std::string, std::string> gasTrade;
std::string contractAddress;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END


ACK(call_contract_ack)

};

REQ(deployers_req)
END

ACK(deployers_ack)
std::vector<std::string> deployers;
END



REQ(tx_req)
bool isGasTrade;
std::pair<std::string,std::string> gasTrade;
std::vector<TxHelper::TransTable> txAsset;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END

REQ(getStakeReq)
std::string fromAddr;
std::string stakeAmount;
std::string StakeType;
std::string commissionRate;
bool isGasTrade;
std::pair<std::string,std::string> gasTrade;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END


REQ(getUnStakeReq)
std::string fromAddr;
std::string utxoHash;
bool isGasTrade;
std::pair<std::string,std::string> gasTrade;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END

REQ(getDelegateReq)
std::string fromAddr;
std::string assetType;
std::string toAddr;
std::string delegateAmount;
std::string delegateType;
bool isGasTrade;
std::pair<std::string,std::string> gasTrade;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END

REQ(getUndelegatereq)
std::string fromAddr;
std::string assetType;
std::string toAddr;
std::string utxoHash;
bool isGasTrade;
std::pair<std::string,std::string> gasTrade;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END

REQ(get_declare_req)
std::string fromAddr;
std::string toAddr;
std::string amount;
std::string multiSignPub;
std::vector<std::string> signAddrList;
std::string signThreshold;
END

REQ(getBonusReq)
std::string addr;
bool isGasTrade;
std::pair<std::string,std::string> gasTrade;
bool isFindUtxo;
std::string txInfo;
std::string assetType;
std::string sleeptime;
END

REQ(get_stakeutxo_req)
std::string fromAddr;
END

ACK(get_stakeutxo_ack)
nlohmann::json utxos;
END

REQ(rsa_code)
std::string isEcode;
std::string strEncTxt;
std::string cipher_text;
std::string sign_message;
std::string strpub;
END

ACK(rsa_pubstr_ack)
std::string rsa_pubstr;
END

REQ(get_UndelegateUtxo_req)
std::string fromAddr;
std::string toAddr;
std::string assetType;
END


ACK(get_UndelegateUtxo_ack)
nlohmann::json utxos;
END


REQ(confirm_transaction_req)
std::string txhash;
std::string height;
END

ACK(confirm_transaction_ack)
nlohmann::json txhash;
nlohmann::json percent;
nlohmann::json sendsize;
nlohmann::json receivedsize;
nlohmann::json tx;
END


REQ(get_tx_info_req)
std::string txhash;
END

ACK(get_tx_info_ack)
std::string tx;
uint64_t blockheight;
std::string blockhash;
END


REQ(getAllbonusInfoReq)
END

ACK(getAllbonusInfoAck)
nlohmann::json info;
END

REQ(get_all_stake_node_list_req)
END

ACK(get_all_stake_node_list_ack)
nlohmann::json list;
END

REQ(getblockheightrReq)
END

ACK(getblockheightrAck)
std::string height;
END

REQ(getversionReq)
END

ACK(getversionAck)
std::string netVersion;
std::string clientVersion;
std::string configVersion;
std::string dbVersion;
END

REQ(balanceReq)
std::string addr;
std::string assetType;
END

ACK(balanceAck)
std::string balance;
std::string addr;
std::string assetType;
END

REQ(getblocktransactioncountReq)
std::string blockHash;
END

ACK(getblocktransactioncountAck)
std::string txCount;
END

REQ(getaccountsReq)
END

ACK(getaccountsAck)
std::vector<std::string> acccountlist;
END

REQ(getchainidReq)
END

ACK(getchainidAck)
std::string chainId;
END

REQ(getpeerlistReq)
END

ACK(getpeerlistAck)
nlohmann::json nodeList;
int size;
END


REQ(getTransactionInfoReq)
std::string txHash;
END

ACK(getTransactionInfoAck)
nlohmann::json tx;
END


REQ(getBlockInfoByTxHashReq)
std::string txHash;
END

ACK(getBlockInfoByTxHashAck)
nlohmann::json blockInfo;
END



REQ(getBlockInfoByHashReq)
std::string blockHash;
END

ACK(getBlockInfoByHashAck)
nlohmann::json blockInfo;
END


REQ(getBlockInfoByHeightReq)
std::string beginHeight;
std::string endHeight;
END

ACK(getBlockInfoByHeightAck)
std::vector<nlohmann::json> blocks;
END

ACK(rpcAck)
std::string txHash;
std::string percent;
std::string sendsize;
std::string receivedsize;
std::string tx;
END

ACK(txAck)
std::string txJson;
std::string height;
std::string vrfJson;
std::string txType;
std::string time;
std::string gas;
std::string sleeptime;
END


ACK(contractAck)
std::string contractJs;
std::string txJson;
std::string height;
std::string sleeptime;
END

REQ(GetYieldInfoReq)
END

ACK(GetYieldInfoAck)
std::string annualizedRate;
END

REQ(GetDelegateReq)
std::string addr;
END

ACK(GetDelegateAck)
std::map<std::string, std::vector<std::pair<std::string, std::string>>> info;
END


REQ(getLockReq)
std::string fromAddr;
std::string lockAmount;
std::string LockType;
bool isGasTrade;
std::pair<std::string,std::string> gasTrade;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END

REQ(getLockutxoReq)
std::string fromAddr;
END

ACK(getLockutxoAck)
std::pair<std::string, uint64_t> utxos;
END

REQ(getUnLockReq)
std::string fromAddr;
std::string utxoHash;
bool isGasTrade;
std::pair<std::string,std::string> gasTrade;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END


REQ(GetProposalReq)
std::string fromAddr;
std::string assetName;
std::string exchangeRate;
std::string duration;
std::string contractAddr;
std::string minVoteNum;
std::string title;
std::string Identifier;
std::pair<std::string,std::string> gasTrade;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END

REQ(getRevokeProposalReq)
std::string fromAddr;
std::string proposalHash;
std::string duration;
std::string minVoteNum;
std::pair<std::string,std::string> gasTrade;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END

REQ(getVoteReq)
std::string fromAddr;
std::string voteHash;
std::string voteTxType;
std::string pollType;
std::pair<std::string,std::string> gasTrade;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END


REQ(getFundReq)
std::string fromAddr;
bool isFindUtxo;
std::string txInfo;
std::string sleeptime;
END

REQ(getAssetTypeReq)
END
ACK(getAssetTypeAck)
std::vector<std::string> assertType;
END


REQ(getAssetTypeInfoReq)
std::string assetType;
END

ACK(getAssetTypeInfoAck)
std::string proposalInfo;
std::vector<std::string> revokeProposalInfo;
END


REQ(getVoteAddrsReq)
std::string hash;
END

ACK(getVoteAddrsAck)
std::map<std::string, uint64_t> approveAddrs;
std::map<std::string, uint64_t> againstAddrs;
END

REQ(getVoteTxHashReq)
std::string hash;
END

ACK(getVoteTxHashAck)
std::vector<std::string> txHashs;
END

REQ(getAddrTypeReq)
std::string addr;
END

ACK(getAddrTypeAck)
bool isStaked;
bool isLocked;
bool isDelegated;
bool isBonus;
bool isQualified;
bool isPackaged;
END


REQ(getBonusAddrByDelegateAddrReq)
std::string addr;
END

ACK(getBonusAddrByDelegateAddrAck)
std::string assetType;
std::string bonusAddr;
END

#endif _RPC_TX_

