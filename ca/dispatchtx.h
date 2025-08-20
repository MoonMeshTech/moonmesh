#ifndef DISPATCHER_HEADER_PROTECTION
#define DISPATCHER_HEADER_PROTECTION
#include <mutex>
#include <thread>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>

#include "ca/global.h"
#include "ca/interface.h"
#include "ca/transaction.h"
#include "ca/transaction_cache.h"

class DependencyManager;
class MessageDispatcher;
class TimerProcessor;

class ContractDispatcher {
public:
    ContractDispatcher();
    ~ContractDispatcher();

    void AddContractInfo(const std::string& contract_hash, const std::vector<std::string>& dependent_contracts);
    void AddContractMessageRequest(const std::string& contract_hash, const ContractTxMsgReq& msg);
    void Process();
    void SetTimeValue(const uint64_t& new_value);
    
    struct MsgInfo {
        std::vector<TxMsgReq> tx_msg_req;
    };

private:
    static constexpr uint64_t kContractWaitingTime = 3 * 1000000ULL;

    std::mutex time_mutex_;
    std::condition_variable cv_;
    bool is_first_ = false;
    uint64_t time_value_ = 0;
};

// EventManager for event-driven management
class EventManager {
public:
    using Callback = std::function<void(const std::string& block_hash)>;
    void Subscribe(const std::string& event, Callback cb);
    void Publish(const std::string& event, const std::string& block_hash);
private:
    std::map<std::string, std::vector<Callback>> callbacks_;
};

class DependencyManager {
public:
    DependencyManager();
    void AddContractInfo(const std::string& contract_hash, const std::vector<std::string>& dependent_addresses);
    void AddMessageRequest(const std::string& contract_hash, const ContractTxMsgReq& msg);
    std::vector<std::vector<TxMsgReq>> GetDependentData();
    std::vector<std::vector<TxMsgReq>> GroupData(const std::vector<std::vector<TxMsgReq>>& tx_vectors);
    bool HasDuplicate(const std::vector<std::string>& vec1, const std::vector<std::string>& vec2);
    void ClearCaches();

    std::map<std::string, std::vector<std::string>> GetAllDependenciesWithTxHashes() const;

    std::set<std::string> GetAllUniqueDependencies() const;

    int HandleContractDependencyBroadcastMsg(const std::shared_ptr<ContractDependencyBroadcastMsg>& msg, const MsgData &msgData);
    void StoreBroadcastDependencies(const ContractDependencyBroadcastMsg& msg);
    bool CanSendTransaction(const ContractTxMsgReq& msg) const;
    void ClearDependency(const std::string& tx_hash);
    void BroadcastAddedDependency(const std::string& contract_hash, const std::vector<std::string>& dependent_addresses);
private:
    //The data received from the initiator is stored
    std::unordered_map<std::string/*txHash*/, std::vector<std::string>/*Contract dependency address*/> contract_dep_cache_; 
    std::unordered_map<std::string, TxMsgReq> contract_msg_cache_;
    mutable std::mutex dep_mutex_;
    mutable std::mutex msg_mutex_;

    std::unordered_map<std::string, std::vector<std::string>> dependency_tx_hashes_;
    mutable std::shared_mutex dependency_tx_mutex_; 
};

class MessageDispatcher {
public:
    int DistributionContractTransactionRequest(std::multimap<std::string, ContractDispatcher::MsgInfo>& distribution,
                                               const std::vector<std::vector<TxMsgReq>>& grouped_data);
    int SendTransactionInfoRequest(const std::string& packager, std::vector<TxMsgReq>& tx_msgs);

    void BroadcastDependencies();
};


// TimerProcessor class declaration
class TimerProcessor {
public:
    TimerProcessor() = default;
    ~TimerProcessor() = default;
    void Start();
    void Stop();
    void SetTimeValue(uint64_t value);
private:
    void ProcessingFunc();
    std::thread timer_thread_;
    std::mutex timer_mutex_;
    std::condition_variable timer_cv_;
    bool running_ = false;
    uint64_t time_value_ = 0;
    static constexpr uint64_t kContractWaitingTime = 3 * 1000000ULL;
};

int HandleContractDependencyBroadcastMsg(const std::shared_ptr<ContractDependencyBroadcastMsg> &msg, const MsgData &msgData);
#endif