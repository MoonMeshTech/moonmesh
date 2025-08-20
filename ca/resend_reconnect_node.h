#ifndef RESEND_RECONNECT_ENABLED
#define RESEND_RECONNECT_ENABLED
#include <map>


#include "utils/account_manager.h"
#include "proto/block.pb.h"

#include "net/peer_node.h"
#include "net/msg_queue.h"

class ResendReconnectNode
{
public:
    ResendReconnectNode() = default;
    ~ResendReconnectNode() = default;

    int sendToAnotherNodeRequest(const BlockMsg & msg,const std::vector<std::string>& sendAddress);
    int DecisionBlock(const BlockMsg & msg);
    void AddResendCache(const BlockMsg & msg, const std::string blockHash,const std::string & addr);
    void RemoveResendBlock(const std::string & blockHash);

private:
    /*first is from*/
    std::mutex mResendMutex;
    std::set<std::string> resendCache;

};

int HandleSendReconnectNodeReq(const std::shared_ptr<SendReconnectNodeReq>& _msg, const MsgData &msgData);
#endif