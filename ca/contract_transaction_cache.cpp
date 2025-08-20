#include "ca/contract_transaction_cache.h"
#include "ca/transaction.h"
#include "ca/txhelper.h"
#include "ca/dispatchtx.h"
#include "ca/block_monitor.h"
#include "ca/ca.h"
#include "db/db_api.h"
#include "utils/tmp_log.h"
#include "common/task_pool.h"
#include "include/logging.h"
#include "net/api.h"
#include "utils/magic_singleton.h"
#include "utils/time_util.h"
#include "utils/account_manager.h"

void ContractTransactionCache::_StartTimer()
{
    // Check cached transactions every 5 seconds
    _timer.AsyncLoop(CHECK_INTERVAL, [this](){
        if(!_IsEmpty())
        {
            _CheckCachedTransactions();
        }
    });
}

bool ContractTransactionCache::_IsEmpty()
{
    std::shared_lock<std::shared_mutex> lock(_cacheMutex);
    return _cachedTransactions.empty();
}

int ContractTransactionCache::AddFailedTransaction(const CTransaction& tx, const ContractTxMsgReq& msg)
{
    std::unique_lock<std::shared_mutex> lock(_cacheMutex);
    
    std::string txHash = tx.hash();
    if (_cachedTransactions.find(txHash) != _cachedTransactions.end())
    {
        DEBUGLOG("Transaction {} already in cache", txHash);
        return 0; // Already cached
    }
    
    _cachedTransactions.emplace(txHash, CachedTransaction(tx, msg));
    DEBUGLOG("Added failed contract transaction to cache: {}", txHash);
    return 0;
}

void ContractTransactionCache::_CheckCachedTransactions()
{
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::vector<std::string> transactionsToRemove;
    std::vector<CachedTransaction> transactionsToSend;
    
    {
        std::shared_lock<std::shared_mutex> lock(_cacheMutex);
        
        for (const auto& [txHash, cachedTx] : _cachedTransactions)
        {
            uint64_t timeInCache = currentTime - cachedTx.timestamp;
            
            // Force send transactions older than 30 seconds
            if (timeInCache >= FORCE_SEND_TIMEOUT)
            {
                DEBUGLOG("Force sending transaction {} after {} seconds", txHash, timeInCache);
                DEBUGLOG("TransactionConfirmed, tx hash: {}", txHash);
                MagicSingleton<EventManager>::GetInstance()->Publish("TransactionConfirmed", txHash);
                transactionsToSend.push_back(cachedTx);
                transactionsToRemove.push_back(txHash);
                continue;
            }
            
            // Check if transaction can be sent now
            if (MagicSingleton<DependencyManager>::GetInstance()->CanSendTransaction(cachedTx.msg))
            {
                DEBUGLOG("Transaction {} is now sendable", txHash);
                transactionsToSend.push_back(cachedTx);
                transactionsToRemove.push_back(txHash);
            }
        }
    }
    
    // Send transactions that are ready
    for (const auto& cachedTx : transactionsToSend)
    {
        int ret = _SendCachedTransaction(cachedTx);
        if (ret != 0)
        {
            ERRORLOG("Failed to send cached transaction {}, ret: {}", cachedTx.tx.hash(), ret);
        }
    }
    
    // Remove sent transactions from cache
    if (!transactionsToRemove.empty())
    {
        std::unique_lock<std::shared_mutex> lock(_cacheMutex);
        for (const auto& txHash : transactionsToRemove)
        {
            _cachedTransactions.erase(txHash);
        }
    }
}

int ContractTransactionCache::_SendCachedTransaction(const CachedTransaction& cachedTx)
{
    try
    {
        // Create a copy of the message to modify
        ContractTxMsgReq modifiedMsg = cachedTx.msg;
        
        // Get the transaction from the message
        CTransaction tx;
        if (!tx.ParseFromString(modifiedMsg.mutable_txmsgreq()->txmsginfo().tx()))
        {
            ERRORLOG("Failed to parse transaction from cached message: {}", cachedTx.tx.hash());
            return -2;
        }
        
        // Update transaction time before recalculating identity
        auto currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        tx.set_time(currentTime);
        
        // Recalculate transaction identity
        DEBUGLOG("txhash:{}, time:{}", tx.hash(), tx.time());
        std::string identity;
        int ret = get_contract_distribution_manager(tx.time(), identity);
        if(ret != 0)
        {
            ERRORLOG("get_contract_distribution_manager fail ret: {}", ret);
            return -3;
        }
        
        // Update transaction identity
        tx.set_identity(identity);
        

        tx.clear_hash();
        tx.clear_verifysign();

        uint64_t top = 0;
        int retNum = transactionDiscoveryHeight(top);
        if(retNum != 0){
            ERRORLOG("transactionDiscoveryHeight error {}", retNum);
            return -4;
        }
        
        // Update transaction hash after identity change
        std::string txHash = Getsha256hash(tx.SerializeAsString());
        tx.set_hash(txHash);
        
        // Update the message with modified transaction
        modifiedMsg.mutable_txmsgreq()->mutable_txmsginfo()->set_tx(tx.SerializeAsString());
        modifiedMsg.mutable_txmsgreq()->mutable_txmsginfo()->set_nodeheight(top);

        // Send the modified contract transaction message
        ret = contractTransactionRequest(std::make_shared<ContractTxMsgReq>(modifiedMsg), MsgData());
        MagicSingleton<BlockMonitor>::GetInstance()->dropshippingTxVec(tx.hash());

        if (ret != 0)
        {
            ERRORLOG("Failed to send cached contract transaction {}, ret: {}", cachedTx.tx.hash(), ret);
            return ret;
        }
        DEBUGLOG("Successfully sent cached contract transaction: {}", cachedTx.tx.hash());
        return 0;
    }
    catch (const std::exception& e)
    {
        ERRORLOG("Exception when sending cached transaction {}: {}", cachedTx.tx.hash(), e.what());
        return -5;
    }
}