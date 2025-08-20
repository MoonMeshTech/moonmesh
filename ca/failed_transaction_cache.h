/**
 * *****************************************************************************
 * @file        failed_transaction_cache.h
 * @brief       
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef TRAN_STORAGE
#define TRAN_STORAGE

#include <map>
#include <unistd.h>
#include <shared_mutex>

#include "utils/timer.hpp"
#include "ca/txhelper.h"
#include "ca/transaction_cache.h"
#include "proto/transaction.pb.h"
#include "proto/ca_protomsg.pb.h"

/**
 * @brief       
 */
class failedTransactionCache
{
public:
    failedTransactionCache(){ _StartTimer(); };
    ~failedTransactionCache() = default;
    failedTransactionCache(failedTransactionCache &&) = delete;
    failedTransactionCache(const failedTransactionCache &) = delete;
    failedTransactionCache &operator=(failedTransactionCache &&) = delete;
    failedTransactionCache &operator=(const failedTransactionCache &) = delete;

public:
    /**
     * @brief       
     * 
     * @param       height: 
     * @param       msg: 
     * @return      int 
     */
	int Add(uint64_t height,const TxMsgReq& msg);
    
    /**
     * @brief       
     * 
     */
	void _StartTimer();

    /**
     * @brief       
     * 
     */
    void StopTimer() { _timer.Cancel(); }
private:
    /**
     * @brief       
     * 
     * @return      true 
     * @return      false 
     */
    bool _IsEmpty();

    /**
     * @brief       
     * 
     */
	void _Check();
private:
    mutable std::shared_mutex txPendingMutex;
    std::map<uint64_t, std::vector<TxMsgReq>> _txPending;
	CTimer _timer;
};

#endif