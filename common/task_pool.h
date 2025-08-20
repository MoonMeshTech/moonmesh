/**
 * *****************************************************************************
 * @file        task_pool.h
 * @brief       
 * @author  ()
 * @date        2023-09-28
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef TASK_POOL_HEADER_GUARD
#define TASK_POOL_HEADER_GUARD
#define BOOST_BIND_NO_PLACEHOLDERS

#include "./config.h"
#include "./global.h"

#include "../utils/magic_singleton.h"
#include "../common/protobuf_define.h"

#include <boost/threadpool.hpp>
using boost::threadpool::pool;

/**
 * @brief       
 * 
 */
void Gettid();

class TaskPool{
public:
    ~TaskPool() = default;
    TaskPool(TaskPool &&) = delete;
    TaskPool(const TaskPool &) = delete;
    TaskPool &operator=(TaskPool &&) = delete;
    TaskPool &operator=(const TaskPool &) = delete;
public:
    TaskPool()
    :caTaskPool_(global::CA_THREAD_NUMBER)
    ,netTaskPool(global::K_NET_THREAD_NUMBER)
    ,broadcastTaskPool(global::kBraodcastThreadNumber)
    ,txTaskPool(global::TX_THREAD_COUNT)
    ,syncTaskPool(global::SYNC_BLOCK_THREAD_NUMBER)
    ,saveBlockTaskPool(global::SAVE_BLOCK_THREAD_NUMBER)
    ,blockTaskPool(global::BLOCK_THREAD_COUNT)
    ,workTaskPool(global::WORK_THREAD_COUNT)
    {}
    
    /**
     * @brief       
     * 
     */
    void initializeTaskPool();
    
    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void commitCaTask(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        caTaskPool_.schedule(boost::bind(func, subMsg, data));
    }
    
    /**
     * @brief       
     * 
     */
    template<class T>
    void commitCaTask(T func)
    {
        caTaskPool_.schedule(func);
    }

    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void CommitNetworkTask(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        netTaskPool.schedule(boost::bind(func, subMsg, data));
    }
    /**
     * @brief       
     * 
     * @param       task 
     */
    void commitBroadcastRequest(std::function<void()> task) {
        broadcastTaskPool.schedule(task);
    }

    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void commitBroadcastRequest(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        broadcastTaskPool.schedule(boost::bind(func, subMsg, data));
    }

    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void CommitTransactionTask(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        txTaskPool.schedule(boost::bind(func, subMsg, data));
    }

    /**
     * @brief       
     * 
     */
    template<class T>
    void CommitTransactionTask(T func)
    {
        txTaskPool.schedule(func);
    }

    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void CommitSyncBlockJob(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        syncTaskPool.schedule(boost::bind(func, subMsg, data));
    }

    /**
     * @brief       
     * 
     */
    template<class T>
    void CommitSyncBlockJob(T func)
    {
        syncTaskPool.schedule(func);
    }

    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void CommitSaveBlockJob(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        saveBlockTaskPool.schedule(boost::bind(func, subMsg, data));
    }

    /**
     * @brief       
     * 
     * @param       func 
     * @param       subMsg 
     * @param       data 
     */
    void commit_block_task(ProtoCallBack func, MessagePtr subMsg, const MsgData &data)
    {
        blockTaskPool.schedule(boost::bind(func, subMsg, data));
    }

    /**
     * @brief       
     * 
     */
    template<class T>
    void commit_block_task(T func)
    {
        blockTaskPool.schedule(func);
    }

    template<class T>
    void commitWorkTask(T func)
    {
        workTaskPool.schedule(func);
    }

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t CaActive() const  {return caTaskPool_.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t CaPending() const {return caTaskPool_.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t NetActive() const {return netTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t NetPending() const  {return netTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t BroadcastActive() const{return broadcastTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t BroadcastPending() const{return broadcastTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t TxActive() const{return txTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t TxPending() const{return txTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t syncBlockActive() const{return syncTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t syncBlockPending() const{return syncTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t isSaveBlockActive() const{return saveBlockTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t saveBlockPending() const{return saveBlockTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t BlockActive() const{return blockTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t BlockPending() const{return blockTaskPool.pending();}

    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t WorkActive() const{return workTaskPool.active();}
    /**
     * @brief       
     * 
     * @return      size_t 
     */
    size_t WorkPending() const{return workTaskPool.pending();}

private:
    pool caTaskPool_;
    pool netTaskPool;
    pool broadcastTaskPool;

    pool txTaskPool;
    pool syncTaskPool;
    pool saveBlockTaskPool;

    pool blockTaskPool;
    pool workTaskPool;
};

#endif // TASK_POOL_HEADER_GUARD
