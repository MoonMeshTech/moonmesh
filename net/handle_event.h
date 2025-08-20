/**
 * *****************************************************************************
 * @file        handle_event.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef HANDLE_EVENT_H
#define HANDLE_EVENT_H

#include <memory>

#include "./msg_queue.h"
#include "../utils/cycliclist.hpp"
#include "../common/task_pool.h"
#include "../proto/net.pb.h"
#include "../proto/ca_protomsg.pb.h"
#include "../proto/interface.pb.h"


/**
 * @brief       
 * 
 * @param       requestToPrintMessage 
 * @param       from 
 * @return      int 
 */
int processPrintMessageRequest(const std::shared_ptr<PrintMsgReq> &requestToPrintMessage, const MsgData &from);

/**
 * @brief       
 * 
 * @param       nodeinfo 
 * @param       fromIp 
 * @param       fromPort 
 * @return      int 
 */

/**
 * @brief       
 * 
 * @param       nodeinfo 
 * @param       fromIp 
 * @param       fromPort 
 * @return      int 
 */
int RegistrationVerifier(const NodeInfo &nodeinfo, uint32_t &fromIp, uint32_t &fromPort);

/**
 * @brief       
 * 
 * @param       msg
 * @param       target_address_cycle_list  
 */
void initializeCyclicTargetAddresses(const std::shared_ptr<BuildBlockBroadcastMsg>& msg, Cycliclist<std::string> & target_address_cycle_list);

/**
 * @brief       
 * 
 * @param       cyclicNodeList
 */
void initialize_cyclic_node_list(Cycliclist<std::string>& cyclicNodeList);

/**
 * @brief       
 * 
 * @param       msg 
 * @param       msgData 
 * @return      int 
 */
int HandleBroadcastMsg( const std::shared_ptr<BuildBlockBroadcastMsg>& msg, const MsgData& msgData);


/**
 * @brief       
 * 
 * @param       registerNode 
 * @param       from 
 * @return      int 
 */
int handleRegisterNodeRequest(const std::shared_ptr<RegisterNodeReq> &registerNode, const MsgData &from);
/**
 * @brief       
 * 
 * @param       registerNodeAcknowledgment 
 * @param       from 
 * @return      int 
 */
int handleRegisterNodeAcknowledgment(const std::shared_ptr<RegisterNodeAck> &registerNodeAcknowledgment, const MsgData &from);

/**
 * @brief       
 * 
 * @param       pingReq 
 * @param       from 
 * @return      int 
 */
int HandlePingReq(const std::shared_ptr<PingReq> &pingReq, const MsgData &from);
/**
 * @brief       
 * 
 * @param       pongReq 
 * @param       from 
 * @return      int 
 */
int handle_pong_request(const std::shared_ptr<PongReq> &pongReq, const MsgData &from);

/**
 * @brief       
 * 
 * @param       nodeSynchronizationRequestMessage 
 * @param       from 
 * @return      int 
 */
int handleSyncNodeRequest(const std::shared_ptr<SyncNodeReq> &nodeSynchronizationRequestMessage, const MsgData &from);
/**
 * @brief       
 * 
 * @param       syncNodeAcknowledgment 
 * @param       from 
 * @return      int 
 */
int handleSyncNodeAcknowledgment(const std::shared_ptr<SyncNodeAck> &syncNodeAcknowledgment, const MsgData &from);

/**
 * @brief       
 * 
 * @param       echoReq 
 * @param       from 
 * @return      int 
 */
int HandleEchoReq(const std::shared_ptr<EchoReq> &echoReq, const MsgData &from);
/**
 * @brief       
 * 
 * @param       echoAck 
 * @param       from 
 * @return      int 
 */
int handleEchoAcknowledge(const std::shared_ptr<EchoAck> &echoAck, const MsgData &from);

/**
 * @brief       
 * 
 * @param       testReq 
 * @param       from 
 * @return      int 
 */
int networkTestRequest(const std::shared_ptr<TestNetReq> &testReq, const MsgData &from);
/**
 * @brief       
 * 
 * @param       testAck 
 * @param       from 
 * @return      int 
 */
int networkTestAcknowledge(const std::shared_ptr<TestNetAck> &testAck, const MsgData &from);

/**
 * @brief       
 * 
 * @param       req 
 * @param       from 
 * @return      int 
 */
int handleCheckTransactionRequest(const std::shared_ptr<CheckTxReq>& req, const MsgData& from);
/**
 * @brief       
 * 
 * @param       ack 
 * @param       from 
 * @return      int 
 */
int handleCheckTransactionAck(const std::shared_ptr<CheckTxAck>& ack, const MsgData& from);

/**
 * @brief       
 * 
 * @param       req 
 * @param       from 
 * @return      int 
 */
int processNodeHeightChangeRequest(const std::shared_ptr<NodeHeightChangedReq>& req, const MsgData& from);
/**
 * @brief       
 * 
 * @param       req 
 * @param       from 
 * @return      int 
 */
int handleNodeAddressChangeRequest(const std::shared_ptr<NodeAddrChangedReq>& req, const MsgData& from);

int handleGetTxUtxoHashRequest(const std::shared_ptr<GetUtxoHashReq>& req, const MsgData& from);
int handleGetTxUtxoHashAcknowledgment(const std::shared_ptr<GetUtxoHashAck>& ack, const MsgData& from);

#endif
