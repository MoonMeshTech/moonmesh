/**
 * *****************************************************************************
 * @file        ca.h
 * @brief       
 * @date        2023-09-26
 * @copyright   mm
 * *****************************************************************************
 */

#ifndef RPC_CREATE_TRANSACTION_HEADER
#define RPC_CREATE_TRANSACTION_HEADER

#include "ca/txhelper.h"
#include "rpc_tx.h"
#include <cstdint>

/**
    * @brief       
    * 
    * @param       fromAddr: 
    * @param       toAddr: 
    * @param       ack: 
    * @return      
    */

void createOrReplaceTransaction(const std::vector<TxHelper::TransTable> txAsset,const std::pair<std::string,std::string>& gasTrade,
                                bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo, txAck * ack);

/**
    * @brief       
    * 
    * @param       fromAddr: 
    * @param       stakeAmount: 
    * @param       stakeType: 
    * @param       ack: 
    * @return      
    */

void stakeTransactionUpdater(const std::string & fromAddr, uint64_t stakeAmount,  int32_t stakeType,const std::pair<std::string,std::string>& gasTrade,
                                bool isGasTrade, txAck* ack, double commissionRate, bool isFindUtxo, const std::string& encodedInfo);
/**
    * @brief       
    * 
    * @param       fromAddr: 
    * @param       utxoHash: 
    * @param       ack: 
    * @return      std::string 
    */


void ReplaceCreateUnstakeTransaction(const std::string& fromAddr, const std::string& utxoHash,const std::pair<std::string,std::string>& gasTrade,
                                                        bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);
/**
    * @brief       
    * 
    * @param       fromAddr: 
    * @param       toAddr: 
    * @param       delegateAmount: 
    * @param       delegateType: 
    * @param       ack: 
    * @return     
    */
void ReplaceCreateDelegatingTransaction(const std::string & fromAddr,const std::string assetType, const std::string& toAddr,
					uint64_t delegateAmount, int32_t delegateType,const std::pair<std::string,std::string>& gasTrade,bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);

/**
    * @brief       
    * 
    * @param       fromAddr: 
    * @param       toAddr: 
    * @param       utxoHash: 
    * @param       ack: 
    * @return      
    */

void ReplaceCreateUndelegatingTransaction(const std::string& fromAddr,const std::string &assetType, const std::string& toAddr, const std::string& utxoHash,
                        const std::pair<std::string,std::string>& gasTrade,bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);

/**
    * @brief       
    * 
    * @param       Addr: 
    * @param       ack: 
    * @return      
    */
void updateTransactionRequest(const std::string &addr, std::string assetType, std::pair<std::string, std::string> gasTrade, bool isGasTrade, bool isFindUtxo, const std::string &encodedInfo, txAck *ack);

/**
 * @brief
 *
 * @param       fromAddr:
 * @param       lockAmount:
 * @param       lockType:
 * @param       gasTrade:
 * @param       isGasTrade:
 * @param       isFindUtxo:
 * @param       encodedInfo:
 * @param       ack:
 */
void ReplaceCreateLockTransaction(const std::string & fromAddr, uint64_t lockAmount, int32_t lockType, const std::pair<std::string,std::string>& gasTrade,
                                                   bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);
/**
 * @brief       
 * 
 * @param       fromAddr: 
 * @param       utxoHash: 
 * @param       gasTrade: 
 * @param       isGasTrade: 
 * @param       isFindUtxo: 
 * @param       encodedInfo: 
 * @param       ack: 
 */
void ReplaceCreateUnLockTransaction(const std::string& fromAddr, const std::string& utxoHash,const std::pair<std::string,std::string>& gasTrade,
                                                    bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);
/**
 * @brief       
 * 
 * @param       fromAddr: 
 * @param       gasTrade: 
 * @param       beinTime: 
 * @param       endTime: 
 * @param       assetName: 
 * @param       rate: 
 * @param       contractAddr: 
 * @param       isFindUtxo: 
 * @param       encodedInfo: 
 * @param       ack: 
 */
void ReplaceCreateProposalTransaction(const std::string& fromAddr, const std::pair<std::string,std::string>& gasTrade, std::string& identifier, std::string& title,
                                                    uint64_t beinTime, uint64_t endTime, std::string assetName, std::string rate, std::string contractAddr, uint64_t minVote, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);
/**
 * @brief       
 * 
 * @param       fromAddr: 
 * @param       gasTrade: 
 * @param       beinTime: 
 * @param       endTime: 
 * @param       proposalHash: 
 * @param       isFindUtxo: 
 * @param       encodedInfo: 
 * @param       ack: 
 */
void ReplaceCreateRevokeProposalTransaction(const std::string& fromAddr, const std::pair<std::string,std::string>& gasTrade,
                                                    uint64_t beinTime, uint64_t endTime, std::string proposalHash, uint64_t minVote, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);
/**
 * @brief       
 * 
 * @param       fromAddr: 
 * @param       gasTrade: 
 * @param       voteHash: 
 * @param       pollType: 
 * @param       voteTxType: 
 * @param       isFindUtxo: 
 * @param       encodedInfo: 
 * @param       ack: 
 */
void ReplaceCreateVoteTransaction(const std::string& fromAddr, const std::pair<std::string,std::string>& gasTrade,
                                                    const std::string& voteHash, const int pollType, const global::ca::TxType& voteTxType, bool isFindUtxo, const std::string& encodedInfo, txAck* ack);

/**
    * @brief       
    * 
    * @param       Addr: 
    * @param       encodedInfo: 
    * @param       ack: 
    * @return      
    */

void ReplaceCreateFundTransaction(const std::string& addr, const std::string& encodedInfo,txAck* ack);

/**
    * @brief       
    * 
    * @param       outTx: 
    * @param       height: 
    * @param       info: 
    * @param       type: 
    * @return      int 
    */
int SendMessage(CTransaction & outTx,int height,Vrf &info,TxHelper::vrfAgentType type);



#endif
