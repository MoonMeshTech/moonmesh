/**
 * *****************************************************************************
 * @file        transaction_cache.h
 * @brief       
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef CA_TRANSACTION_CACHE_DATA
#define CA_TRANSACTION_CACHE_DATA

#include <map>
#include <list>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include <shared_mutex>

#include "proto/transaction.pb.h"
#include "proto/ca_protomsg.pb.h"
#include "proto/block.pb.h"
#include "utils/timer.hpp"
#include "ca/transaction_entity.h"
#include "net/msg_queue.h"
#include "mpt/trie.h"
#include "ca/packager_dispatch.h"

/**
 * @brief       Transaction cache class. After the transaction flow ends, add the transaction to this class. 
                Pack blocks every time a certain interval elapses or when the number of transactions reaches a certain number.
 */
class TransactionCache
{
    private:
        typedef std::map<uint64_t, std::list<TransactionEntity>>::iterator cacheIter;

    private:
        // Transaction container
        std::vector<TransactionEntity> _transactionCache;
        std::list<CTransaction> buildTxs;
        std::mutex buildTxsMutex;

        // The mutex of the transaction container
        std::mutex transactionCacheMutex;
        // Contract Transaction container
        std::vector<TransactionEntity> _contractCache;
        // The mutex of the Contract transaction container
        std::mutex contractCacheMutex;
        // Condition variables are used to package blocks
        std::condition_variable _blockBuilder;
        std::condition_variable contractPreBlockWaiter;
        // Timers are used for packing at specific time intervals
        CTimer _buildTimer;
        // Thread variables are used for packaging
        std::thread transactionCacheBuildThread;
        // Packing interval
        static const int BUILD_INTERVAL;
        // Transaction expiration interval
        static const time_t _kTxExpireInterval;
        // Packaging threshold
        static const int BUILD_THRESHOLD;
        // cache of contract info
        std::map<std::string, std::pair<nlohmann::json, uint64_t>> contractInfoCache;
        // The mutex of contractInfoCache
        std::shared_mutex contractInfoCacheMutex;

        std::string preContractBlockHash;
         std::map<std::string, std::pair<uint64_t, std::set<std::string> >> dirtyContractMap;
        std::shared_mutex dirtyContractMapMutex;
        std::atomic<bool> _threadRun = true;
    
        std::mutex _threadMutex;
    public:
        TransactionCache();
        ~TransactionCache() = default;

        /**
         * @brief
         * @param       height:
         * @return      void 
         */
        void setBlockCount(uint64_t height);

        /**
         * @brief
         * @return      uint64_t 
         */
        uint64_t getBlockCount();
        /**
         * @brief       Add a cache
         * 
         * @param       transaction: 
         * @param       sendTxMsg: 
         * @return      int 
         */
        int AddCache(CTransaction& transaction, const std::shared_ptr<TxMsgReq>& msg);

        /**
         * @brief       Start the packaging block building thread 
         * 
         * @return      true 
         * @return      false 
         */
        bool Process();

        /**
         * @brief       
         * 
         */
        void Stop();

        void addContractInfoToCache(const std::string& transactionHash, const nlohmann::json& txInfoJson, const uint64_t& txtime);

        /**
         * @brief       Get the contract json information
         * 
         * @param       transaction:
         * @param       txInfoJson:  
         * @return      int 
         */

        int get_contract_info_cache(const std::string& transactionHash, nlohmann::json& txInfoJson);
        /**
         * @brief       

         * @return      string 
         */
        std::string get_contract_prev_block_hash();
        /**
         * @brief
         * @param       blockHash:       
         */
        void contractBlockNotification(const std::string& blockHash);
        /**
         * @brief
         *
         * @param       contractAddress
         * @param       transactionHash
         * @param       contractPreHashCache_
         * @return      string
         */
        std::string get_and_update_contract_pre_hash(const std::string &contractAddress, const std::string &transactionHash,
                                   std::map<std::string, std::string> &contractPreHashCache_);
        /**
         * @brief
         *
         * @param       transactionHash
         * @param       dirtyContract
         */
        void set_dirty_contract_map(const std::string& transactionHash, const std::set<std::string>& dirtyContract);
        bool get_dirty_contract_map(const std::string& transactionHash, std::set<std::string>& dirtyContract);
 
        void removeExpiredFromDirtyContractMap();

        /**
         * @brief
         *
         * @param       addr
         * @param       transactionHeight
         * @param       time
         * @return      true
         * @return      false
         */
        static bool hasContractPackingPermission(const std::string& addr, uint64_t transactionHeight, uint64_t time);
        
        std::pair<int, std::string> executeContracts(const std::map<std::string, CTransaction> &dependentContractTxMap_,
                                                      int64_t blockNumber);

        bool removeContractsCacheRequest(const std::map<std::string, CTransaction>& contractTxs);

        bool removeContractInfoCacheRequest(const std::map<std::string, CTransaction>& contractTxs);

        void ProcessContract(int64_t topTransactionHeight_);

        int handleContractPackagerMessage(const std::shared_ptr<ContractPackagerMsg> &msg, const MsgData &msgdata);

    private:

        void addBuildTransaction(const CTransaction &transaction);
        const std::list<CTransaction>& getBuildTransactions();
        void clearBuiltTransactions();
        
        int get_build_block_height(std::vector<TransactionEntity>& txcache);
        /**
         * @brief       Threading functions
         */
        void processTransactionCacheFunc();
        /**
         * @brief
         * @param       transaction
         * @param       contractPreHashCache_
         * @param       contractDataStorage
         * @return      int
         */
        int addContractInfoCache(const CTransaction &transaction,
                                  std::map<std::string, std::string> &contractPreHashCache_,
                                  contractDataContainer *contractDataStorage, int64_t blockNumber);
        /**
         * @brief
         *
         * @param       transactionHash
         * @param       calledContract
         * @return      true
         * @return      false
         */
        bool verifyDirtyContractFlag(const std::string &transactionHash, const std::vector<std::string> &calledContract);
        int fetchContractTxPreHash(const std::list<CTransaction>& txs, std::list<std::pair<std::string, std::string>>& contractTxPrevHashList);

    	int ContractTransaction(const std::vector<TxMsgReq> &transmitMessageRequests);
};
/**
 * @brief
 *
 * @param       num
 * @param       nodeSelfHeight
 * @param       pledgeAddr
 * @param       node_ids_to_send
 * @return      int
*/
static int get_contract_prehash_find_node(uint32_t num, uint64_t nodeSelfHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &node_ids_to_send);
/**
 * @brief
 *
 * @param       msg
 * @param       msgData
 * @return      int
*/  
int handleContractPackagerMessage(const std::shared_ptr<ContractPackagerMsg> &msg, const MsgData &msgData);


int handleSeekContractPreHashRequest(const std::shared_ptr<newSeekContractPreHashReq> &msg, const MsgData &msgdata);
int handleSeekContractPreHashAcknowledgement(const std::shared_ptr<newSeekContractPreHashAck> &msg, const MsgData &msgdata);
int newSeekContractPreHash(const std::list<std::pair<std::string, std::string>> &contractTxPrevHashList);

#endif
