/**
 * *****************************************************************************
 * @file        transaction.h
 * @brief       
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef CA_TRANSACTION
#define CA_TRANSACTION

#include <map>
#include <memory>
#include <thread>
#include <vector>
#include <regex>

#include "ca/global.h"
#include "ca/txhelper.h"
#include "ca/block_stroage.h"
#include "ca/block_monitor.h"
#include "ca/vrf_consensus.h"

#include "net/msg_queue.h"
#include "net/interface.h"

#include "utils/cycliclist.hpp"

#include "proto/block.pb.h"
#include "proto/transaction.pb.h"
#include "proto/block.pb.h"
#include "proto/ca_protomsg.pb.h"
#include "proto/interface.pb.h"
#include "mpt/trie.h"

/**
 * @brief       Get the Balance By Utxo object
 * 
 * @param       address: 
 * @param       balance: 
 * @return      int 
 */
int get_balance_by_utxo(const std::string & address, const std::string & assetType, uint64_t &balance);

typedef enum emTransactionType{
	TRANSACTION_TYPE_UNKNOWN = -1,	// Unknown
	kTransactionTypeGenesis = 0, 	// Genesis Deal
	kTransactionTypeTx,			// Normal trading
} TransactionType;

/**
 * @brief       Get the Transaction Type object
 * 
 * @param       tx: 
 * @return      TransactionType 
 */
TransactionType GetTransactionType(const CTransaction & tx);

/**
 * @brief       Receive transaction flow information
 * 
 * @param       msg: 
 * @param       msgData: 
 * @return      int 
 */
int HandleTx( const std::shared_ptr<TxMsgReq>& msg, const MsgData& msgData);
/**
 * @brief       Deal with contract transaction flow
 * 
 * @param       msg: 
 * @param       msgData: 
 * @return      int 
 */
int contractTransactionRequest( const std::shared_ptr<ContractTxMsgReq>& msg, const MsgData& msgData);
/**
 * @brief       Handle transaction flow
 * 
 * @param       msg: 
 * @param       outTx: 
 * @return      int 
 */
int handleTransaction( const std::shared_ptr<TxMsgReq>& msg, CTransaction & outTx);
/**
 * @brief       Receive block flow information
 * 
 * @param       msg: 
 * @param       msgData: 
 * @return      int 
 */
int HandleBlock(const std::shared_ptr<BlockMsg>& msg, const MsgData& msgData);

/**
 * @brief       Adding block signatures
 * 
 * @param       block: 
 * @return      int 
 */
int AddBlockSign(CBlock &block);

/**
 * @brief       Verifying block signature
 * 
 * @param       block: 
 * @return      int 
 */
int VerifyBlockSign(const CBlock &block);
/**
 * @brief       Handle block flows
 * 
 * @param       msg: 
 * @return      int 
 */
int handleBlock(const std::shared_ptr<BlockMsg>& msg, Node *node = nullptr);
/**
 * @brief       Processing block broadcast information
 * 
 * @param       msg: 
 * @param       msgData: 
 * @return      int 
 */
int handleBuildBlockBroadcastMessage( const std::shared_ptr<BuildBlockBroadcastMsg>& msg, const MsgData& msgData);
/**
 * @brief       Find multiple signing nodes
 * 
 * @param       tx: 
 * @param       msg: 
 * @param       nextNodes: 
 * @return      int 
 */
int find_sign_node(const CTransaction & tx, const std::shared_ptr<TxMsgReq> &msg, std::unordered_set<std::string> & nextNodes, const uint64_t& blockBuildHeight);

/**
 * @brief       Calculate the packager by time
 * 
 * @param       txTime: 
 * @param       txHeight: 
 * @param       packager: 
 * @param       proof:
 * @param       txHash:
 * @return      int 
 */
int calculatePackerByTime(const uint64_t& txTime, std::string& packager);
/**
 * @brief       Get the contract block packager
 * 
 * @param       txTime: 
 * @param       txHeight: 
 * @param       packager: 
 * @param       info:
 * @return      int 
 */
int get_contract_distribution_manager(const uint64_t& txTime, std::string& packager);
/**
 * @brief       Contract packager validation
 * 
 * @param       tx: 
 * @param       height: 
 * @param       vrfInfo: 
 * @return      int 
 */
int ContractVerifier(const CTransaction& tx);
/**
 * @brief       Find the pledged amount through the pledged address
 * 
 * @param       address: 
 * @param       stakeamount: 
 * @param       stakeType: 
 * @return      int 
 */
int SearchStake(const std::string &address, uint64_t &stakeamount,  global::ca::StakeType stakeType);
/**
 * @brief       Whether the transaction is issued by itself
 * 
 * @param       tx: 
 * @return      TxHelper::vrfAgentType 
 */
TxHelper::vrfAgentType requiresAgent(const CTransaction & tx);


/**
 * @brief       Whether the current address meets the qualification of depledging
 * 
 * @param       fromAddr: 
 * @param       utxoHash: 
 * @param       stakedAmount: 
 * @return      int 
 */
int IsQualifiedToUnstake(const std::string& fromAddr, const std::string& utxoHash, uint64_t& stakedAmount, const std::string& assetType);
int IsQualifiedToUnLock(const std::string& fromAddr, const std::string& utxoHash, uint64_t& lockedAmount, const std::string& assetType);
/**
 * @brief       Check whether the Delegating qualification is met
 * 
 * @param       fromAddr: 
 * @param       toAddr: 
 * @param       delegateAmount: 
 * @return      int 
 */
int CheckDelegatingQualification(const std::string& fromAddr, 
                        const std::string& toAddr, const std::string& assetType, uint64_t delegateAmount);
/**
 * @brief       Check if the bonus qualification is met
 * 
 * @param       BonusAddr: 
 * @param       txTime: 
 * @param       checkAnomaly: 
 * @return      int 
 */
int evaluateBonusEligibility(const std::string& BonusAddr, const uint64_t& txTime, bool checkAnomaly = true);


int CheckFundQualificationAndGetRewardAmount(const std::string& FundAddr, const uint64_t& txTime, uint64_t& lockedAmonut, bool checkAnomaly = true);

/**
 * @brief       Whether the solution Delegating qualification is met
 * 
 * @param       fromAddr: 
 * @param       toAddr: 
 * @param       utxoHash: 
 * @param       delegatingedAmount: 
 * @return      int 
 */
int IsQualifiedToUndelegating(const std::string& fromAddr, const std::string &assetType, const std::string& toAddr,
						const std::string& utxoHash, uint64_t& delegatingedAmount);

/**
 * @brief       Verify that the transaction has timed out
 * 
 * @param       tx: 
 * @return      int 
 */
int verifyTransactionTimeoutRequest(const CTransaction &tx);

/**
 * @brief
 * 
 * @param       tx: 
 * @return      int 
 */
int verifyTxUtxoHeight(const CTransaction &tx, const uint64_t& txUtxoHeight);

/**
 * @brief       Check time of the unstake, unstake time must be more than 30 days
 * 
 * @param       utxo: 
 * @return      true 
 * @return      false 
 */
bool isMoreThan30DaysForUnstake(const std::string& utxo);

/**
 * @brief       Check time of the redeem, redeem time must be more than 30 days,
 * 
 * @param       utxo: 
 * @return      true 
 * @return      false 
 */
bool IsMoreThan1DayForUndelegating(const std::string& utxo);
/**
 * @brief       Verify the Bonus Addr
 * 
 * @param       BonusAddr: 
 * @return      int 
 */
int VerifyBonusAddr(const std::string & BonusAddr);
/**
 * @brief       Get the Delegating Amount And Duration object
 * 
 * @param       bonusAddr: 
 * @param       curTime: 
 * @param       zeroTime: 
 * @param       mpDelegatingAddr2Amount: 
 * @return      int 
 */
int GetDelegatingAmountAndDuration(const std::string & bonusAddr,const uint64_t &curTime,const uint64_t &zeroTime,std::multimap<std::string, std::pair<uint64_t,uint64_t>> &mpDelegatingAddr2Amount);
/**
 * @brief       Get the Total Delegating Yesterday object
 * 
 * @param       curTime: 
 * @param       totaldelegating: 
 * @return      int 
 */
int GetTotalDelegatingYesterday(const uint64_t &curTime, uint64_t &totaldelegating);
/**
 * @brief       Get the Total Burn Yesterday object
 * 
 * @param       curTime: 
 * @param       total_brun: 
 * @return      int 
 */
int getTotalBurnAmountForYesterday(const uint64_t &curTime, uint64_t &total_brun);

/**
 * @brief       Notify the node of the height change
 * 
 */
void notifyNodeHeightChangeRequest();

/**
 * @brief       Validate the transaction information request
 * 
 * @param       msg: 
 * @return      int 
 */
int verifyTransactionMessageRequest(const TxMsgReq & msg);

/**
 * @brief       
 * 
 * @param       txMsg: 
 * @param       tx: 
 * @return      int 
 */
int DropShippingTransaction(const std::shared_ptr<TxMsgReq> & txMsg,const CTransaction &tx);

/**
 * @brief       
 * 
 * @param       txMsg: 
 * @param       tx: 
 * @param       addr:
 * @return      int 
 */
int DropShippingTransaction(const std::shared_ptr<TxMsgReq> &txMsg, const CTransaction &tx, const std::string& addr);
/**
 * @brief       
 * 
 * @param       txMsg: 
 * @param       tx: 
 * @return      int 
 */
int dropCallShippingRequest(const std::shared_ptr<ContractTxMsgReq> & txMsg,const CTransaction &tx);

/**
 * @brief       Calculate Gas
 * 
 * @param       tx: 
 * @param       gas: 
 * @return      int 
 */
int CalculateGas(const CTxUtxos &utxo,const std::string & txType, const uint64_t gasSize, uint64_t &gas);

/**
 * @brief       Generate Gas
 * 
 * @param       tx: 
 * @param       voutSize: 
 * @param       gas: 
 * @return      int 
 */
int GenerateGas(const CTxUtxos &utxo, const std::string & txType, const uint64_t voutSize, const uint64_t gasSize,uint64_t &gas);
int precalculatedGas(const CTransaction &tx, const uint64_t voutSize, uint64_t &gas);

/**
 * @brief       Verify that the vrf data source is correct
 * 
 * @param       tx: 
 * @return      int 
 */
bool txConsensusCheckStatus(const CTransaction &tx);
/**
 * @brief       Get the contract root hash
 * 
 * @param       contractAddress:
 * @param       rootHash: 
 * @param		contractDataStorage:
 * @return      int 
 */
int fetchContractRootHash(const std::string& contractAddress, std::string& rootHash, contractDataContainer* contractDataStorage);

/**
 * @brief       Determines if the current block is a contract block
 * 
 * @param       block: 
 * @return      return
 * @return		false
 */
bool isContractBlocked(const CBlock & block);
/**
 * @brief       Calculate Pack Node
 * 
 * @param       seed:
 * @param       targetAddrs: 
 * @return      string
 */
static std::string CalculationPackNode(const std::string seed, const std::vector<std::string> &targetAddrs);
/**
 * @brief       Find the contract packaging node
 * 
 * @param       txHash:
 * @param       utcTime: 
 * @param       targetAddr: 
 * @return      int
 */
int FindContractPackNode(const std::string & txHash, const uint64_t& utcTime, std::string &targetAddr);
/**
 * @brief       Verify the contract packaging node
 * 
 * @param       dispatchNodeAddr:
 * @param       randNum: 
 * @param       targetAddr: 
 * @param       vrfNodeList: 
 * @return      int
 */
int ContractPackVerifier(const std::string& seed, const uint64_t& time, const std::string& targetAddr);

/**
 * @brief       Get Contract Addr
 * 
 * @param       tx: tx
 * @return      std::string Contract Addr
 */
std::string GetContractAddr(const CTransaction & tx);
std::set<std::string> get_contract_pre_hash(const CBlock & block);

uint64_t getStake(const std::string& addr);

int GetStakeAmountByTime(const std::string& address, const uint64_t& time, uint64_t& total);
int GetdelegateAmountByTime(const std::string& address, const uint64_t& time, uint64_t& delegateAmount);
int64_t GetStakedelegateAmountByTime(const std::string& addr, const uint64_t& time);

template<typename RandomEngine>
void shuffleInput(std::vector<std::pair<std::string, VRFStructure>>& input, RandomEngine& engine);

std::vector<std::string> random_selection(std::vector<std::pair<std::string, VRFStructure>>& input, int numSelections, std::string seed);
int find_validation_nodes(const uint64_t& blockTime, const std::string &addr, std::vector<std::string>& validationAddrs);
int SearchNodeToSendMsg(BlockMsg &msg, const CBlock& cblock);
int VerifyBlockSignatureNodeSelection(const uint64_t& blockTime, const std::string& packagerAddr, const std::vector<std::string>& getValidationAddrs);
std::vector<std::pair<std::string, VRFStructure>> shuffleMap(const std::map<std::string, VRFStructure>& original_map, const std::string& seed);

int get_block_packager(std::string &packager, const std::string & txData, const uint64_t txTime);
int VerifyBlockPackager(const CTransaction& tx);
int CheckNodeValidity(const std::string &addr);

template <typename T>
std::vector<T> getRandomElements(const std::vector<T>& vec, size_t numElements) {
    if (numElements > vec.size()) {
        numElements = vec.size();
    }

    std::vector<size_t> indices(vec.size());
    for (size_t i = 0; i < vec.size(); ++i) {
        indices[i] = i;
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());

    std::shuffle(indices.begin(), indices.end(), gen);

    std::vector<T> randomElements;
    randomElements.reserve(numElements);
    for (size_t i = 0; i < numElements; ++i) {
        randomElements.push_back(vec[indices[i]]);
    }

    return randomElements;
}

std::string GetPackager(const CBlock & block);

int processCheckContractPreHashRequest(const std::shared_ptr<CheckContractPreHashReq> &msg, const MsgData &msgData);
int handleContractPreHashAck(const std::shared_ptr<CheckContractPreHashAck> &msg, const MsgData &msgData);

int CheckGasAssetsInput(std::string &_isGasAssets, std::pair<std::string, std::string> &gasAssets);
int CheckGasAssetsQualification(const std::pair<std::string, std::string> &gasAssets, const uint64_t &height);
#endif
