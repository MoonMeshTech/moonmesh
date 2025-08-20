#include "./peer_node.h"

#include <sys/time.h>
#include <chrono>
#include <bitset>

#include "./ip_port.h"
#include "./api.h"
#include "./epoll_mode.h"
#include "./global.h"
#include "./unregister_node.h"

#include "../include/logging.h"
#include "../utils/contract_utils.h"
#include "../utils/console.h"
#include "../net/unregister_node.h"
#include "../net/global.h"
#include "../common/config.h"

bool PeerNode::Add(const Node& node)
{
	uint64_t chainHeight = 0;
	if(!net_callback::onCalculateChainHeight(chainHeight))
	{
		return false;
	}

	std::unique_lock<std::shared_mutex> lck(mutexForNodes);
	if(node.address.size() == 0)
	{
		return false;
	} 
	if(MagicSingleton<Config>::GetInstance()->_Verify(node) != 0)
	{
		return false;
	}
	auto itr = _nodeMap.find(node.address);
	if (itr != _nodeMap.end())
	{
		return false;
	}
	DEBUGLOG("PeerNode::Add ip:{}, publicport:{}, fd:{}",IpPort::IpSz(node.publicIp), node.publicPort, node.fd);
	this->_nodeMap[node.address] = node;

	return true;
}

bool PeerNode::Update(const Node & node)
{
	uint64_t chainHeight = 0;
	if(!net_callback::onCalculateChainHeight(chainHeight))
	{
		return false;
	}

	{
		std::unique_lock<std::shared_mutex> lck(mutexForNodes);
		auto itr = this->_nodeMap.find(node.address);
		if (itr == this->_nodeMap.end())
		{
			return false;
		}
		
		DEBUGLOG("PeerNode::Update ip:{}, publicport:{}, fd:{}",IpPort::IpSz(node.publicIp), node.publicPort, node.fd);
		this->_nodeMap[node.address] = node;
	}

	return true;
}

bool PeerNode::AddOrUpdate(Node node)
{
	std::unique_lock<std::shared_mutex> lck(mutexForNodes);
	this->_nodeMap[node.address] = node;
	
	return true;
}

void PeerNode::DeleteNode(std::string Addr)
{
	DEBUGLOG("DeleteNode addr:{}", Addr);
	std::unique_lock<std::shared_mutex> lck(mutexForNodes);
	auto nodeIt = _nodeMap.find(Addr);
	
	if (nodeIt != _nodeMap.end())
	{
		int fd = nodeIt->second.fd;
		if(fd > 0)
		{
			MagicSingleton<EpollMode>::GetInstance()->DeleteEpollEvent(fd);
			close(fd);
		}	
		u32 ip = nodeIt->second.publicIp;
		u16 port = nodeIt->second.publicPort;
		if(!MagicSingleton<bufferControl>::GetInstance()->DeleteBuffer(ip, port))
		{
			ERRORLOG(RED "DeleteBuffer ERROR ip:({}), port:({}) " RESET, IpPort::IpSz(ip), port);
		}

		MagicSingleton<UnregisterNode>::GetInstance()->deleteSplitNodeList(nodeIt->first);			
		nodeIt = _nodeMap.erase(nodeIt);
	}
	else
	{
		DEBUGLOG("Not found  {} in _nodeMap", Addr);
	}
}

void PeerNode::CloseFd(int fd)
{
	DEBUGLOG("DeleteNode ip:{}", IpPort::IpSz(IpPort::get_peer_nip_info(fd)));
	if(fd <= 0)
	{
		return;
	}

	MagicSingleton<bufferControl>::GetInstance()->DeleteBuffer(fd); 
	MagicSingleton<EpollMode>::GetInstance()->DeleteEpollEvent(fd);
	int ret = close(fd);
	if(ret != 0)
	{
		DEBUGLOG("CloseFd close error, fd = {}, errno = {}, ret = ",fd , errno, ret);
	}
}

void PeerNode::delete_by_fd(int fd)
{
	DEBUGLOG("DeleteNode ip:{}, fd:{}", IpPort::IpSz(IpPort::get_peer_nip_info(fd)), fd);
	std::unique_lock<std::shared_mutex> lck(mutexForNodes);
	auto nodeIt = _nodeMap.begin();
	for(; nodeIt != _nodeMap.end(); ++nodeIt)
	{
        if(nodeIt->second.fd == fd)
		{
			break;
		}	
    }

	u32 ip = nodeIt->second.publicIp;
	u16 port = nodeIt->second.publicPort;

	if (nodeIt != _nodeMap.end())
	{
		if(!MagicSingleton<bufferControl>::GetInstance()->DeleteBuffer(ip, port))
		{
			ERRORLOG(RED "DeleteBuffer ERROR ip:({}), port:({}), fd:{}" RESET, IpPort::IpSz(ip), port, fd);
		}

		MagicSingleton<UnregisterNode>::GetInstance()->deleteSplitNodeList(nodeIt->first);		
		nodeIt = _nodeMap.erase(nodeIt);
	}
	else
	{
		if(!MagicSingleton<bufferControl>::GetInstance()->DeleteBuffer(fd))
		{
			ERRORLOG(RED "DeleteBuffer ERROR ip:({}), port:({})" RESET, IpPort::IpSz(ip), port);
		}
	}

	if(fd > 0)
	{
		MagicSingleton<EpollMode>::GetInstance()->DeleteEpollEvent(fd);
		close(fd);
	}	
}


void PeerNode::Print(std::vector<Node> & nodeList)
{
	std::cout << std::endl;
	std::cout << "------------------------------------------------------------------------------------------------------------" << std::endl;
	for (auto& i : nodeList)
	{
		i.Print();
	}
	std::cout << "------------------------------------------------------------------------------------------------------------" << std::endl;
	std::cout << "PeerNode size is: " << nodeList.size() << std::endl;
}

void PeerNode::Print(const Node & node)
{
	std::cout << "---------------------------------- node info --------------------------------------------------------------------------" << std::endl;
	
	std::cout << "publicIp: " << IpPort::IpSz(node.publicIp) << std::endl;
	std::cout << "localIp: " << IpPort::IpSz(node.listenIp) << std::endl;
	std::cout << "publicPort: " << node.publicPort << std::endl;
	std::cout << "localPort: " << node.listenPort << std::endl;
	std::cout << "connKind: " << node.connKind << std::endl;
	std::cout << "fd: " << node.fd << std::endl;
	std::cout << "pulse: " << node.pulse << std::endl;
	std::cout << "address: " << node.address << std::endl;
	std::cout << "chainHeight: " << node.height << std::endl;

	std::cout << "---------------------------------- end --------------------------------------------------------------------------" << std::endl;
}

std::string PeerNode::NodelistInfo(std::vector<Node> & nodeList)
{
	std::ostringstream oss;	
	oss << std::endl;
	oss << "------------------------------------------------------------------------------------------------------------" << std::endl;
	for (auto& i : nodeList)
	{
		oss << i.InfoStr();
	}
	oss << "------------------------------------------------------------------------------------------------------------" << std::endl;
	oss << "PeerNode size is: " << nodeList.size() << std::endl;
	return oss.str();
}



bool PeerNode::locateNodeByFd(int fd, Node &node)
{
	std::shared_lock<std::shared_mutex> lck(mutexForNodes);
	for (auto x : _nodeMap)
	{
		if (x.second.fd == fd)
		{
			node = x.second;
			return true;
		}
	}
	return false;
}

bool PeerNode::verifyPeerNodeIdRequest(const int fd, const std::string &peerId)
{
	Node node;
    if(!MagicSingleton<PeerNode>::GetInstance()->locateNodeByFd(fd, node))
    {
        ERRORLOG("Invalid message peerNode_id:{}, node.address:{}", peerId, node.address);
        return false;
    }
    if(peerId != node.address)
    {
        ERRORLOG("Invalid message peerNode_id:{}, node.address:{}", peerId, node.address);
        return false;
    }
	return true;
}

// find node
bool PeerNode::FindNode(std::string const &Addr, Node &x)
{
	std::shared_lock<std::shared_mutex> lck(mutexForNodes);

	std::string strId = Addr;
	auto it = _nodeMap.find(strId);
	if (it != _nodeMap.end())
	{
		x = it->second;
		return true;
	}
	return false;
}

std::vector<Node> PeerNode::GetNodelist(NodeType type, bool mustAlive)
{
	std::shared_lock<std::shared_mutex> lck(mutexForNodes);
	std::vector<Node> rst;
	auto cb = _nodeMap.cbegin(), ce = _nodeMap.cend();
	for (; cb != ce; ++cb)
	{
		if(type == NODE_ALL || (type == NODE_PUBLIC))
		{
			if(mustAlive)
			{
				if(cb->second.IsConnected())
				{
					rst.push_back(cb->second);
				}
			}else{
				rst.push_back(cb->second);
			}
		}
	}
	return rst;
}
void PeerNode::GetNodelist(std::map<std::string, bool>& nodeAddrs, NodeType type, bool mustAlive)
{
	std::shared_lock<std::shared_mutex> lck(mutexForNodes);
	auto cb = _nodeMap.cbegin(), ce = _nodeMap.cend();
	for (; cb != ce; ++cb)
	{
		if(type == NODE_ALL || (type == NODE_PUBLIC))
		{
			if(mustAlive)
			{
				if(cb->second.IsConnected())
				{
					nodeAddrs[cb->first] = false;
				}
			}else{
				nodeAddrs[cb->first] = false;
			}
		}
	}
	return;
}

uint64_t PeerNode::GetNodelistSize()
{
	std::shared_lock<std::shared_mutex> lck(mutexForNodes);
	return _nodeMap.size();
}

// Refresh threads
extern std::atomic<int> node_list_refresh_time;
bool PeerNode::initNodeListRefreshThread()
{
	_refreshThread = std::thread(std::bind(&PeerNode::refreshNodeListThread, this));
	_refreshThread.detach();
	return true;
}

//Network nodes swap threads
bool PeerNode::nodelistSwitchThread()
{
	nodeSwitchThread = std::thread(std::bind(&PeerNode::switchNodeListThread, this));
	nodeSwitchThread.detach();
	return true;
}

void PeerNode::switchNodeListThread()
{
	do
	{
		sleep(global::node_list_refresh_time);
		if(!nodesSwapEnd)
		{
			return;
		}
		MagicSingleton<UnregisterNode>::GetInstance()->syncStartNode();
	} while (true);
}

//Thread functions
void PeerNode::refreshNodeListThread()
{
	std::lock_guard<std::mutex> lck(global::mutex_listen_thread);
	while(!global::listen_thread_inited)
	{
		global::conditionListenThread.wait(global::mutex_listen_thread);
	}
	auto configServerList = MagicSingleton<Config>::GetInstance()->GetServer();
	int port = MagicSingleton<Config>::GetInstance()->GetServerPort();
	
	std::map<std::string, int> server_list;
	for (auto & configServerIp: configServerList)
	{
		server_list.insert(std::make_pair(configServerIp, port));
	}

	MagicSingleton<UnregisterNode>::GetInstance()->startRegistrationNode(server_list);
}

int PeerNode::ConnectNode(Node & node)
{
	if (node.fd > 0)
	{
		return -1;
	}

	net_com::ANALYSIS_CONNECTION_KIND(node);
	INFOLOG("node.connKind: {}", node.connKind);

	uint32_t u32_ip = node.publicIp;
	uint16_t port = node.listenPort;

	u16 connectPort;
	int cfd = net_com::InitConnection(u32_ip, port, connectPort);
	if (cfd <= 0)
	{
		return -2;
	}

	DEBUGLOG("||||ConnectNode: ip:({}) port:({}) fd:{} ",IpPort::IpSz(u32_ip),connectPort, cfd);
	node.fd = cfd;
	node.publicPort = connectPort;
	MagicSingleton<bufferControl>::GetInstance()->AddBuffer(u32_ip, connectPort, cfd); 
	MagicSingleton<EpollMode>::GetInstance()->EpollLoop(cfd, EPOLLIN | EPOLLET | EPOLLOUT);
	
	return 0;
}

int PeerNode::DisconnectNode(Node & node)
{
	if (node.fd <= 0)
	{
		return -1;
	}

	MagicSingleton<EpollMode>::GetInstance()->DeleteEpollEvent(node.fd);
	close(node.fd);
	if(!MagicSingleton<bufferControl>::GetInstance()->DeleteBuffer(node.publicIp, node.publicPort))
	{
		ERRORLOG(RED "DeleteBuffer ERROR ip:({}), port:({})" RESET, IpPort::IpSz(node.publicIp), node.publicPort);
	}			
	return 0;
}

int PeerNode::DisconnectNode(uint32_t ip, uint16_t port, int fd)
{
	DEBUGLOG("DeleteNode ip:{}", IpPort::IpSz(IpPort::get_peer_nip_info(fd)));
	if (fd <= 0)
	{
		return -1;
	}

	MagicSingleton<EpollMode>::GetInstance()->DeleteEpollEvent(fd);
	close(fd);
	if(!MagicSingleton<bufferControl>::GetInstance()->DeleteBuffer(ip, port))
	{
		ERRORLOG(RED "DeleteBuffer ERROR ip:({}), port:({})" RESET, IpPort::IpSz(ip), port);
	}			
	return 0;
}

//Get the ID
const std::string PeerNode::GetSelfId()
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	return _currNode.address;
}

//Get pub
const std::string PeerNode::GetSelfPub()
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	return _currNode.pub;
}

// Set the ID
void PeerNode::SetSelfId(const std::string &Addr)
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	_currNode.address = Addr;
}

// Set the node ID
void PeerNode::SetSelfIdentity(const std::string & identity)
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	_currNode.identity = identity;
}

//Set the node name
void PeerNode::SetSelfName(const std::string &name)
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	_currNode.name = name;
}

//Set the node logo
void PeerNode::set_user_logo(const std::string &logo)
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	_currNode.logo = logo;
}

// Set the IP
void PeerNode::set_self_ip_public(const u32 publicIp)
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	_currNode.publicIp = publicIp;
}

// Set the IP
void PeerNode::set_self_ip_listen(const u32 listenIp)
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	_currNode.listenIp = listenIp;
}


//Set the port
void PeerNode::setPublicSelfPort(const u16 portPublic)
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	_currNode.publicPort = portPublic;
}


// Set the port
void PeerNode::configureSelfPortForListening(const u16 portListen)
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	_currNode.listenPort = portListen;
}

void PeerNode::SetSelfHeight(u32 height)
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
    _currNode.SetHeight(height);
}

void PeerNode::SetSelfHeight()
{
	if (net_callback::chain_height_handler)
	{
		uint32_t height = 0;
		int ret = net_callback::chain_height_handler(height);
		if (ret >= 0)
		{
            SetSelfHeight(height);
		}
	}
	else
	{
		INFOLOG("Set self chain height: callback empty!!!");
	}
}

void PeerNode::SetSelfVer(const std::string & ver)
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	_currNode.ver = ver;
}

u32 PeerNode::get_self_chain_height_newest()
{
	// Update to newest chain height
    SetSelfHeight();
	return _currNode.height;
}


// Own node
const Node PeerNode::GetSelfNode()
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	return _currNode;
}


// Get the  address
const std::string PeerNode::GetAddress()
{
	std::lock_guard<std::mutex> lck(mutexForCurrent);
	return _currNode.address;
}

int PeerNode::UpdateAddress(const std::string &oldPub, const std::string & newPub)
{
	if (oldPub.size() == 0 || newPub.size() == 0)
	{
		return -1;
	}
	
	std::string oldAddr = GenerateAddr(oldPub);
	std::string newAddr = GenerateAddr(newPub);
	
	Node node;
	if (!FindNode(oldAddr, node))
	{
		return -2;
	}
	
	node.address = newAddr;
	node.identity = newPub;

	DeleteNode(oldAddr);
	if (!Add(node))
	{
		return -3;
	}
	return 0;
}