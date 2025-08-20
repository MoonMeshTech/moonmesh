#include "resend_reconnect_node.h"
#include "common/global_data.h"
#include "ca/transaction.h"


int ResendReconnectNode::sendToAnotherNodeRequest(const BlockMsg & msg,const std::vector<std::string>& sendAddress)
{
	std::string msgId;
    SendReconnectNodeReq req;

    BlockMsg * blockmsg = req.mutable_msg();
	*blockmsg = msg;
	for(auto & nodeId : sendAddress)
	{
		req.add_sendaddress(nodeId);
	}

	std::vector<Node> nodelist =  MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	std::random_shuffle(nodelist.begin(),nodelist.end()); 

	std::vector<Node> verifyNode = getRandomElements(nodelist,(size_t)nodelist.size()/4);
	for(auto &node : verifyNode)
	{
		NetSendMessage<SendReconnectNodeReq>(node.address, req, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
		DEBUGLOG("send another node is : {} ",node.address);
	}

	return 0;
}


int ResendReconnectNode::DecisionBlock(const BlockMsg & msg)
{
	std::unique_lock<std::mutex> lck(mResendMutex);
	CBlock cblock;
	if (!cblock.ParseFromString(msg.block()))
	{
		ERRORLOG("fail to serialization!!");
		return -1;
	}

	auto iter = std::find(resendCache.begin(),resendCache.end(),cblock.hash());
	if(iter != resendCache.end() && cblock.sign_size() == 2)
	{
		NetSendMessage<BlockMsg>(GenerateAddr(cblock.sign(0).pub()), msg, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
		return 1;
	}

	return 0;	
}

void ResendReconnectNode::RemoveResendBlock(const std::string & blockHash)
{
	std::unique_lock<std::mutex> lck(mResendMutex);
	auto iter = std::find(resendCache.begin(),resendCache.end(),blockHash);
	if(iter != resendCache.end())
	{
		resendCache.erase(iter);
	}
	return;	
}

void ResendReconnectNode::AddResendCache(const BlockMsg & msg, const std::string blockHash,const std::string & addr)
{
	std::unique_lock<std::mutex> lck(mResendMutex);
	resendCache.insert(blockHash);
	NetSendMessage<BlockMsg>(addr, msg, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
	return;
}

int HandleSendReconnectNodeReq(const std::shared_ptr<SendReconnectNodeReq>& _msg, const MsgData &msgData)
{
	SendReconnectNodeAck ack;
	BlockMsg * blockmsg  = ack.mutable_msg();
	*blockmsg = _msg->msg();

	std::vector<Node> nodelist =  MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	std::vector<std::string> node_list_address;
	for(const auto & node : nodelist)
	{
		node_list_address.push_back(node.address);
	}

	CBlock cblock;
	if (!cblock.ParseFromString(blockmsg->block()))
	{
		ERRORLOG("fail to serialization!!");
		return -1;
	}

	for(const auto & addr : _msg->sendaddress())
	{
		auto iter = std::find(node_list_address.begin(),node_list_address.end(),addr);
		if(iter != node_list_address.end())
		{
			MagicSingleton<ResendReconnectNode>::GetInstance()->AddResendCache(*blockmsg,cblock.hash(),addr);			
		}
	}

	return 0;
}

