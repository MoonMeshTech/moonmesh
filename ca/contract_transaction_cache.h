/**
 * *****************************************************************************
 * @file        contract_transaction_cache.h
 * @brief       Cache for failed contract transactions
 * @date        2024-01-01
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef CONTRACT_TRANSACTION_CACHE_H
#define CONTRACT_TRANSACTION_CACHE_H

#include <map>
#include <unistd.h>
#include <shared_mutex>
#include <chrono>

#include "utils/timer.hpp"
#include "ca/txhelper.h"

#include "ca/dispatchtx.h"
#include "proto/transaction.pb.h"
#include "proto/ca_protomsg.pb.h"

/**
 * @brief Cache for contract transactions that failed to send
 */
class ContractTransactionCache
{
public:
    ContractTransactionCache() { _StartTimer(); };
    ~ContractTransactionCache() = default;
    ContractTransactionCache(ContractTransactionCache &&) = delete;
    ContractTransactionCache(const ContractTransactionCache &) = delete;
    ContractTransactionCache &operator=(ContractTransactionCache &&) = delete;
    ContractTransactionCache &operator=(const ContractTransactionCache &) = delete;

    struct CachedTransaction {
        CTransaction tx;
        ContractTxMsgReq msg;
        uint64_t timestamp;
        
        CachedTransaction(const CTransaction& transaction, const ContractTxMsgReq& message)
            : tx(transaction), msg(message), timestamp(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()) {}
    };

public:
    /**
     * @brief Add a failed contract transaction to cache
     * 
     * @param tx: The contract transaction
     * @param msg: The contract transaction message
     * @return int: 0 on success, negative on error
     */
    int AddFailedTransaction(const CTransaction& tx, const ContractTxMsgReq& msg);
    
    /**
     * @brief Start the timer for periodic checking
     */
    void _StartTimer();

    /**
     * @brief Stop the timer
     */
    void StopTimer() { _timer.Cancel(); }

private:
    /**
     * @brief Check if cache is empty
     * 
     * @return true if empty, false otherwise
     */
    bool _IsEmpty();

    /**
     * @brief Periodically check cached transactions
     */
    void _CheckCachedTransactions();

    /**
     * @brief Send a cached transaction
     * 
     * @param cachedTx: The cached transaction to send
     * @return int: 0 on success, negative on error
     */
    int _SendCachedTransaction(const CachedTransaction& cachedTx);

private:
    mutable std::shared_mutex _cacheMutex;
    std::map<std::string, CachedTransaction> _cachedTransactions; // key: transaction hash
    CTimer _timer;

    static constexpr uint64_t FORCE_SEND_TIMEOUT = 15; // 30 seconds timeout for force sending
    static constexpr uint64_t CHECK_INTERVAL = 1 * 1000; // Check every 1 seconds
};

#endif // CONTRACT_TRANSACTION_CACHE_H