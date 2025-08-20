#include "ca/dispatchtx.h"
#include "include/logging.h"
#include "net.pb.h"
#include "net/peer_node.h"

ContractDispatcher::ContractDispatcher() = default;

ContractDispatcher::~ContractDispatcher() = default;

void ContractDispatcher::AddContractInfo(const std::string& contract_hash, const std::vector<std::string>& dependent_addresses) {
    MagicSingleton<DependencyManager>::GetInstance()->AddContractInfo(contract_hash, dependent_addresses);
}

void ContractDispatcher::AddContractMessageRequest(const std::string& contract_hash, const ContractTxMsgReq& msg) {
    MagicSingleton<DependencyManager>::GetInstance()->AddMessageRequest(contract_hash, msg);
}

void ContractDispatcher::Process() {
    MagicSingleton<TimerProcessor>::GetInstance()->Start();
}

void ContractDispatcher::SetTimeValue(const uint64_t& new_value) {
    MagicSingleton<TimerProcessor>::GetInstance()->SetTimeValue(new_value);
}

void DependencyManager::AddContractInfo(const std::string& contract_hash, const std::vector<std::string>& dependent_contracts) {
    std::unique_lock<std::mutex> locker(dep_mutex_);
    contract_dep_cache_[contract_hash] = dependent_contracts;

    locker.unlock();
    BroadcastAddedDependency(contract_hash, dependent_contracts);
}

void DependencyManager::AddMessageRequest(const std::string& contract_hash, const ContractTxMsgReq& msg) {
    std::unique_lock<std::mutex> locker(msg_mutex_);
    TxMsgReq txmsg = msg.txmsgreq();
    contract_msg_cache_[contract_hash] = txmsg;
}

std::vector<std::vector<TxMsgReq>> DependencyManager::GetDependentData() {
    std::unique_lock<std::mutex> msg_lock(msg_mutex_);
    const auto& msg_cache = contract_msg_cache_;
    msg_lock.unlock();

    DEBUGLOG("Gathering dependent data");
    std::vector<std::set<std::string>> grouped_deps;
    {
        std::unique_lock<std::mutex> dep_lock(dep_mutex_);
        for (const auto& [key, values] : contract_dep_cache_) {
            std::set<std::string> common_keys{key};
            for (const auto& [other_key, other_values] : contract_dep_cache_) {
                if (key == other_key) continue;
                if (HasDuplicate(values, other_values)) {
                    common_keys.insert(other_key);
                }
            }
            if (!common_keys.empty()) {
                bool found = false;
                for (auto& item_set : grouped_deps) {
                    std::set<std::string> intersection;
                    std::set_intersection(item_set.begin(), item_set.end(), common_keys.begin(), common_keys.end(),
                                          std::inserter(intersection, intersection.begin()));
                    if (!intersection.empty()) {
                        item_set.insert(common_keys.begin(), common_keys.end());
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    grouped_deps.push_back(common_keys);
                }
            }
        }
    }
    std::vector<std::vector<TxMsgReq>> grouped_msgs;
    for (const auto& hash_container : grouped_deps) {
        std::vector<TxMsgReq> msg_vec;
        for (const auto& hash : hash_container) {
            auto it = msg_cache.find(hash);
            if (it != msg_cache.end()) {
                msg_vec.push_back(it->second);
            }
        }
        if (!msg_vec.empty()) {
            grouped_msgs.push_back(msg_vec);
        }
    }
    return grouped_msgs;
}

std::vector<std::vector<TxMsgReq>> DependencyManager::GroupData(const std::vector<std::vector<TxMsgReq>>& tx_vectors) {
    constexpr int PARTS = 8;
    size_t size = tx_vectors.size();
    std::vector<std::vector<TxMsgReq>> grouped;
    if (size <= PARTS) {
        grouped = tx_vectors;
    } else {
        size_t per_part = size / PARTS;
        size_t remain = size % PARTS;
        size_t index = 0;
        for (int i = 0; i < PARTS; ++i) {
            std::vector<TxMsgReq> part;
            size_t end = index + per_part;
            for (size_t j = index; j < end; ++j) {
                part.insert(part.end(), tx_vectors[j].begin(), tx_vectors[j].end());
            }
            index = end;
            if (remain > 0) {
                size_t extra_end = index + remain;
                for (size_t j = index; j < extra_end; ++j) {
                    part.insert(part.end(), tx_vectors[j].begin(), tx_vectors[j].end());
                }
                index = extra_end;
                remain = 0;
            }
            grouped.push_back(std::move(part));
        }
    }
    return grouped;
}

bool DependencyManager::HasDuplicate(const std::vector<std::string>& vec1, const std::vector<std::string>& vec2) {
    std::unordered_set<std::string> dep_set(vec1.begin(), vec1.end());
    for (const auto& s : vec2) {
        if (dep_set.count(s) > 0) {
            return true;
        }
    }
    return false;
}

int MessageDispatcher::DistributionContractTransactionRequest(std::multimap<std::string, ContractDispatcher::MsgInfo>& distribution,
                                                              const std::vector<std::vector<TxMsgReq>>& grouped_data) {
    for (const auto& msg_container : grouped_data) {
        std::string hash_concat;
        std::vector<TxMsgReq> tx_data = msg_container;
        std::vector<uint64_t> timestamps;
        for (const auto& tx_msg : msg_container) {
            CTransaction tx;
            if (!tx.ParseFromString(tx_msg.txmsginfo().tx())) {
                ERRORLOG("Failed to parse transaction");
                continue;
            }
            hash_concat += tx.hash();
            timestamps.push_back(tx.time());
        }
        uint64_t max_time = timestamps.empty() ? 0 : *std::max_element(timestamps.begin(), timestamps.end());

        std::string input_hash = Getsha256hash(hash_concat);
        std::string pack_addr;
        int ret = FindContractPackNode(input_hash, max_time, pack_addr);
        if (ret != 0) {
            ERRORLOG("Find pack node failed: {}", ret);
            continue;
        }

        ContractDispatcher::MsgInfo info;
        info.tx_msg_req = std::move(tx_data);
        distribution.emplace(pack_addr, info);
    }
    if (distribution.empty()) {
        ERRORLOG("Distribution map is empty");
        return -3;
    }
    return 0;
}

int MessageDispatcher::SendTransactionInfoRequest(const std::string& packager, std::vector<TxMsgReq>& tx_msgs) {
    DEBUGLOG("Sending transaction info request");
    ContractPackagerMsg msg;
    msg.set_version(global::GetVersion());

    for (auto& msg_req : tx_msgs) {
        *msg.add_txmsgreq() = msg_req;
    }

    std::string owner = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    std::string serialized_hash = Getsha256hash(msg.SerializeAsString());
    std::string signature;
    std::string pub;
    if (TxHelper::Sign(owner, serialized_hash, signature, pub) != 0) {
        ERRORLOG("Signature failed");
        return -1;
    }

    CSign* sign = msg.mutable_sign();
    sign->set_sign(signature);
    sign->set_pub(pub);

    if (owner == packager) {
        DEBUGLOG("Owner and packager are the same: {}", owner);
        auto send_txmsg = std::make_shared<ContractPackagerMsg>(msg);
        MsgData msgdata;
        handleContractPackagerMessage(send_txmsg, msgdata);
        return 0;
    }
    NetSendMessage<ContractPackagerMsg>(packager, msg, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    return 0;
}

void TimerProcessor::Start() {
    running_ = true;
    timer_thread_ = std::thread(&TimerProcessor::ProcessingFunc, this);
    timer_thread_.detach();
}

void TimerProcessor::Stop() {
    running_ = false;
    timer_cv_.notify_all();
}

void TimerProcessor::SetTimeValue(uint64_t value) {
    std::unique_lock<std::mutex> lock(timer_mutex_);
    if (time_value_ == 0) {
        time_value_ = value;
    }
    timer_cv_.notify_all();
}

void TimerProcessor::ProcessingFunc() {
    uint64_t time_baseline = 0;
    while (running_) {
        {
            std::unique_lock<std::mutex> lock(timer_mutex_);
            if (timer_cv_.wait_for(lock, std::chrono::seconds(1), [this]{ return !running_; })) {
                break;
            }
        }

        time_value_ += 1000000;

        if (time_baseline == 0) {
            time_baseline = MagicSingleton<TimeUtil>::GetInstance()->getTimestampPerUnit(time_value_);
        }

        if (time_value_ >= time_baseline + kContractWaitingTime) {
            auto dependent_data = MagicSingleton<DependencyManager>::GetInstance()->GetDependentData();
            if (dependent_data.empty()) {
                ERRORLOG("+++++++++++++++++ dependent_data is empty");
                continue;
            }

            auto grouped_data = MagicSingleton<DependencyManager>::GetInstance()->GroupData(dependent_data);
            if (grouped_data.empty()) {
                ERRORLOG("+++++++++++++++++ grouped_data is empty");
                continue;
            }

            std::multimap<std::string, ContractDispatcher::MsgInfo> distribution;
            int ret = MagicSingleton<MessageDispatcher>::GetInstance()->DistributionContractTransactionRequest(distribution, grouped_data);
            if (ret != 0) {
                ERRORLOG("++++++++++++++++ distributionContractTransactionRequest error ret :{} ++++++++++++++++++", ret);
                continue;
            }

            for (auto& item : distribution) {
                DEBUGLOG("Processing distribution item for packager: {}, Number of transactions: {}", item.first, item.second.tx_msg_req.size());
                DEBUGLOG("Sending transaction request to packager: {}", item.first);
                DEBUGLOG("-0-0-0-0-0-0-0-0- send packager : {}, Number of transactions: {} ", item.first, item.second.tx_msg_req.size());
                for(auto & txmsg : item.second.tx_msg_req)
                {
                    CTransaction contractTx;
                    if (!contractTx.ParseFromString(txmsg.txmsginfo().tx()))
                    {
                        ERRORLOG("Failed to deserialize transaction body!");
                        continue;
                    }
                    DEBUGLOG("send packager : {} , tx hash : {}", item.first, contractTx.hash());
                }
                MagicSingleton<MessageDispatcher>::GetInstance()->SendTransactionInfoRequest(item.first, item.second.tx_msg_req);
            }

            MagicSingleton<DependencyManager>::GetInstance()->ClearCaches();
            time_baseline = 0;
            time_value_ = 0;
        }
    }
}

void DependencyManager::ClearCaches() {
    std::scoped_lock lock(dep_mutex_, msg_mutex_);
    contract_dep_cache_.clear();
    contract_msg_cache_.clear();
}

std::map<std::string, std::vector<std::string>> DependencyManager::GetAllDependenciesWithTxHashes() const {
    std::map<std::string, std::vector<std::string>> dependency_tx_map;
    
    std::unordered_map<std::string, std::vector<std::string>> dep_cache_copy;
    std::unordered_map<std::string, TxMsgReq> msg_cache_copy;
    std::unordered_map<std::string, std::vector<std::string>> dependency_tx_hashes_copy;
    
    {
        std::lock_guard<std::mutex> lock1(dep_mutex_);
        std::lock_guard<std::mutex> lock2(msg_mutex_);
        std::lock_guard<std::shared_mutex> lock3(dependency_tx_mutex_);
        dep_cache_copy = contract_dep_cache_;
        msg_cache_copy = contract_msg_cache_;
        dependency_tx_hashes_copy = dependency_tx_hashes_;
    }
    
    for (const auto& [contract_hash, deps] : dep_cache_copy) {
        auto msg_it = msg_cache_copy.find(contract_hash);
        if (msg_it != msg_cache_copy.end()) {
            CTransaction tx;
            if (tx.ParseFromString(msg_it->second.txmsginfo().tx())) {
                std::string tx_hash = tx.hash();
                
                for (const auto& dep_addr : deps) {
                    if (!dep_addr.empty()) {
                        dependency_tx_map[dep_addr].push_back(tx_hash);
                        DEBUGLOG("Adding dependency from cache: {} -> {}", dep_addr, tx_hash);
                    }
                }
            }
        }
    }
    
    for (const auto& [dep_addr, tx_hashes] : dependency_tx_hashes_copy) {
        if (dep_addr.empty() || tx_hashes.empty()) {
            continue;
        }
        
        auto& existing_hashes = dependency_tx_map[dep_addr];
        for (const auto& tx_hash : tx_hashes) {
            if (!tx_hash.empty() && std::find(existing_hashes.begin(), existing_hashes.end(), tx_hash) == existing_hashes.end()) {
                existing_hashes.push_back(tx_hash);
                DEBUGLOG("Adding dependency from tx hashes map: {} -> {}", dep_addr, tx_hash);
            }
        }
    }
    
    DEBUGLOG("Total unique dependency addresses: {}", dependency_tx_map.size());
    return dependency_tx_map;
}

std::set<std::string> DependencyManager::GetAllUniqueDependencies() const {
    std::unique_lock<std::mutex> lock(dep_mutex_);
    std::set<std::string> all_dependencies;
    for (const auto& pair : contract_dep_cache_) {
        all_dependencies.insert(pair.second.begin(), pair.second.end());
    }
    return all_dependencies;
}

void MessageDispatcher::BroadcastDependencies() {
    auto dependency_tx_map = MagicSingleton<DependencyManager>::GetInstance()->GetAllDependenciesWithTxHashes();
    
    if (dependency_tx_map.empty()) {
        DEBUGLOG("No dependencies to broadcast");
        return;
    }
    
    DEBUGLOG("Broadcasting dependencies with transaction hashes");
    ContractDependencyBroadcastMsg broadcast_msg;
    broadcast_msg.set_version(global::GetVersion());

    for (const auto& [dep_addr, tx_hashes] : dependency_tx_map) {
        if (tx_hashes.empty()) {
            continue; 
        }
        
        auto* dep_info = broadcast_msg.add_dependencies();
        dep_info->set_dependencyaddress(dep_addr);

        for (const auto& tx_hash : tx_hashes) {
            dep_info->add_txhashes(tx_hash);
        }
        
        DEBUGLOG("Adding dependency {} with {} transaction hashes", dep_addr, tx_hashes.size());
    }

    if (broadcast_msg.dependencies_size() == 0) {
        DEBUGLOG("@@@@ No valid dependencies to broadcast");
        return;
    }
    
    std::string owner = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    std::string serialized = broadcast_msg.SerializeAsString();
    std::string serialized_hash = Getsha256hash(serialized);
    std::string signature;
    std::string pub;
    
    if (TxHelper::Sign(owner, serialized_hash, signature, pub) == 0) {
        CSign* sign = broadcast_msg.mutable_sign();
        sign->set_sign(signature);
        sign->set_pub(pub);
        
        auto nodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist(NODE_ALL, true);
        int broadcast_count = 0;
        
        for (const auto& node : nodes) {
            if (node.address != owner) {
                bool success = NetSendMessage(node.address, broadcast_msg, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::kPriorityLow0);
                if (success) {
                    broadcast_count++;
                }
            }
        }
        
        DEBUGLOG("Successfully broadcasted dependency information to {} nodes", broadcast_count);
    } else {
        ERRORLOG("Failed to sign broadcast message");
    }
}


void DependencyManager::BroadcastAddedDependency(const std::string& contract_hash, const std::vector<std::string>& dependent_addresses) {
    if (dependent_addresses.empty()) {
        DEBUGLOG("@@@@ No dependencies to broadcast for contract {}", contract_hash);
        return;
    }
    
    DEBUGLOG("@@@@ Broadcasting added dependency for contract {} with {} addresses", contract_hash, dependent_addresses.size());

    {
        std::unique_lock<std::shared_mutex> lock(dependency_tx_mutex_);
        for (const auto& dep_addr : dependent_addresses) {
            if (dep_addr.empty()) continue;
            
            if (dependency_tx_hashes_.find(dep_addr) == dependency_tx_hashes_.end()) {
                dependency_tx_hashes_[dep_addr] = {};
            }
            
            auto& existing_hashes = dependency_tx_hashes_[dep_addr];
            if (std::find(existing_hashes.begin(), existing_hashes.end(), contract_hash) == existing_hashes.end()) {
                existing_hashes.push_back(contract_hash);
                DEBUGLOG("@@@@ Locally added dependency {} with tx hash {}", dep_addr, contract_hash);
            }
        }
    }
    
    ContractDependencyBroadcastMsg broadcast_msg;
    broadcast_msg.set_version(global::GetVersion());
    
    for (const auto& dep_addr : dependent_addresses) {
        if (dep_addr.empty()) continue;
        auto* dep_info = broadcast_msg.add_dependencies();
        dep_info->set_dependencyaddress(dep_addr);
        dep_info->add_txhashes(contract_hash);
        DEBUGLOG("@@@@ Adding dependency {} with tx hash {}", dep_addr, contract_hash);
    }
    
    if (broadcast_msg.dependencies_size() == 0) {
        DEBUGLOG("No valid dependencies to broadcast");
        return;
    }
    
    std::string owner = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    std::string serialized = broadcast_msg.SerializeAsString();
    std::string serialized_hash = Getsha256hash(serialized);
    std::string signature;
    std::string pub;
    
    if (TxHelper::Sign(owner, serialized_hash, signature, pub) == 0) {
        CSign* sign = broadcast_msg.mutable_sign();
        sign->set_sign(signature);
        sign->set_pub(pub);
        
        auto nodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist(NODE_ALL, true);
        int broadcast_count = 0;
        
        for (const auto& node : nodes) {
            if (node.address != owner) {
                bool success = NetSendMessage(node.address, broadcast_msg, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::kPriorityLow0);
                if (success) {
                    broadcast_count++;
                }
            }
        }
        
        DEBUGLOG("@@@@ Successfully broadcasted added dependency to {} nodes", broadcast_count);
    } else {
        ERRORLOG("Failed to sign broadcast message");
    }
}


int DependencyManager::HandleContractDependencyBroadcastMsg(const std::shared_ptr<ContractDependencyBroadcastMsg>& msg, const MsgData &msgData)
{
    DEBUGLOG("HandleContractDependencyBroadcastMsg");
    if (!msg) {
        ERRORLOG("ContractDependencyBroadcastMsg is null");
        return -1;
    }
    
    if (!msg->has_sign())
    {
        ERRORLOG("ContractDependencyBroadcastMsg missing signature");
        return -2;
    }

    if (0 != Util::IsVersionCompatible(msg->version()))
    {
        ERRORLOG("ContractDependencyBroadcastMsg version incompatible");
        return -3;
    }

    CSign sign = msg->sign();
    std::string pub = sign.pub();
    std::string signature = sign.sign();
    
    if (pub.empty() || signature.empty()) {
        ERRORLOG("ContractDependencyBroadcastMsg has empty public key or signature");
        return -4;
    }
    
    ContractDependencyBroadcastMsg msg_copy = *msg;
    msg_copy.clear_sign();
    std::string message_hash = Getsha256hash(msg_copy.SerializeAsString());
    
    Account account;
    if(MagicSingleton<AccountManager>::GetInstance()->getAccountPublicKeyByBytes(pub, account) == false){
        ERRORLOG("ContractDependencyBroadcastMsg Get public key from bytes failed!");
        return -5;
    }

    if(account.Verify(message_hash, signature) == false)
    {
        ERRORLOG("ContractDependencyBroadcastMsg Public key verify sign failed!");
        return -6;
    }

    std::string sender = GenerateAddr(pub);
    if (sender.empty())
    {
        ERRORLOG("ContractDependencyBroadcastMsg failed to get sender address");
        return -7;
    }

    if (msg->dependencies_size() == 0) {
        DEBUGLOG("ContractDependencyBroadcastMsg from {} contains no dependencies", sender);
        return 0;
    }

    StoreBroadcastDependencies(*msg);
    
    DEBUGLOG("Successfully processed ContractDependencyBroadcastMsg from {} with {} dependencies", 
             sender, msg->dependencies_size());
    return 0;
}

void DependencyManager::StoreBroadcastDependencies(const ContractDependencyBroadcastMsg& msg) {
    std::unique_lock<std::shared_mutex> lock(dependency_tx_mutex_);

    std::string sender;
    if (msg.has_sign()) {
        std::string pub = msg.sign().pub();
        sender = GenerateAddr(pub);
        if (sender.empty()) {
            ERRORLOG("Failed to get sender address from public key");
            return;
        }
    } else {
        ERRORLOG("ContractDependencyBroadcastMsg has no signature");
        return;
    }
    DEBUGLOG("@@@@ StoreBroadcastDependencies, sender: {}", sender);

    for (const auto& dep_info : msg.dependencies()) {
        std::string dep_addr = dep_info.dependencyaddress();
        if (dep_addr.empty()) {
            ERRORLOG("Empty dependency address in broadcast message");
            continue;
        }

        if (dependency_tx_hashes_.find(dep_addr) == dependency_tx_hashes_.end()) {
            dependency_tx_hashes_[dep_addr] = {};
        }

        if (dep_info.txhashes_size() == 0) {
            DEBUGLOG("@@@@ No transaction hashes for dependency address: {}", dep_addr);
            continue;
        }

        auto& existing_hashes = dependency_tx_hashes_[dep_addr];
        int new_hashes_count = 0;

        DEBUGLOG("@@@@ Before storing, dependency {} has hashes size: {}", dep_addr, existing_hashes.size());

        for (const auto& hash : dep_info.txhashes()) {
            if (hash.empty()) {
                continue;
            }
            
            if (std::find(existing_hashes.begin(), existing_hashes.end(), hash) == existing_hashes.end()) {
                DEBUGLOG("@@@@ New transaction hash {} for dependency {}", hash, dep_addr);
                existing_hashes.push_back(hash);
                new_hashes_count++;
            }
        }
        
        DEBUGLOG("@@@@ Stored broadcast dependency: {} from {} with {} new transaction hashes (total: {})", dep_addr, sender, new_hashes_count, existing_hashes.size());
    }
}

bool DependencyManager::CanSendTransaction(const ContractTxMsgReq& msg) const {
    std::shared_lock<std::shared_mutex> dependency_lock(dependency_tx_mutex_);

    if (msg.txmsgreq().txmsginfo().contractstoragelist_size() == 0) {
        DEBUGLOG("@@@@ No dependent addresses in message, can send transaction");
        return true;
    }
    
    CTransaction contractTx;
	if (!contractTx.ParseFromString(msg.txmsgreq().txmsginfo().tx()))
	{
		ERRORLOG("Failed to deserialize transaction body!");
		return false;
	}

    std::string tx_hash = contractTx.hash();
    if (tx_hash.empty()) {
        ERRORLOG("Transaction hash is empty in message");
        return false;
    }
    
    bool can_send = true;
    DBReader dbReader;
    
    for (const auto& dependent_identifier : msg.txmsgreq().txmsginfo().contractstoragelist()) {
        if (dependent_identifier.empty()) {
            DEBUGLOG("Empty dependent address in message, skipping");
            continue;
        }
        
        DEBUGLOG("@@@@ Checking dependency for address: {}", dependent_identifier);

        DEBUGLOG("@@@@ Current dependency_tx_hashes_ keys:");
        for (const auto& pair : dependency_tx_hashes_) {
            DEBUGLOG("@@@@   Key: '{}' (length: {})", pair.first, pair.first.length());
        }
        
        auto tx_hashes_it = dependency_tx_hashes_.find(dependent_identifier);
        if (tx_hashes_it == dependency_tx_hashes_.end()) {
            DEBUGLOG("@@@@ No transaction hashes found for dependency '{}' (length: {}), can send transaction", dependent_identifier, dependent_identifier.length());
            continue;
        }
        
        if(!tx_hashes_it->second.empty()){
            can_send = false;
        }
    }
    
    DEBUGLOG("@@@@ Final decision for transaction {}: {}", tx_hash, can_send ? "can send" : "cannot send");
    return can_send;
}

int HandleContractDependencyBroadcastMsg(const std::shared_ptr<ContractDependencyBroadcastMsg> &msg, const MsgData &msgData)
{
    DEBUGLOG("HandleContractDependencyBroadcastMsg");
    if (!msg) {
        ERRORLOG("Received null ContractDependencyBroadcastMsg");
        return -1;
    }
    
    try {
        MagicSingleton<DependencyManager>::GetInstance()->HandleContractDependencyBroadcastMsg(msg, msgData);
        return 0;
    } catch (const std::exception& e) {
        ERRORLOG("Exception in HandleContractDependencyBroadcastMsg: {}", e.what());
        return -2;
    } catch (...) {
        ERRORLOG("Unknown exception in HandleContractDependencyBroadcastMsg");
        return -3;
    }
}

void EventManager::Subscribe(const std::string& event, Callback cb) {
    callbacks_[event].push_back(cb);
}

void EventManager::Publish(const std::string& event, const std::string& block_hash) {
    auto it = callbacks_.find(event);
    if (it != callbacks_.end()) {
        for (auto& cb : it->second) {
            cb(block_hash);
        }
    }
}

DependencyManager::DependencyManager() {
    MagicSingleton<EventManager>::GetInstance()->Subscribe("TransactionConfirmed", [this](const std::string& tx_hash) {
        ClearDependency(tx_hash);
    });
}

void DependencyManager::ClearDependency(const std::string& tx_hash) {
    std::unique_lock<std::shared_mutex> lock(dependency_tx_mutex_);

    // Log current dependencies before clearing
    DEBUGLOG("@@@@ Before clearing dependency for transaction hash: {}", tx_hash);
    for (auto it = dependency_tx_hashes_.begin(); it != dependency_tx_hashes_.end(); ) {
        auto& hashes = it->second;
        hashes.erase(std::remove(hashes.begin(), hashes.end(), tx_hash), hashes.end());
        DEBUGLOG("@@@@ After erasing transaction hash {} from address {}, {} hashes remain", tx_hash, it->first, hashes.size());
        if (hashes.empty()) {
            DEBUGLOG("@@@@ Erased empty dependency list for address: {}", it->first);
            it = dependency_tx_hashes_.erase(it);
        } else {
            ++it;
        }
    }
}
