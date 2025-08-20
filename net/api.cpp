
#include "api.h"

#include <arpa/inet.h>
#include <signal.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <errno.h>

#include <string>
#include <sstream>
#include <utility>

#include "./global.h"
#include "./dispatcher.h"
#include "./socket_buf.h"
#include "./work_thread.h"
#include "./epoll_mode.h"
#include "./http_server.h"
#include "./ip_port.h"
#include "./peer_node.h"
#include "./global.h"

#include "../proto/net.pb.h"
#include "../proto/common.pb.h"
#include "../proto/block.pb.h"
#include "db/db_api.h"

#include "../common/global.h"
#include "../include/logging.h"
#include "../utils/time_util.h"
#include "../utils/console.h"

#include "../utils/account_manager.h"
#include "../utils/cycliclist.hpp"
#include "../utils/tmp_log.h"
#include "common/global_data.h"
#include "key_exchange.h"

#include "../ca/algorithm.h"

int net_tcp::Socket(int family, int type, int protocol)
{
	int n;

	if ((n = socket(family, type, protocol)) < 0)
		ERRORLOG("can't create socket file");
	return n;
}

int net_tcp::Accept(int fd, struct sockaddr *sa, socklen_t *salenptr)
{
	int n;

	if ((n = accept(fd, sa, salenptr)) < 0)
	{
		if ((errno == ECONNABORTED) || (errno == EINTR) || (errno == EWOULDBLOCK))
		{
			goto ret;
		}
		else
		{
			ERRORLOG("accept error");
		}
	}
ret:
	return n;
}

int net_tcp::Bind(int fd, const struct sockaddr *sa, socklen_t salen)
{
	int n;

	if ((n = bind(fd, sa, salen)) < 0)
		ERRORLOG("bind error");
	return n;
}

int net_tcp::Connect(int fd, const struct sockaddr *sa, socklen_t salen)
{
	int n;

	int bufLen;
	int optLen = sizeof(bufLen);
	getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&bufLen, (socklen_t *)&optLen);

	int recvBuf = 1 * 1024 * 1024;
	SetSocketOption(fd, SOL_SOCKET, SO_RCVBUF, (const void *)&recvBuf, sizeof(int));

	int sndBuf = 1 * 1024 * 1024;
	SetSocketOption(fd, SOL_SOCKET, SO_SNDBUF, (const void *)&sndBuf, sizeof(int));

	getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&bufLen, (socklen_t *)&optLen);

	if ((n = connect(fd, sa, salen)) < 0)
	{
	}

	return n;
}

int net_tcp::Listen(int fd, int backLog)
{
	int n;

	if ((n = listen(fd, backLog)) < 0)
		ERRORLOG("listen error");
	return n;
}

int net_tcp::Send(int sockfd, const void *buf, size_t len, int flags)
{
	if (sockfd < 0)
	{
		ERRORLOG("Send func: file description err"); // Error sending file descriptor
		return -1;
	}
	int bytesLeft;
	int writtenBytes;
	char *ptr;
	ptr = (char *)buf;
	bytesLeft = len;
	while (bytesLeft > 0)
	{
		writtenBytes = write(sockfd, ptr, bytesLeft);
		if (writtenBytes <= 0) /* Something went wrong */
		{
			if (writtenBytes == 0)
			{
				continue;
			}
			if (errno == EINTR)
			{
				continue;
			}
			else if (errno == EAGAIN) /* EAGAIN : Resource temporarily unavailable*/
			{

				return len - bytesLeft;
			}
			else 
			{
				MagicSingleton<PeerNode>::GetInstance()->delete_by_fd(sockfd);
				return -2;
			}
		}

		bytesLeft -= writtenBytes;
		ptr += writtenBytes; /* Continue writing from the rest of the place */
	}
	return len;
}

int net_tcp::SetSocketOption(int fd, int level, int optName, const void *optVal, socklen_t optLen)
{
	int ret;

	if ((ret = setsockopt(fd, level, optName, optVal, optLen)) == -1)
		ERRORLOG("setsockopt error");
	return ret;
}

int net_tcp::initialize_listen_server(int port, int listenNum)
{
	struct sockaddr_in servAddr;
	int listener;
	int opt = 1;
	listener = Socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

	bzero(&servAddr, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY); // any addr
	servAddr.sin_port = htons(port);

	SetSocketOption(listener, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt,
			   sizeof(opt));
	SetSocketOption(listener, SOL_SOCKET, SO_REUSEPORT, (const void *)&opt,
			   sizeof(opt));

	Bind(listener, (struct sockaddr *)&servAddr, sizeof(servAddr));

	int recvBuf = 1 * 1024 * 1024;
	SetSocketOption(listener, SOL_SOCKET, SO_RCVBUF, (const void *)&recvBuf, sizeof(int));
	int sndBuf = 1 * 1024 * 1024;
	SetSocketOption(listener, SOL_SOCKET, SO_SNDBUF, (const void *)&sndBuf, sizeof(int));
	Listen(listener, listenNum);

	return listener;
}
int net_tcp::setFdNonBlocking(int sockfd)

{
	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK) == -1)
	{
		ERRORLOG("setnonblock error");
		return -1;
	}
	return 0;
}

int net_com::InitConnection(u32 u32_ip, u16 u16_port, u16 &connectedPort)
{
	int confd = 0;
	struct sockaddr_in servAddr = {0};
	struct sockaddr_in my_addr = {0};
	int ret = 0;

	confd = Socket(AF_INET, SOCK_STREAM, 0);
	int flags = 1;
	SetSocketOption(confd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(int));
	flags = 1;
	SetSocketOption(confd, SOL_SOCKET, SO_REUSEPORT, &flags, sizeof(int));

	// Connect to each other
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(u16_port);
	struct in_addr addr = {0};
	memcpy(&addr, &u32_ip, sizeof(u32_ip));
	inet_pton(AF_INET, inet_ntoa(addr), &servAddr.sin_addr);

	/*The default timeout timeout for Linux systems is 75s during blocking conditions*/
	if (setFdNonBlocking(confd) < 0)
	{
		DEBUGLOG("setnonblock error");
		return -1;
	}

	ret = Connect(confd, (struct sockaddr *)&servAddr, sizeof(servAddr));

	struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
	getsockname(confd, (struct sockaddr*)&clientAddr, &clientAddrLen); // Gets the local address on the connection represented by sockfd
	connectedPort = ntohs(clientAddr.sin_port);

	if (ret != 0)
	{
		if (errno == EINPROGRESS)
		{
			struct epoll_event peerConnectionEvent;
			int epollFD = -1;
			struct epoll_event pendingEvents;
			unsigned int numEvents = -1;

			if ((epollFD = epoll_create(1)) == -1)
			{
				ERRORLOG("Could not create the epoll FD list!");
				close(confd);
				return -2;
			}     

			peerConnectionEvent.data.fd = confd;
			peerConnectionEvent.events = EPOLLOUT | EPOLLIN | EPOLLERR;

			if (epoll_ctl(epollFD, EPOLL_CTL_ADD, confd, &peerConnectionEvent) == -1)
			{
				ERRORLOG("Could not add the socket FD to the epoll FD list!");
				close(confd);
				close(epollFD);
				return -3;
			}

			numEvents = epoll_wait(epollFD, &pendingEvents, 1, 3*1000);

			if (numEvents < 0)
			{
				ERRORLOG("Serious error in epoll setup: epoll_wait () returned < 0 status!");
				close(epollFD);
				close(confd);
				return -4;
			}
			int retVal = -1;
			socklen_t retValLen = sizeof (retVal);
			if (getsockopt(confd, SOL_SOCKET, SO_ERROR, &retVal, &retValLen) < 0)
			{
				ERRORLOG("getsockopt SO_ERROR error!");
				close(confd);
				close(epollFD);
				return -5;
			}

			if (retVal == 0)  // succeed
			{
				close(epollFD);
				return confd;
			} 
			else
			{
				close(epollFD);
				close(confd);
				return -6;
			}	
		}
		else
		{
			close(confd);
			return -7;			
		}
	}

	return confd;
}

void net_com::SendMessageTask(const std::string& addr, BuildBlockBroadcastMsg &msg) {
  	net_com::SendMessage(addr, msg);
}

void net_com::SendVRFConsensusInfoTask(const Node& node, VRFConsensusInfo &msg)
{
	net_com::SendMessage(node, msg, net_com::Compress::COMPRESS_TRUE, net_com::Priority::kHighPriorityLevel2);
}

bool net_com::SendOneMessage(const Node &to, const NetPack &pack)
{
	auto msg = Pack::packageToString(pack);
	uint8_t priority = pack.flag & 0xF;

	return SendOneMessage(to, msg, priority);
}

bool net_com::SendOneMessage(const Node &to, const std::string &msg, const int8_t priority)
{
	MsgData sendData;
	sendData.type = E_WRITE;
	sendData.fd = to.fd;
	sendData.ip = to.publicIp;
	sendData.port = to.publicPort;
	
	MagicSingleton<bufferControl>::GetInstance()->addWritePack_(sendData.ip, sendData.port, msg);
	bool bRet = global::queue_write_counter.Push(sendData);
	return true;

}

bool net_com::SendOneMessage(const MsgData& to, const NetPack &pack)
{
	MsgData sendData;
	sendData.type = E_WRITE;
	sendData.fd = to.fd;
	sendData.ip = to.ip;
	sendData.port = to.port;

	auto msg = Pack::packageToString(pack);	
	MagicSingleton<bufferControl>::GetInstance()->addWritePack_(sendData.ip, sendData.port, msg);
	bool bRet = global::queue_write_counter.Push(sendData);
	return bRet;
}

bool net_com::sendEcdhMessageRequest(const Node &dest, KeyExchangeRequest &msg)
{
	CommonMsg comm_msg;
	Pack::initializeCommonMessageRequest(comm_msg, msg, 0, 0);
	NetPack pack;
	Pack::packedCommonMessage(comm_msg, (uint8_t)Priority::kHighPriorityLevel2, pack);

	return net_com::SendOneMessage(dest, pack);
}


uint64_t net_data::dataPackPortAndIp(uint16_t port, uint32_t ip)
{
	uint64_t ret = port;
	ret = ret << 32 | ip;
	return ret;
}

uint64_t net_data::dataPackPortAndIp(int port, std::string ip)
{
	uint64_t ret = port;
	uint32_t tmp;
	inet_pton(AF_INET, ip.c_str(), &tmp);
	ret = ret << 32 | tmp;
	return ret;
}
std::pair<uint16_t, uint32_t> net_data::convertDataPackPortAndIpToInt(uint64_t portAndIp)
{
	uint64_t tmp = portAndIp;
	uint32_t ip = tmp << 32 >> 32;
	uint16_t port = portAndIp >> 32;
	return std::pair<uint16_t, uint32_t>(port, ip);
}
std::pair<int, std::string> net_data::convertDataPackPortAndIpToString(uint64_t portAndIp)
{
	uint64_t tmp = portAndIp;
	uint32_t ip = tmp << 32 >> 32;
	uint16_t port = portAndIp >> 32;
	char buf[100];
	inet_ntop(AF_INET, (void *)&ip, buf, 16);
	return std::pair<uint16_t, std::string>(port, buf);
}

int net_com::ANALYSIS_CONNECTION_KIND(Node &to)
{
	to.connKind = dataRateToOutput; //Outer and external direct connection
	return to.connKind;
}

bool net_com::InitializeNetwork()
{
	// Capture SIGPIPE signal to prevent accidental exit of the program
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGPIPE, &sa, NULL);

	// Block the SIGPIPE signal
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	sigprocmask(SIG_BLOCK, &set, NULL);

	// Ignore the SIGPIPE signal
	signal(SIGPIPE, SIG_IGN);

	if (MagicSingleton<Config>::GetInstance()->GetIP().empty())
	{
		std::string localhost_ip;
		if (!IpPort::GetLocalHostIp(localhost_ip))
		{
			DEBUGLOG("Failed to obtain the local Intranet IP address.");
			return false;
		}
		MagicSingleton<Config>::GetInstance()->SetIP(localhost_ip);
	}

	global::g_localIp = MagicSingleton<Config>::GetInstance()->GetIP();	
	
	// Get the native intranet IP address
	if(global::g_localIp.empty())
	{
		DEBUGLOG("IP address is empty.");
		return false;
	}

	if(IpPort::IsLan(global::g_localIp))
	{
		std::cout << "The current IP address is " << global::g_localIp << ". Please use an external IP address." << std::endl;
		return false;
	}

	INFOLOG("The Intranet ip is not empty");
	
	Account acc;
	if (MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(acc) != 0)
	{
		return false;
	}

	MagicSingleton<PeerNode>::GetInstance()->SetSelfId(acc.GetAddr());
	MagicSingleton<PeerNode>::GetInstance()->SetSelfIdentity(acc.GetPubStr());
	MagicSingleton<PeerNode>::GetInstance()->SetSelfHeight();
	
	MagicSingleton<PeerNode>::GetInstance()->set_self_ip_listen(IpPort::IpNum(global::g_localIp.c_str()));
	MagicSingleton<PeerNode>::GetInstance()->configureSelfPortForListening(kServerMainPort);
	MagicSingleton<PeerNode>::GetInstance()->set_self_ip_public(IpPort::IpNum(global::g_localIp.c_str()));
	MagicSingleton<PeerNode>::GetInstance()->setPublicSelfPort(kServerMainPort);

	Config::Info info = {};
	MagicSingleton<Config>::GetInstance()->GetInfo(info);

	MagicSingleton<PeerNode>::GetInstance()->SetSelfName(info.name);
	MagicSingleton<PeerNode>::GetInstance()->set_user_logo(info.logo );

	MagicSingleton<PeerNode>::GetInstance()->SetSelfVer(global::GetVersion());

	// Work thread pool start
	MagicSingleton<WorkThreads>::GetInstance()->Start();

	// Create a listening thread
	MagicSingleton<EpollMode>::GetInstance()->EPOL_MODE_START();
	
	// Start "refresh nodelist" thread 
    MagicSingleton<PeerNode>::GetInstance()->initNodeListRefreshThread();
	
	//Start Network node switching
	MagicSingleton<PeerNode>::GetInstance()->nodelistSwitchThread();

	// Start the heartbeat
	global::g_heartTimer.AsyncLoop(HEARTBEAT_INTERVAL * 1000, net_com::DealHeart);

	return true;
}

//Test single message
int net_com::sendOneMessageRequest()
{
	DEBUGLOG(RED "sendOneMessageRequest start" RESET);
	std::string id;
	std::cout << "please input id:";
	std::cin >> id;

	while (true)
	{
		//Verify that the ID is legitimate
		bool result = isValidAddress(id);
		if (false == result)
		{
			std::cout << "invalid id , please input id:";
			std::cin >> id;
			continue;
		}
		else
		{
			break;
		}
	};

	std::string msg;
	std::cout << "please input msg:";
	std::cin >> msg;

	int num;
	std::cout << "please input num:";
	std::cin >> num;

	bool bl;
	for (int i = 0; i < num; ++i)
	{
		bl = net_com::sendPrintMessageRequest(id, msg);

		if (bl)
		{
			printf("The %d send success\n", i + 1);
		}
		else
		{
			printf("The0 %d send fail\n", i + 1);
		}

	}
	return bl ? 0 : -1;
}

//Test the broadcast information
int net_com::testBroadcastMessage()
{
	std::string str_buf = "Hello World!";

	PrintMsgReq requestToPrintMessage;
	requestToPrintMessage.set_data(str_buf);

	bool isSucceed = net_com::BroadCastMessage(requestToPrintMessage);
    if(isSucceed == false)
    {
        ERRORLOG(":broadcast PrintMsgReq failed!");
        return -1;
    }
	return 0;
}

bool net_com::sendBigDataForTest()
{
	std::string id;
	std::cout << "please input id:";
	std::cin >> id;
	auto IsVaild = [](std::string idStr) {
		int count = 0;
		for (auto i : idStr)
		{
			if (i != '1' || i != '0')
				return false;
			count++;
		}
		return count == 16;
	};
	while (IsVaild(id))
	{
		std::cout << "IsVaild id , please input id:";
		std::cin >> id;
	};
	Node tmpNode;
	if (!MagicSingleton<PeerNode>::GetInstance()->FindNode(std::string(id), tmpNode))
	{
		DEBUGLOG("invaild id, not in my peer node");
		return false;
	}
	std::string tmpData;
	int txtNum;
	std::cout << "please input test byte num:";
	std::cin >> txtNum;
	for (int i = 0; i < txtNum; i++)
	{
		char x, s;									  
		s = (char)rand() % 2;						  
		if (s == 1)									  
			x = (char)rand() % ('Z' - 'A' + 1) + 'A'; 
		else
			x = (char)rand() % ('z' - 'a' + 1) + 'a'; 
		tmpData.push_back(x);						 
	}
	tmpData.push_back('z');
	tmpData.push_back('z');
	tmpData.push_back('z');
	tmpData.push_back('z');
	tmpData.push_back('z');

	net_com::sendPrintMessageRequest(tmpNode, tmpData, 1);
	return true;
}

bool net_com::sendPrintMessageRequest(Node &to, const std::string data, int type)
{
	PrintMsgReq requestToPrintMessage;
	requestToPrintMessage.set_data(data);
	requestToPrintMessage.set_type(type);
	net_com::SendMessage(to, requestToPrintMessage);
	return true;
}

bool net_com::sendPrintMessageRequest(const std::string & id, const std::string data, int type)
{
	PrintMsgReq requestToPrintMessage;
	requestToPrintMessage.set_data(data);
	requestToPrintMessage.set_type(type);
	net_com::SendMessage(id, requestToPrintMessage);
	return true;
}

int net_com::registerNodeRequest(Node& dest, std::string &msgId, bool isNodeListFetched)
{
	INFOLOG("registerNodeRequest");

	RegisterNodeReq getNodes;
	getNodes.set_is_get_nodelist(isNodeListFetched);
	getNodes.set_msg_id(msgId);
	NodeInfo* mynode = getNodes.mutable_mynode();
	const Node & selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();

	std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	auto findResult = std::find_if(nodelist.begin(), nodelist.end(), [dest](const Node &findNode)
								{ return dest.address == findNode.address; });	
	if(findResult != nodelist.end()){
		DEBUGLOG("ConnectNode address:{}, ip:{}, port:{}",dest.address, IpPort::IpSz(dest.publicIp), dest.publicPort);
		return 0;
	}

	if(!isValidAddress(selfNode.address))
	{
		ERRORLOG(" registerNodeRequest selfNode.address {} error",selfNode.address);
		return -1;
	}

    mynode->set_addr(selfNode.address);
	mynode->set_name(selfNode.name);
	mynode->set_listen_ip( selfNode.listenIp);
	mynode->set_logo(selfNode.logo);
	mynode->set_listen_port( selfNode.listenPort);

	mynode->set_time_stamp(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
	mynode->set_height(MagicSingleton<PeerNode>::GetInstance()->get_self_chain_height_newest());
	mynode->set_version(global::GetVersion());
	// sign
	std::string signature;
	Account acc;
	if(MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(acc) != 0)
	{
		ERRORLOG("The default account does not exist");
		return -2;
	}
	if (selfNode.address != acc.GetAddr())
	{
		ERRORLOG("The account address is incorrect : {} , : {}", selfNode.address, acc.GetAddr());
		return -3;
	}
	if(!acc.Sign(Getsha256hash(acc.GetAddr()), signature))
	{
		ERRORLOG("sign fail , address : {}", acc.GetAddr());
		return -4;
	}

	mynode->set_identity(acc.GetPubStr());
	mynode->set_sign(signature);

	auto ret = MagicSingleton<KeyExchangeManager>::GetInstance()->keyExchangeRequest(dest);
	if(ret < 0)
	{
		ERRORLOG("KeyExchange fail !!! ip:{}, fd:{}, ret:{}",IpPort::IpSz(dest.publicIp), dest.fd, ret);
		return -5;
	}
	std::string ipPort = std::to_string(dest.publicIp) + ":" + std::to_string(dest.publicPort);
	DEBUGLOG("AddResNode, msg_id:{} peerId:{}", msgId, ipPort);
	if(!dataMgrPtr.AddResNode(msgId, ipPort))
	{
		return -6;
	}
	net_com::SendMessage(dest, getNodes, net_com::Compress::COMPRESS_TRUE, net_com::Priority::kHighPriorityLevel2);
	
	return 0;
}

void net_com::SendPingReq(const Node& dest)
{
	PingReq pingReq;
	std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
	pingReq.set_id(defaultAddr);
	DEBUGLOG("dest addr:{}", dest.address);
	net_com::SendMessage(dest, pingReq, net_com::Compress::COMPRESS_TRUE, net_com::Priority::kHighPriorityLevel2);
}

void net_com::sendPongRequestMessage(const Node& dest)
{
	PongReq pongReq;
	std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
	pongReq.set_id(defaultAddr);
	DEBUGLOG("dest addr:{}", dest.address);
	net_com::SendMessage(dest, pongReq, net_com::Compress::COMPRESS_TRUE, net_com::Priority::kHighPriorityLevel2);
}

void net_com::DealHeart()
{
	Node mynode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();	
	std::vector<Node> pubNodeList_ = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();

	//Exclude yourself
	std::vector<Node>::iterator end = pubNodeList_.end();
	for(std::vector<Node>::iterator it = pubNodeList_.begin(); it != end; ++it)
	{
		if(mynode.address == it->address)
		{
			it = pubNodeList_.erase(it);
		}
	}
	std::vector<Node> nodelist;
	nodelist.insert(nodelist.end(),pubNodeList_.begin(),pubNodeList_.end());
	for(auto &node:nodelist)
	{
		node.pulse -= 1;
		if(node.pulse <= 0)
		{
			// net_com::SendPingReq(node);
			DEBUGLOG("DealHeart delete node: {}, ip:{} , port:{}, fd:{}", node.address, IpPort::IpSz(node.publicIp), node.publicPort, node.fd);

			MagicSingleton<PeerNode>::GetInstance()->DeleteNode(node.address);
		}
		else
		{
			MagicSingleton<PeerNode>::GetInstance()->Update(node);
			net_com::SendPingReq(node);
		}
	}	
}

bool net_com::synchronizeNodeRequest(const Node& dest, std::string &msgId)
{
	DEBUGLOG("synchronizeNodeRequest from.ip:{}", IpPort::IpSz(dest.publicIp));
	SyncNodeReq nodeSynchronizationRequestMessage;
	//Get its own node information
	auto self_node = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
	std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	
	if(nodelist.size() == 0)
	{
		return false;
	}
	//Stores its own node ID
	nodeSynchronizationRequestMessage.set_ids(std::move(self_node.address));
	nodeSynchronizationRequestMessage.set_msg_id(msgId);
	if(!dataMgrPtr.AddResNode(msgId, dest.address))
	{
		return false;
	}
	return net_com::SendMessage(dest, nodeSynchronizationRequestMessage, net_com::Compress::COMPRESS_TRUE, net_com::Priority::kHighPriorityLevel2);
}

void net_com::nodeHeightChanged()
{
	NodeHeightChangedReq requestForHeightChange;
	std::string selfId = MagicSingleton<PeerNode>::GetInstance()->GetSelfId();

	requestForHeightChange.set_id(selfId);
	uint32 chainHeight = 0;
	int ret = net_callback::chain_height_handler(chainHeight);
	requestForHeightChange.set_height(chainHeight);

	Account defaultEd;
	MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(defaultEd);

	std::stringstream Height;

	Height << selfId << "_" << std::to_string(chainHeight);
	std::string vinHashSerialized = Getsha256hash(Height.str());
	std::string signature;
	std::string pub;

	if (defaultEd.Sign(vinHashSerialized, signature) == false)
	{
		std::cout << "tx sign fail !" << std::endl;
	}
	CSign * sign = requestForHeightChange.mutable_sign();
	sign->set_sign(signature);
	sign->set_pub(defaultEd.GetPubStr());


	auto selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
	std::vector<Node> publicNodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	for (auto& node : publicNodes)
	{
		net_com::SendMessage(node, requestForHeightChange, net_com::Compress::kCompressDisabled, net_com::Priority::kHighPriorityLevel2);
	}
}

namespace net_callback
{
	onChainHeight chain_height_handler =  nullptr;
	chainHeightCalculationHandler onCalculateChainHeight = nullptr;
}

void net_callback::register_chain_height_callback(onChainHeight callback)
{
	net_callback::chain_height_handler = callback;
}

void net_callback::register_calculate_chain_height_callback(chainHeightCalculationHandler callback)
{
	net_callback::onCalculateChainHeight = callback;
}

bool net_com::broadcastMessage( BuildBlockBroadcastMsg& blockConstructionMessage, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{	
	const std::vector<Node>&& public_node_list = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	if(public_node_list.empty())
	{
		ERRORLOG("public_node_list is empty!");
		return false;
	}

	const double threshold = 0.25;
	const std::size_t unConnectedCount = std::count_if(public_node_list.cbegin(), public_node_list.cend(), [](const Node &node){ return node.fd == -1;});
	double percent = static_cast<double>(unConnectedCount) / public_node_list.size();
	if(percent > threshold)
	{
		ERRORLOG("Unconnected nodes are {},accounting for {}%", unConnectedCount, percent * 100);
		return false;
	}

	INFOLOG("Verification passed, start broadcasting!");

	CBlock block;
    block.ParseFromString(blockConstructionMessage.blockraw());

	
	auto getNextNumber=[&](int limit) ->int {
	  	std::random_device seed;
	 	std::ranlux48 engine(seed());
	 	std::uniform_int_distribution<int> u(0, limit-1);
	 	return u(engine);
	};

	auto fetchTargetIndexes=[&](int num,int limit,const std::vector<Node> & source)->std::set<std::string>
	{
		std::set<std::string> allAddresses;
		if(limit < num){
			ERRORLOG(" The source is less the num !! [limit:{}],[num:{}]",limit,num);
			return allAddresses;
		}
		else if(limit == num)
		{
			for(const auto & node : source)
			{
				allAddresses.insert(node.address);
			}
			return allAddresses;
		}

		while(allAddresses.size()< num){
			int index=getNextNumber(limit);
			allAddresses.insert(source[index].address);
		}
		return allAddresses;
	};

	auto getRootOfEquation=[](int listSize)->int
	{
		int x1=(-1+std::sqrt(1-4*(-listSize)))/2;
		int x2=(-1-std::sqrt(1-4*(-listSize)))/2;
		return (x1 >0) ? x1:x2;
	};

	std::vector<Node> nodeList = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	std::set<std::string> addrs;
	
	if(block.height() < global::ca::MIN_UNSTAKE_HEIGHT)
	{
		if(nodeList.size() <= global::broadcast_threshold)
		{
			blockConstructionMessage.set_type(1);//Set the number of broadcasts to two
			for(auto &node : nodeList)
			{
				blockConstructionMessage.add_castaddrs(node.address);
			}
			for(auto & node : nodeList){
				MagicSingleton<TaskPool>::GetInstance()->commitBroadcastRequest(std::bind(&net_com::SendMessageTask, node.address, blockConstructionMessage));
			}

		}else{
			std::set<std::string> addrs = fetchTargetIndexes(global::broadcast_threshold,nodeList.size(),nodeList);
			blockConstructionMessage.set_type(1);//Set the number of broadcasts to 1
			
			for(auto &addr:addrs)
			{
				blockConstructionMessage.add_castaddrs(addr);	
			}
			for(auto & addr : addrs) {		
				MagicSingleton<TaskPool>::GetInstance()->commitBroadcastRequest(std::bind(&net_com::SendMessageTask, addr, blockConstructionMessage));
			}
		}
	}
	else
	{
		DBReader dbReader;

		std::string assetType;
		int ret = ca_algorithm::GetCanBeRevokeAssetType(assetType);
		if(ret != 0){
			ERRORLOG("Get Can BeRevoke AssetType fail!");
		}

		std::vector<Node> eligibleAddress;
		std::vector<std::string> pledgeUtxoHashes;
		for(auto & node : nodeList)
		{

			//Verification of  pledge
			int ret = dbReader.getStakeAddrUtxo(node.address, assetType, pledgeUtxoHashes);
			if(DBStatus::DB_SUCCESS == ret || !pledgeUtxoHashes.empty()){
				eligibleAddress.push_back(node);
			}
		}

		if(nodeList.size() >= global::broadcast_threshold * global::broadcast_threshold)
		{
			int m = getRootOfEquation(nodeList.size());
			int threshold = eligibleAddress.size() > m ? m : eligibleAddress.size();
			addrs = fetchTargetIndexes(threshold, eligibleAddress.size(), eligibleAddress);
			
			blockConstructionMessage.set_type(1);//Set the number of broadcasts to 1
			for(auto &addr:addrs)
			{
				blockConstructionMessage.add_castaddrs(addr);	
			}
			
			for(auto & addr : addrs){	
				MagicSingleton<TaskPool>::GetInstance()->commitBroadcastRequest(std::bind(&net_com::SendMessageTask, addr, blockConstructionMessage));
			}
		}
		else
		{
			int threshold = eligibleAddress.size() > global::broadcast_threshold ? global::broadcast_threshold : eligibleAddress.size();
			std::set<std::string> addrs = fetchTargetIndexes(threshold, eligibleAddress.size(), eligibleAddress);

			blockConstructionMessage.set_type(1);//Set the number of broadcasts to 1
			
			for(auto & addr : addrs)
			{
				blockConstructionMessage.add_castaddrs(addr);	
			}

			for(auto & addr : addrs){	
				MagicSingleton<TaskPool>::GetInstance()->commitBroadcastRequest(std::bind(&net_com::SendMessageTask, addr, blockConstructionMessage));
			}
		}
	}
	
	return true;
}