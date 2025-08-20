/**
 * *****************************************************************************
 * @file        interface.h
 * @brief       
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef CA_INTERFACE_H_
#define CA_INTERFACE_H_

#include "proto/sdk.pb.h"
#include "utils/return_ack.h"
#include "proto/interface.pb.h"


/**
 * @brief       Obtaining the ID of your own node returns 0 successfully
 * 
 * @return      std::string 
 */
std::string NetGetSelfNodeId();

/**
 * @brief       Get the Block Req Impl object
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int getBlockRequestImplementation(const std::shared_ptr<GetBlockReq>& req, GetBlockAck & ack);

/**
 * @brief       Get the Balance Req Impl object
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int getBalanceRequestImpl(const std::shared_ptr<GetBalanceReq>& req, GetBalanceAck & ack);


/**
 * @brief       Stake list requests
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int getStakeListRequestImplementation(const std::shared_ptr<GetStakeListReq>& req, GetStakeListAck & ack);

/**
 * @brief       List of Delegatings
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetDelegateListReqImpl(const std::shared_ptr<GetDelegateListReq> &req, GetDelegateListAck &ack);

/**
 * @brief       utxo Get UTXO
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int getUtxoRequestImpl(const std::shared_ptr<GetUtxoReq>& req, GetUtxoAck & ack);

/**
 * @brief       Query all Delegating accounts and amounts on the delegatingee node
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetAllDelegatingAddressReqImpl(const std::shared_ptr<GetAllDelegateAddressReq>& req, GetAllDelegateAddressAck & ack);

/**
 * @brief       Get all delegatingable nodes
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int stakeNodeListRequestImpl(const std::shared_ptr<GetAllStakeNodeListReq>& req, GetAllStakeNodeListAck & ack);

/**
 * @brief       Get a list of signatures
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetSignCountListRequestImpl(const std::shared_ptr<GetSignCountListReq>& req, GetSignCountListAck & ack);

/**
 * @brief       Calculate the commission
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int getHeightRequestImplementation(const std::shared_ptr<GetHeightReq>& req, GetHeightAck & ack);

/**
 * @brief       Check the current claim amount
 * 
 * @param       req: 
 * @param       ack: 
 * @return      int 
 */
int GetBonusListRequestImpl(const std::shared_ptr<GetBonusListReq> & req, GetBonusListAck & ack);

/**
 * @brief       Query whether the transaction is linked
 * 
 * @param       msg: 
 * @param       ack: 
 * @return      int 
 */
int sendConfirmationTransactionRequest(const std::shared_ptr<ConfirmTransactionReq>& msg,  ConfirmTransactionAck & ack);

/**
 * @brief       Get the Rest Delegating Amount Req Impl object
 * 
 * @param       msg: 
 * @param       ack: 
 * @return      int 
 */
int GetRestdelegateAmountReqImpl(const std::shared_ptr<GetRestDelegateAmountReq>& msg,  GetRestDelegateAmountAck & ack);

/**
 * @brief       Get the block
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int handleBlockFetchRequest(const std::shared_ptr<GetBlockReq>& req, const MsgData & msgData);

/**
 * @brief       Get the balance
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int getBalanceRequest(const std::shared_ptr<GetBalanceReq>& req, const MsgData & msgData);

/**
 * @brief       Get node information
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int handleGetNodeInfoRequest(const std::shared_ptr<GetNodeInfoReq>& req, const MsgData& msgData);

/**
 * @brief       Stake list requests
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int handleGetStakeListRequest(const std::shared_ptr<GetStakeListReq>& req, const MsgData & msgData);

/**
 * @brief       List of Delegatings
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetDelegateListReq(const std::shared_ptr<GetDelegateListReq>& req, const MsgData & msgData);
/**
 * @brief       utxo Get UTXO
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int handleGetUtxoRequest(const std::shared_ptr<GetUtxoReq>& req, const MsgData & msgData);

/**
 * @brief       Query all Delegating accounts and amounts on the delegatingee node
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetAllDelegatingAddressReq(const std::shared_ptr<GetAllDelegateAddressReq>& req, const MsgData & msgData);

/**
 * @brief       Get all delegatingable nodes
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int handleGetAllStakeNodeListRequest(const std::shared_ptr<GetAllStakeNodeListReq>& req, const MsgData & msgData);

/**
 * @brief      Get a list of signatures 
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int handleGetSignCountListRequest(const std::shared_ptr<GetSignCountListReq>& req, const MsgData & msgData);

/**
 * @brief       Calculate the commission
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int getHeightRequest(const std::shared_ptr<GetHeightReq>& req, const MsgData & msgData);

/**
 * @brief       Check the current claim amount
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int processBonusListRequest(const std::shared_ptr<GetBonusListReq>& req, const MsgData & msgData);

/**
 * @brief       Query transaction chain up
 * 
 * @param       msg: 
 * @param       msgData: 
 * @return      int 
 */
int confirmTransactionRequestHandler(const std::shared_ptr<ConfirmTransactionReq>& msg, const MsgData & msgData);

/**
 * @brief       
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int HandleGetRestdelegateAmountReq(const std::shared_ptr<GetRestDelegateAmountReq>& req, const MsgData & msgData);

/**
 * @brief       
 * 
 */
void RegisterInterface();

/**
 * @brief       
 * 
 * @param       req: 
 * @param       msgData: 
 * @return      int 
 */
int getSdkAllNeedsRequest(const std::shared_ptr<GetSDKReq>& req, const MsgData & msgData);
#endif