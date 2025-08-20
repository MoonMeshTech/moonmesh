#include "ca/failed_transaction_cache.h" 
#include "db/db_api.h"
#include "transaction.h"

#include "utils/tmp_log.h"
#include "common/task_pool.h"
#include "include/logging.h"

void failedTransactionCache::_StartTimer()
{
	//Notifications for inspections at regular intervals
	_timer.AsyncLoop(2 * 1000, [this](){
        if(!_IsEmpty())
        {
            _Check();
        }
	});
}

bool failedTransactionCache::_IsEmpty()
{
    std::shared_lock<std::shared_mutex> lock(txPendingMutex);
    if(_txPending.empty())
    {
        return true;
    }
    else
    {
        return false;
    }
}

void failedTransactionCache::_Check()
{
    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return;
    }

    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::vector<decltype(_txPending.begin())> deleteTransactionPendingStatus;

    {
        std::unique_lock<std::shared_mutex> lock(txPendingMutex);
        for (auto iter = _txPending.begin(); iter != _txPending.end(); /* no increment */)
        {
            auto txHeight = iter->first;
            uint64_t nodeHeight = 0;
            for (auto &node : nodelist)
            {
                if (node.height >= txHeight)
                {
                    nodeHeight++;
                }
            }

            if (nodeHeight < nodelist.size() * 0.75 || top < txHeight)
            {
                ++iter;
                continue;
            }

            bool shouldDelete = false;
            std::vector<std::pair<decltype(iter), decltype(iter->second.begin())>> tasks;
            
            for (auto txIter = iter->second.begin(); txIter != iter->second.end(); ++txIter)
            {
                if (top > txHeight + 3)
                {
                    shouldDelete = true;
                    break;
                }
                
                auto transmitMessageCopy = std::make_shared<TxMsgReq>(*txIter);
                tasks.emplace_back(iter, txIter);
                MagicSingleton<TaskPool>::GetInstance()->CommitTransactionTask(
                    [transmitMessageCopy]() {
                        CTransaction tx;
                        int ret = handleTransaction(transmitMessageCopy, tx);

                        tx.clear_hash();
                        tx.clear_verifysign();
                        tx.set_hash(Getsha256hash(tx.SerializeAsString()));
                        if (ret != 0)
                        {
                            DEBUGLOG("TTT tx verify <fail!!!> txhash:{}, ret:{}", tx.hash(), ret);
                            return;
                        }

                        DEBUGLOG("TTT tx verify <success> txhash:{}", tx.hash());
                    });
            }

            if (shouldDelete)
            {
                deleteTransactionPendingStatus.push_back(iter);
                ++iter;
            }
            else
            {
                for (const auto &task : tasks)
                {
                    task.first->second.erase(task.second);
                }
                ++iter;
            }
        }
    }

    {
        std::unique_lock<std::shared_mutex> lock(txPendingMutex);
        for (auto &iter : deleteTransactionPendingStatus)
        {
            _txPending.erase(iter);
        }
    }
}
	
int failedTransactionCache::Add(uint64_t height, const TxMsgReq& msg)
{
    DEBUGLOG("TTT NodelistHeight discontent, repeat commit tx ,NodeHeight:{}, txUtxoHeight:{}",height, msg.txmsginfo().txutxoheight());
    std::unique_lock<std::shared_mutex> lock(txPendingMutex);
    _txPending[height].push_back(msg);
	return 0;
}
