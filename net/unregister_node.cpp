#include "unregister_node.h"

#include "./handle_event.h"
#include "./api.h"

#include "../utils/time_util.h"
#include "../net/peer_node.h"
#include "../utils/magic_singleton.h"
#include "../common/global_data.h"
#include "../common/config.h"
#include "../proto/net.pb.h"
#include "../utils/account_manager.h"

#include "../ca/algorithm.h"
#include "../ca/bonus_addr_cache.h"

UnregisterNode::UnregisterNode()
{
}
UnregisterNode::~UnregisterNode()
{
}

int UnregisterNode::Add(const Node & node)
{
    std::unique_lock<std::shared_mutex> lck(mutexForNodes);
    std::string key = std::to_string(node.publicIp) + std::to_string(node.publicPort);

    if(key.size() == 0)
	{
		return -1;
	} 
	auto itr = _nodes.find(key);
	if (itr != _nodes.end())
	{
		return -2;
	}
	this->_nodes[key] = node;


    return 0;
}

bool UnregisterNode::Register(std::map<uint32_t, Node> nodeMap)
{
    std::string msgId;
    uint32 sendNum = nodeMap.size();
    if (!dataMgrPtr.CreateWait(5, sendNum, msgId))
    {
        return false;
    }

    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::map<uint32_t, int> node_ip_and_fd;
    
    for (auto & unconnectedNode : nodeMap)
    {
        bool isFind = false;
        for (auto & node : nodelist)
        {
            if (unconnectedNode.second.publicIp == node.publicIp)
            {
                isFind = true;
                break;
            }
        }
        
        if (isFind)
        {
            continue;
        }

        int ret = net_com::registerNodeRequest(unconnectedNode.second, msgId, false);
        if(ret != 0)
        {
            ERRORLOG("registerNodeRequest fail ret = {}", ret);
            continue;
        }
        node_ip_and_fd.insert(std::make_pair(unconnectedNode.second.publicIp, unconnectedNode.second.fd));
    }

    std::vector<std::string> returnDatas;
    if (!dataMgrPtr.WaitData(msgId, returnDatas))//Wait for enough voting data to be received
    {
        if (returnDatas.empty())
        {
            ERRORLOG("wait Register time out send:{} recv:{}", sendNum, returnDatas.size());
            for(auto& t: node_ip_and_fd)
            {
                DEBUGLOG("wait Register time out, 111 close fd:{}, ip = {}",t.second , IpPort::IpSz(t.first));
                MagicSingleton<PeerNode>::GetInstance()->CloseFd(t.second);
            }
            return false;
        }
    }

    RegisterNodeAck registerNodeAcknowledgment;
    for (auto &retData : returnDatas)
    {
        registerNodeAcknowledgment.Clear();
        if (!registerNodeAcknowledgment.ParseFromString(retData))
        {
            continue;
        }
        uint32_t ip = registerNodeAcknowledgment.from_ip();
        uint32_t port = registerNodeAcknowledgment.from_port();

        auto find = node_ip_and_fd.find(ip);
        if(find != node_ip_and_fd.end())
        {
            node_ip_and_fd.erase(find);
        }

        DEBUGLOG("UnregisterNode::Register ip:{}, port:{}", ip, port);
        std::cout << "registerNodeAcknowledgment.nodes_size(): " << registerNodeAcknowledgment.nodes_size() <<std::endl;
        if(registerNodeAcknowledgment.nodes_size() <= 1)
	    {
            const NodeInfo &nodeinfo = registerNodeAcknowledgment.nodes(0);
			if (MagicSingleton<bufferControl>::GetInstance()->IsExists(ip, port) /* && node.is_established()*/)
			{
                DEBUGLOG("handleRegisterNodeAcknowledgment--FALSE from.ip: {}", IpPort::IpSz(ip));
                auto ret = RegistrationVerifier(nodeinfo, ip, port);
                if(ret < 0)
                {
                    DEBUGLOG("RegistrationVerifier error ,ip : {}, port: {}, ret:{}", ip, port, ret);
                }
			}
        }
    }

    for(auto& t: node_ip_and_fd)
    {
        DEBUGLOG("node_ip_and_fd, 111 close fd:{}, ip = {}, fd:{}",t.second , IpPort::IpSz(t.first), t.second);
        MagicSingleton<PeerNode>::GetInstance()->CloseFd(t.second);
    }

    return true;
}
bool UnregisterNode::startRegistrationNode(std::map<std::string, int> &serverList)
{
    std::string msgId;
    uint32 sendNum = serverList.size();
    if (!dataMgrPtr.CreateWait(5, sendNum, msgId))
    {
        return false;
    }
    Node selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
    for (auto & item : serverList)
	{
        //The party actively establishing the connection
		Node node;
		node.publicIp = IpPort::IpNum(item.first);
		node.listenIp = selfNode.listenIp;
		node.listenPort = kServerMainPort;

		if (item.first == global::g_localIp)
		{
			continue;
		}

		int ret = net_com::registerNodeRequest(node, msgId, true);
        if(ret != 0)
        {
            ERRORLOG("startRegistrationNode error ret : {}", ret);
        }
	}

    std::vector<std::string> returnDatas;
    if (!dataMgrPtr.WaitData(msgId, returnDatas))//Wait for enough voting data to be received
    {
        if (returnDatas.empty())
        {
            ERRORLOG("wait startRegistrationNode time out send:{} recv:{}", sendNum, returnDatas.size());
            return false;
        }
    }
    RegisterNodeAck registerNodeAcknowledgment;
    std::map<uint32_t, Node> nodeMap;


    for (auto &retData : returnDatas)
    {
        registerNodeAcknowledgment.Clear();
        if (!registerNodeAcknowledgment.ParseFromString(retData))
        {
            continue;
        }

        uint32_t ip = registerNodeAcknowledgment.from_ip();
        uint32_t port = registerNodeAcknowledgment.from_port();
        uint32_t fd = registerNodeAcknowledgment.fd();
        for (int i = 0; i < registerNodeAcknowledgment.nodes_size(); i++)
	    {
            const NodeInfo &nodeinfo = registerNodeAcknowledgment.nodes(i);
            {
                Node node;
                node.listenIp = selfNode.listenIp;
	            node.listenPort = kServerMainPort;
                node.publicIp = nodeinfo.public_ip();
                node.address = nodeinfo.addr();
            }
            if(nodeinfo.addr() == selfNode.address)
            {
                continue;
            }
            if(i == 0)
            {
                //Determine if TCP is connected
                if (MagicSingleton<bufferControl>::GetInstance()->IsExists(ip, port))
                {
                    DEBUGLOG("handleRegisterNodeAcknowledgment--TRUE from.ip: {}", IpPort::IpSz(ip));
                    auto ret = RegistrationVerifier(nodeinfo, ip, port);
                    if(ret < 0)
                    {
                        DEBUGLOG("RegistrationVerifier error ip:{}, port:{}, ret:{}", IpPort::IpSz(ip), port, ret);
                        MagicSingleton<PeerNode>::GetInstance()->DisconnectNode(ip, port, fd);
                        continue;
                    }
                }
            }
            else
            {
                Node node;
                node.listenIp = selfNode.listenIp;
	            node.listenPort = kServerMainPort;
                node.publicIp = nodeinfo.public_ip();
                DEBUGLOG("Add NodeList--TRUE ip: {}", IpPort::IpSz(node.publicIp));
                if(nodeMap.find(node.publicIp) == nodeMap.end())
                {
                    DEBUGLOG("add ip:{}, port:{}",IpPort::IpSz(ip), node.publicIp);
                    nodeMap[nodeinfo.public_ip()] = node;
                }
            } 
        }
    }
    Register(nodeMap);
    return true;
}

bool UnregisterNode::syncStartNode()
{
    std::string msgId;
    std::vector<Node> node_list = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    uint32 sendNum = node_list.size();
    if (!dataMgrPtr.CreateWait(5, sendNum, msgId))
    {
        return false;
    }
    Node selfNode = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
    
    for (auto & node : node_list)
    {
        //Determine if TCP is connected
        if (MagicSingleton<bufferControl>::GetInstance()->IsExists(node.publicIp, node.publicPort) /* && node.is_established()*/)
        {
            net_com::synchronizeNodeRequest(node, msgId);
        }
        else
        {
            DEBUGLOG("synchronizeNodeRequest error id:{} ip:{} port:{}", node.address, IpPort::IpSz(node.publicIp), node.publicPort);
        }
    }

    std::vector<std::string> returnDatas;
    if (!dataMgrPtr.WaitData(msgId, returnDatas))//Wait for enough voting data to be received
    {
        if (returnDatas.empty())
        {
            ERRORLOG("wait startRegistrationNode time out send:{} recv:{}", sendNum, returnDatas.size());
            return false;
        }
    }
    SyncNodeAck syncNodeAcknowledgment;
    std::map<uint32_t, Node> nodeMap;
    std::vector<Node> syncNodes;
    std::map<std::string, std::vector<Node>> vrfNodeList;

    
    auto VerifySign = [&](const CSign & sign, const std::string & serHash) -> int{
        if (sign.sign().size() == 0 || sign.pub().size() == 0)
        {
            return -1;
        }
        if (serHash.size() == 0)
        {
            return -2;
        }

        EVP_PKEY* eckey = nullptr;
        if(get_ed_pub_key_by_bytes(sign.pub(), eckey) == false)
        {
            EVP_PKEY_free(eckey);
            ERRORLOG("Get public key from bytes failed!");
            return -3;
        }

        if(ed25519VerificationMessage(serHash, eckey, sign.sign()) == false)
        {
            EVP_PKEY_free(eckey);
            ERRORLOG("Public key verify sign failed!");
            return -4;
        }
        EVP_PKEY_free(eckey);
        return 0;
    };

    for (auto &retData : returnDatas)
    {
        syncNodeAcknowledgment.Clear();
        if (!syncNodeAcknowledgment.ParseFromString(retData))
        {
            continue;
        }
        auto syncNodeAcknowledgmentMessage = syncNodeAcknowledgment;
        syncNodeAcknowledgmentMessage.clear_sign();
        std::string vinHashSerialized = Getsha256hash(syncNodeAcknowledgmentMessage.SerializeAsString());

        int verificationResult = VerifySign(syncNodeAcknowledgment.sign(), vinHashSerialized);
        if (verificationResult != 0)
        {
            ERRORLOG("targetNodelist VerifySign fail!!!");
            continue;
        }

        std::vector<Node> target_address_list;
        for (int i = 0; i < syncNodeAcknowledgment.nodes_size(); i++)
	    {
            const NodeInfo &nodeinfo = syncNodeAcknowledgment.nodes(i);
            if(nodeinfo.addr() == selfNode.address)
            {
                continue;
            }
            Node node;
            node.listenIp = selfNode.listenIp;
            node.listenPort = kServerMainPort;
            node.publicIp = nodeinfo.public_ip();
            node.address = nodeinfo.addr();
            node.height = nodeinfo.height();

            if(nodeMap.find(node.publicIp) == nodeMap.end())
            {
                
                nodeMap[nodeinfo.public_ip()] = node;
            }
            target_address_list.push_back(node);

        }
        vrfNodeList[syncNodeAcknowledgment.ids()] = target_address_list;
    }

    for(auto & item : vrfNodeList)
    {
        std::sort(item.second.begin(), item.second.end(), compareStructs);
        auto last = std::unique(item.second.begin(), item.second.end(), compareStructs);
        item.second.erase(last, item.second.end());
        DEBUGLOG(" sort and unique @@@@@@ ");
        for(auto & i : item.second)
        {
            syncNodes.push_back(i);
        }
    }

    //Count the number of IPs and the number of times they correspond to IPs
    {
        std::map<Node,int, NodeCompare> syncNodeCount;
        for(auto it = syncNodes.begin(); it != syncNodes.end(); ++it)
        {
            syncNodeCount[*it]++;
        }
        data_to_split_and_insert(syncNodeCount);
        syncNodes.clear();
        syncNodeCount.clear();
    }

    //Only the latest elements are stored in the maintenance map map
    if(stake_node_list.size() == 2 || unstakeNodeList.size() == 2)
    {
        clear_split_node_list_data();
    }

    if(nodeMap.empty())
    {
        auto configServerList = MagicSingleton<Config>::GetInstance()->GetServer();
        int port = MagicSingleton<Config>::GetInstance()->GetServerPort();
        
        std::map<std::string, int> serverList;
        for (auto & configServerIp: configServerList)
        {
            serverList.insert(std::make_pair(configServerIp, port));
        }

        MagicSingleton<UnregisterNode>::GetInstance()->startRegistrationNode(serverList);
    }
    else
    {
        Register(nodeMap);
    }

    return true;
}



void UnregisterNode::GetIpMap(std::map<uint64_t, std::map<std::string, int>> & m1,std::map<uint64_t, std::map<std::string, int>> & m2)
{
    std::unique_lock<std::mutex> locker(mutexStackList);
    m1 = stake_node_list;
    m2 = unstakeNodeList;
}


void UnregisterNode::deleteSplitNodeList(const std::string & addr)
{
    std::unique_lock<std::mutex> locker(mutexStackList);
    if(stake_node_list.empty() || unstakeNodeList.empty())
    {
        ERRORLOG("list is empty!");
        return;
    }

    for(auto & [_,iter] : stake_node_list)
    {
        for(auto iter2 = iter.begin();iter2 != iter.end(); ++iter2)
        {
            if(iter2->first == addr)
            {
                iter2 = iter.erase(iter2);
                return;
            }
        }
    }
    
    for(auto & [_,iter] : unstakeNodeList)
    {
        for(auto iter2 = iter.begin();iter2 != iter.end(); ++iter2)
        {
            if(iter2->first == addr)
            {
                iter2 = iter.erase(iter2);
                return;
            }
        }
    }
}

void UnregisterNode::get_consensus_stake_node_list(std::map<std::string,int>& consensusStakeNodeMap_)
{
    std::unique_lock<std::mutex> lck(mutexStackList);
    if(stake_node_list.empty())
    {
        return;
    }
    consensusStakeNodeMap_.insert(stake_node_list.rbegin()->second.begin(), stake_node_list.rbegin()->second.end());
    return;
}

void UnregisterNode::get_consensus_node_list(std::map<std::string,int>& consensus_node_map)
{
    std::unique_lock<std::mutex> lck(mutexStackList);
    if(stake_node_list.empty() || unstakeNodeList.empty())
    {
        return;
    }
    consensus_node_map.insert(stake_node_list.rbegin()->second.begin(), stake_node_list.rbegin()->second.end());
    
    for(const auto& it : unstakeNodeList.rbegin()->second)
    {
        consensus_node_map[it.first] = it.second;
    }
    return;
}

void UnregisterNode::clear_split_node_list_data()
{
    std::unique_lock<std::mutex> lck(mutexStackList);
    auto it = stake_node_list.begin();
    stake_node_list.erase(it);

    auto _it = unstakeNodeList.begin();
    unstakeNodeList.erase(_it);
    DEBUGLOG("clear_split_node_list_data @@@@@ ");
}

static int calculateAverage(const std::vector<int>& vec)
{
    if (vec.empty()) {
        std::cout << "Error: Vector is empty." << std::endl;
        return 0;
    }

    int sum = 0;
    
    for (int num : vec)
    {
        sum += num;
    }
    
    int average = static_cast<double>(sum) / vec.size();
    
    return average;
}

void UnregisterNode::data_to_split_and_insert(const std::map<Node, int, NodeCompare>  syncNodeCount)
{
    std::unique_lock<std::mutex> locker(mutexStackList);
    std::map<std::string, int>  m_stakeSyncNodeCount;
    std::map<std::string, int>  unstakeSyncNodeCount;

    auto VerifyBonusAddr = [](const std::string& bonusAddr) -> int
    {
        uint64_t delegateAmount;
        auto ret = MagicSingleton<mm::ca::BonusAddrCache>::GetInstance()->getAmount(bonusAddr, delegateAmount); 
        if (ret < 0)
        {
            return -99;
        }
        return delegateAmount >= global::ca::kMinDelegatingAmt ? 0 : -99;
    };

    DBReader dbReader;

    std::string assetType;
    int ret = ca_algorithm::GetCanBeRevokeAssetType(assetType);
    if(ret != 0){
        ERRORLOG("Get Can BeRevoke AssetType fail!");
    }

    DEBUGLOG("data_to_split_and_insert @@@@@ ");
    for(auto & item : syncNodeCount)
    {
        //Verification of Delegating and pledge
        int ret = VerifyBonusAddr(item.first.address);
        std::vector<std::string> pledgeUtxoHashes;
        int retValue = dbReader.getStakeAddrUtxo(item.first.address,assetType, pledgeUtxoHashes);
        bool HasPledged = DBStatus::DB_SUCCESS == retValue || !pledgeUtxoHashes.empty();
        if (HasPledged && ret == 0)
        {
            m_stakeSyncNodeCount.insert(std::make_pair(item.first.address,item.second));
        }
        else
        {
            unstakeSyncNodeCount.insert(std::make_pair(item.first.address,item.second));
        }
    }

    uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    stake_node_list[nowTime] = m_stakeSyncNodeCount;
    unstakeNodeList[nowTime] = unstakeSyncNodeCount;
}