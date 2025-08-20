/**
 * *****************************************************************************
 * @file        dispatcher.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   mm
 * *****************************************************************************
 */

#ifndef IP_DISPATCHER_HEADER
#define IP_DISPATCHER_HEADER

#include <functional>
#include <map>

#include "./msg_queue.h"
#include "../common/protobuf_define.h"

class ProtobufDispatcher
{
public:
    /**
     * @brief       
     * 
     * @param       data 
     * @return      int 
     */
    int Handle(const MsgData &data);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void registerCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void NetRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);
    
    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void registerBroadcastCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void TxRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);
    
    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void registerSyncBlockCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void registerSaveBlockCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       cb 
     */
    template <typename T>
    void blockRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb);

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void unregisterCallback();

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void NetworkUnregisterRequest();

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void unregister_broadcast_callback();

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void txUnregisterCallback_();

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void unregisterSyncBlockCallback();

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void unregisterSaveBlockCallback();

    /**
     * @brief       
     * 
     * @tparam T 
     */
    template <typename T>
    void blockUnregisterCallback();

    /**
     * @brief       
     * 
     * @param       oss 
     */
    void TaskInfo(std::ostringstream& oss);
private:
    /**
     * @brief       
     * 
     * @param       where 
     * @return      std::string 
     */
    friend std::string PrintCache(int where);

    std::map<const std::string, ProtoCallBack> chainProtocolCallbacks;
    std::map<const std::string, ProtoCallBack> netProtocolCallbacks;
    std::map<const std::string, ProtoCallBack> broadcastProtocol;
    std::map<const std::string, ProtoCallBack> txProtocolCallbacks;
    std::map<const std::string, ProtoCallBack> syncBlockProtocolCallbacks;
    std::map<const std::string, ProtoCallBack> saveBlockProtocolCallbacks;
    std::map<const std::string, ProtoCallBack> blockProtocolCallbacks;
};

template <typename T>
void ProtobufDispatcher::registerCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    chainProtocolCallbacks[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}

template <typename T>
void ProtobufDispatcher::NetRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    netProtocolCallbacks[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}


template <typename T>
void ProtobufDispatcher::registerBroadcastCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    broadcastProtocol[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}

template <typename T>
void ProtobufDispatcher::TxRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    txProtocolCallbacks[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}

template <typename T>
void ProtobufDispatcher::registerSyncBlockCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    syncBlockProtocolCallbacks[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}

template <typename T>
void ProtobufDispatcher::registerSaveBlockCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    saveBlockProtocolCallbacks[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}

template <typename T>
void ProtobufDispatcher::blockRegisterCallback(std::function<int(const std::shared_ptr<T> &msg, const MsgData &from)> cb)
{
    blockProtocolCallbacks[T::descriptor()->name()] = [cb](const MessagePtr &msg, const MsgData &from)->int
    {
        return cb(std::static_pointer_cast<T>(msg), from);
    };
}

template <typename T>
void ProtobufDispatcher::unregisterCallback()
{
    chainProtocolCallbacks.erase(T::descriptor()->name());
}
template <typename T>
void ProtobufDispatcher::NetworkUnregisterRequest()
{
    netProtocolCallbacks.erase(T::descriptor()->name());
}

template <typename T>
void ProtobufDispatcher::unregister_broadcast_callback()
{
    broadcastProtocol.erase(T::descriptor()->name());
}

template <typename T>
void ProtobufDispatcher::txUnregisterCallback_()
{
    txProtocolCallbacks.erase(T::descriptor()->name());
}

template <typename T>
void ProtobufDispatcher::unregisterSyncBlockCallback()
{
    syncBlockProtocolCallbacks.erase(T::descriptor()->name());
}

template <typename T>
void ProtobufDispatcher::unregisterSaveBlockCallback()
{
    saveBlockProtocolCallbacks.erase(T::descriptor()->name());
}

template <typename T>
void ProtobufDispatcher::blockUnregisterCallback()
{
    blockProtocolCallbacks.erase(T::descriptor()->name());
}
#endif
