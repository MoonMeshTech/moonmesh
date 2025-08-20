/**
 * *****************************************************************************
 * @file        api.h
 * @brief       
 * @author  ()
 * @date        2023-09-25
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef NETWORK_API_HEADER_GUARD
#define NETWORK_API_HEADER_GUARD

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <assert.h>
#include <netinet/tcp.h>

#include <iostream>
#include <string>
#include <random>

#include "./peer_node.h"
#include "./ip_port.h"
#include "./pack.h"
#include "./socket_buf.h"
#include "./global.h"
#include "./handle_event.h"

#include "../common/config.h"
#include "../common/global.h"
#include "../include/logging.h"
#include "../proto/common.pb.h"
#include "../utils/util.h"
#include "../proto/ca_protomsg.pb.h"
#include "key_exchange.h"
/**
 * @brief       
 * 
 */
namespace net_tcp
{
	/**
	 * @brief       
	 * 
	 * @param       fd 
	 * @param       sa 
	 * @param       salenptr 
	 * @return      int 
	 */
	int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr);
	
	/**
	 * @brief       
	 * 
	 * @param       fd 
	 * @param       sa 
	 * @param       salen 
	 * @return      int 
	 */
	int Bind(int fd, const struct sockaddr *sa, socklen_t salen);

	/**
	 * @brief       
	 * 
	 * @param       fd 
	 * @param       sa 
	 * @param       salen 
	 * @return      int 
	 */
	int Connect(int fd, const struct sockaddr *sa, socklen_t salen);

	/**
	 * @brief       
	 * 
	 * @param       fd 
	 * @param       backLog 
	 * @return      int 
	 */
	int Listen(int fd, int backLog);

	/**
	 * @brief       
	 * 
	 * @param       family 
	 * @param       type 
	 * @param       protocol 
	 * @return      int 
	 */
	int Socket(int family, int type, int protocol);

	/**
	 * @brief       
	 * 
	 * @param       sockfd 
	 * @param       buf 
	 * @param       len 
	 * @param       flags 
	 * @return      int 
	 */
	int Send(int sockfd, const void *buf, size_t len, int flags);

	/**
	 * @brief       Set the Socket Option object
	 * 
	 * @param       fd 
	 * @param       level 
	 * @param       optName 
	 * @param       optVal 
	 * @param       optLen 
	 * @return      int 
	 */
	int SetSocketOption(int fd, int level, int optName, const void *optVal, socklen_t optLen);

	/**
	 * @brief       
	 * 
	 * @param       port 
	 * @param       listenNum 
	 * @return      int 
	 */
	int initialize_listen_server(int port, int listenNum);

	/**
	 * @brief       Set the Fd No Blocking object
	 * 
	 * @param       sockfd 
	 * @return      int 
	 */
	int setFdNonBlocking(int sockfd);
}
namespace net_data
{
	/**
	 * @brief       
	 * 
	 * @param       por 
	 * @param       ip 
	 * @return      uint64_t 
	 */
	uint64_t dataPackPortAndIp(uint16_t por, uint32_t ip);

	/**
	 * @brief       
	 * 
	 * @param       port 
	 * @param       ip 
	 * @return      uint64_t 
	 */
	uint64_t dataPackPortAndIp(int port, std::string ip);

	/**
	 * @brief       
	 * 
	 * @param       portAndIp 
	 * @return      std::pair<uint16_t, uint32_t> 
	 */
	std::pair<uint16_t, uint32_t> convertDataPackPortAndIpToInt(uint64_t portAndIp);
	
	/**
	 * @brief       
	 * 
	 * @param       portAndIp 
	 * @return      std::pair<int, std::string> 
	 */
	std::pair<int, std::string> convertDataPackPortAndIpToString(uint64_t portAndIp);

}
/**
 * @brief       
 * 
 */
namespace net_com
{
	using namespace net_tcp;
	using namespace net_data;

	enum class Compress : uint8_t
	{
		kCompressDisabled = 0,
		COMPRESS_TRUE = 1
	};

	enum class Encrypt : uint8_t
	{
		ENCRYPT_FALSE = 0,
		ENCRYPT_TRUE = 1,
	};

	enum class Priority : uint8_t
	{
		kPriorityLow0 = 0,
		kPriorityLevelLow1 = 2,
		kPriorityLevelLow2 = 4,

		kPriorityLevelMiddle0 = 5,
		MIDDLE_PRIORITY_LEVEL = 8,
		priorityMiddle2 = 10,

		kPriorityHighLevel0 = 11,
		PRIORITY_HIGH_LEVEL_1 = 14,
		kHighPriorityLevel2 = 15,
	};
	/**
	 * @brief       
	 * 
	 * @param       u32_ip 
	 * @param       u16_port 
	 * @param       connectedPort 
	 * @return      int 
	 */
	int InitConnection(u32 u32_ip, u16 u16_port, u16 &connectedPort);

	/**
	 * @brief       
	 * 
	 * @param       dest 
	 * @param       msg 
	 * @return      true 
	 * @return      false 
	 */
	bool sendEcdhMessageRequest(const Node &dest, KeyExchangeRequest &msg);


	/**
	 * @brief       
	 * 
	 * @param       to 
	 * @param       pack 
	 * @return      true 
	 * @return      false 
	 */
	bool SendOneMessage(const Node &to, const NetPack &pack);

	/**
	 * @brief       
	 * 
	 * @param       to 
	 * @param       msg 
	 * @param       priority 
	 * @return      true 
	 * @return      false 
	 */
	bool SendOneMessage(const Node &to, const std::string &msg, const int8_t priority);

	/**
	 * @brief       
	 * 
	 * @param       to 
	 * @param       pack 
	 * @return      true 
	 * @return      false 
	 */
	bool SendOneMessage(const MsgData &to, const NetPack &pack);

	/**
	 * @brief       
	 * 
	 * @param       addr 
	 * @param       msg 
	 */
	void SendMessageTask(const std::string& addr, BuildBlockBroadcastMsg &msg);
	
	/**
	 * @brief       
	 * 
	 * @param       node 
	 * @param       msg 
	 */
	void SendVRFConsensusInfoTask(const Node& node, VRFConsensusInfo &msg);
	/**
	 * @brief       
	 * 
	 */
	template <typename T>
	bool SendMessage(const std::string id,
					  T &msg,
					  const net_com::Compress isCompress = net_com::Compress::COMPRESS_TRUE,
					  const net_com::Encrypt isEncrypt = net_com::Encrypt::ENCRYPT_FALSE,
					  const net_com::Priority priority = net_com::Priority::kPriorityLow0);

	/**
	 * @brief       
	 * 
	 */
	template <typename T>
	bool SendMessage(const Node &dest,
					  T &msg,
					  const net_com::Compress isCompress = net_com::Compress::COMPRESS_TRUE,
					  const net_com::Priority priority = net_com::Priority::kPriorityLow0);

	/**
	 * @brief       
	 * 
	 */
	template <typename T>
	bool SendMessage(const MsgData &from,
					  T &msg,
					  const net_com::Compress isCompress = net_com::Compress::COMPRESS_TRUE,
					  const net_com::Encrypt isEncrypt = net_com::Encrypt::ENCRYPT_FALSE,
					  const net_com::Priority priority = net_com::Priority::kPriorityLow0);

	/**
	 * @brief       
	 * 
	 */
	template <typename T>
	bool BroadCastMessage(T &msg,
						   const net_com::Compress isCompress = net_com::Compress::COMPRESS_TRUE,
						   const net_com::Encrypt isEncrypt = net_com::Encrypt::ENCRYPT_FALSE,
						   const net_com::Priority priority = net_com::Priority::kPriorityLow0);

	/**
	 * @brief       
	 * 
	 */
	bool broadcastMessage( BuildBlockBroadcastMsg &blockConstructionMessage,
								const net_com::Compress isCompress = net_com::Compress::COMPRESS_TRUE,
								const net_com::Encrypt isEncrypt = net_com::Encrypt::ENCRYPT_FALSE,
								const net_com::Priority priority = net_com::Priority::kPriorityLow0);

	/**
	 * @brief       
	 * 
	 * @param       to 
	 * @return      int 
	 */
	int ANALYSIS_CONNECTION_KIND(Node &to);

	/**
	 * @brief       
	 * 
	 * @return      true 
	 * @return      false 
	 */
	bool InitializeNetwork();

	/**
	 * @brief       
	 * 
	 * @return      int 
	 */
	int sendOneMessageRequest();

	/**
	 * @brief       
	 * 
	 * @return      true 
	 * @return      false 
	 */
	bool sendBigDataForTest();

	/**
	 * @brief       
	 * 
	 * @return      int 
	 */
	int testBroadcastMessage();

	/**
	 * @brief       
	 * 
	 * @param       to 
	 * @param       data 
	 * @param       type 
	 * @return      true 
	 * @return      false 
	 */
	bool sendPrintMessageRequest(Node &to, const std::string data, int type = 0);

	/**
	 * @brief       
	 * 
	 * @param       id 
	 * @param       data 
	 * @param       type 
	 * @return      true 
	 * @return      false 
	 */
	bool sendPrintMessageRequest(const std::string &id, const std::string data, int type = 0);

	/**
	 * @brief       
	 * 
	 * @param       dest 
	 * @param       msgId 
	 * @param       isNodeListFetched 
	 * @return      int 
	 */
	int registerNodeRequest(Node &dest, std::string &msgId, bool isNodeListFetched);

	/**
	 * @brief       
	 * 
	 * @param       dest 
	 */
	void SendPingReq(const Node &dest);

	/**
	 * @brief       
	 * 
	 * @param       dest 
	 */
	void sendPongRequestMessage(const Node &dest);

	/**
	 * @brief       
	 * 
	 */
	void DealHeart();

	/**
	 * @brief       
	 * 
	 * @param       dest 
	 * @param       msgId 
	 * @return      true 
	 * @return      false 
	 */
	bool synchronizeNodeRequest(const Node &dest, std::string &msgId);

	/**
	 * @brief       
	 * 
	 */
	void nodeHeightChanged();
}

namespace net_callback
{
	/**
	 * @brief       
	 * 
	 * @param       callback 
	 */
	using onChainHeight = std::function<int(uint32_t &)>;
	using chainHeightCalculationHandler = std::function<bool(uint64_t &)>;
	void register_chain_height_callback(onChainHeight callback);
	extern onChainHeight chain_height_handler;

	void register_calculate_chain_height_callback(chainHeightCalculationHandler callback);
	extern chainHeightCalculationHandler onCalculateChainHeight;
}



template <typename T>
bool net_com::SendMessage(const Node &dest, T &msg, const net_com::Compress isCompress, const net_com::Priority priority)
{
	CommonMsg commMsg;
	auto key = MagicSingleton<KeyExchangeManager>::GetInstance()->getKey(dest.fd);
	if(key == nullptr)
	{
		ERRORLOG("null key");
		return false;
	}
	Pack::initializeCommonMessageRequest(commMsg, msg, *key.get(), (uint8_t)net_com::Encrypt::ENCRYPT_TRUE, (uint8_t)isCompress);
	NetPack pack;
	Pack::packedCommonMessage(commMsg, (uint8_t)priority, pack);

	return net_com::SendOneMessage(dest, pack);
}


template <typename T>
bool net_com::SendMessage(const std::string id, T &msg, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{
	Node node;
	auto find = MagicSingleton<PeerNode>::GetInstance()->FindNode(id, node);
	if (find)
	{
		return net_com::SendMessage(node, msg, isCompress, priority);
	}
	else if(id != MagicSingleton<PeerNode>::GetInstance()->GetSelfId())
	{
		Node transNode;
		transNode.address = id;
		return net_com::SendMessage(transNode, msg, isCompress, priority);
	}
}

template <typename T>
bool net_com::SendMessage(const MsgData &from, T &msg, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{
	Node node;
	auto find = MagicSingleton<PeerNode>::GetInstance()->locateNodeByFd(from.fd, node);
	if (find)
	{
		return net_com::SendMessage(node, msg, isCompress, priority);
	}
	else
	{
		CommonMsg comm_msg;
		Pack::initializeCommonMessageRequest(comm_msg, msg, (uint8_t)isEncrypt, (uint8_t)isCompress);

		NetPack pack;
		Pack::packedCommonMessage(comm_msg, (uint8_t)priority, pack);
		return net_com::SendOneMessage(from, pack);
	}
}

/**
 * @brief       
 * 
 */
template <typename T>
bool net_com::BroadCastMessage(T &msg, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{
	const Node &selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();

	const std::vector<Node> &&public_node_list = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	if (global::GetBuildType() == GenesisConfig::BuildType::BUILD_TYPE_DEV)
	{
		INFOLOG("Total number of public nodelists: {}",  public_node_list.size());
	}
	if (public_node_list.empty())
	{
		ERRORLOG("public_node_list is empty!");
		return false;
	}

	INFOLOG("Verification passed, start broadcasting!");

	// Send to public nodelist
	for (auto &item : public_node_list)
	{
		if (selfNode.address != item.address)
		{
			net_com::SendMessage(item, msg);
		}
	}
	return true;
}

#endif
