#ifndef NET_INTERFACE_HEADER_GUARD
#define NET_INTERFACE_HEADER_GUARD


#include <iostream>
#include <map>
#include <vector>
#include <unordered_map>
#include "../net/api.h"
#include "../net/msg_queue.h"
#include "../net/dispatcher.h"

static bool NetInit()
{
    // register
    MagicSingleton<TaskPool>::GetInstance()->initializeTaskPool();
    
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<RegisterNodeReq>(handleRegisterNodeRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<RegisterNodeAck>(handleRegisterNodeAcknowledgment);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<SyncNodeReq>(handleSyncNodeRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<SyncNodeAck>(handleSyncNodeAcknowledgment);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<CheckTxReq>(handleCheckTransactionRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<CheckTxAck>(handleCheckTransactionAck);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<GetUtxoHashReq>(handleGetTxUtxoHashRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<GetUtxoHashAck>(handleGetTxUtxoHashAcknowledgment);
    
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<PrintMsgReq>(processPrintMessageRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<PingReq>(HandlePingReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<PongReq>(handle_pong_request);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<EchoReq>(HandleEchoReq);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<EchoAck>(handleEchoAcknowledge);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<TestNetAck>(networkTestAcknowledge);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<TestNetReq>(networkTestRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<NodeHeightChangedReq>(processNodeHeightChangeRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<NodeAddrChangedReq>(handleNodeAddressChangeRequest);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<KeyExchangeRequest>(handleKeyExchangeRequest);
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetRegisterCallback<KeyExchangeResponse>(handleKeyExchangeAcknowledgment);

    MagicSingleton<ProtobufDispatcher>::GetInstance()->registerBroadcastCallback<BuildBlockBroadcastMsg>(HandleBroadcastMsg);

    net_com::InitializeNetwork();
    return true;
}

/**
 * @brief    Single point to send information, T type is the type of protobuf protocol   
 * 
 */
template <typename T>
bool NetSendMessage(const std::string & id, 
                        const T & msg, 
                        const net_com::Compress isCompress, 
                        const net_com::Encrypt isEncrypt, 
                        const net_com::Priority priority)
{
    return net_com::SendMessage(id, msg, isCompress, isEncrypt, priority);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const std::string & id, 
                        const T & msg)
{
    return net_com::SendMessage(id, msg, net_com::Compress::kCompressDisabled, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::kPriorityLow0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const std::string & id, 
                        const T & msg, 
                        const net_com::Compress isCompress)
{
    return net_com::SendMessage(id, msg, isCompress, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::kPriorityLow0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const std::string & id, 
                        const T & msg, 
                        const net_com::Encrypt isEncrypt)
{
    return net_com::SendMessage(id, msg, net_com::Compress::kCompressDisabled, isEncrypt, net_com::Priority::kPriorityLow0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const std::string & id, 
                        const T & msg, 
                        const net_com::Priority priority)
{
    return net_com::SendMessage(id, msg, net_com::Compress::kCompressDisabled, net_com::Encrypt::ENCRYPT_FALSE, priority);
}



/**
 * @brief    To send information with a receipt address, type T is the type of protobuf protocol 
 * 
 */
template <typename T>
bool NetSendMessage(const MsgData & from, 
                        const T & msg, 
                        const net_com::Compress isCompress, 
                        const net_com::Encrypt isEncrypt, 
                        const net_com::Priority priority)
{
    return net_com::SendMessage(from, msg, isCompress, isEncrypt, priority);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const MsgData & from, 
                        const T & msg)
{
    return net_com::SendMessage(from, msg, net_com::Compress::kCompressDisabled, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::kPriorityLow0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const MsgData & from, 
                        const T & msg, 
                        const net_com::Compress isCompress)
{
    return net_com::SendMessage(from, msg, isCompress, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::kPriorityLow0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const MsgData & from, 
                        const T & msg, 
                        const net_com::Encrypt isEncrypt)
{
    return net_com::SendMessage(from, msg, net_com::Compress::kCompressDisabled, isEncrypt, net_com::Priority::kPriorityLow0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetSendMessage(const MsgData & from, 
                        const T & msg,
                        const net_com::Priority priority)
{
    return net_com::SendMessage(from, msg, net_com::Compress::kCompressDisabled, net_com::Encrypt::ENCRYPT_FALSE, priority);
}

/**
 * @brief      Broadcast information
 * 
 */
template <typename T>
bool NetBroadcastMessage(const T& msg, 
                            const net_com::Compress isCompress, 
                            const net_com::Encrypt isEncrypt, 
                            const net_com::Priority priority)
{
    return net_com::BroadCastMessage(msg, isCompress, isEncrypt, priority);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetBroadcastMessage(const T& msg)
{
    return net_com::BroadCastMessage(msg, net_com::Compress::kCompressDisabled, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::kPriorityLow0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetBroadcastMessage(const T& msg, 
                            const net_com::Compress isCompress)
{
    return net_com::BroadCastMessage(msg, isCompress, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::kPriorityLow0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetBroadcastMessage(const T& msg, 
                            const net_com::Encrypt isEncrypt)
{
    return net_com::BroadCastMessage(msg, net_com::Compress::kCompressDisabled, isEncrypt, net_com::Priority::kPriorityLow0);
}

/**
 * @brief       
 * 
 */
template <typename T>
bool NetBroadcastMessage(const T& msg, 
                            const net_com::Priority priority)
{
    return net_com::BroadCastMessage(msg, net_com::Compress::kCompressDisabled, net_com::Encrypt::ENCRYPT_FALSE, priority);
}


/**
 * @brief       Send node which was changed
 * 
 */

static void nodeHeightChangedMessage()
{
	net_com::nodeHeightChanged();
}




/**
 * @brief       
 * 
 */
template <typename T>
void NetworkUnregisterRequest()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->NetworkUnregisterRequest<T>();
}

/**
 * @brief       
 * 
 */
template <typename T>
void unregisterCallback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->unregisterCallback<T>();
}

/**
 * @brief       
 * 
 */
template <typename T>
void unregister_broadcast_callback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->unregister_broadcast_callback<T>();
}

/**
 * @brief       
 * 
 */
template <typename T>
void txUnregisterCallback_()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->txUnregisterCallback_<T>();
}

/**
 * @brief       
 * 
 */
template <typename T>
void unregisterSyncBlockCallback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->unregisterSyncBlockCallback<T>();
}

/**
 * @brief       
 * 
 */
template <typename T>
void unregisterSaveBlockCallback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->unregisterSaveBlockCallback<T>();
}

/**
 * @brief       
 * 
 */
template <typename T>
void blockUnregisterCallback()
{
    MagicSingleton<ProtobufDispatcher>::GetInstance()->blockUnregisterCallback<T>();
}

#endif

