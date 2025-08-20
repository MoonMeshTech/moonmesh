/**
 * *****************************************************************************
 * @file        block_helper.h
 * @brief       
 * @date        2023-09-25
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef CA_BLOCK_HELPER_HEADER_GUARD
#define CA_BLOCK_HELPER_HEADER_GUARD

#include <map>
#include <stack>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <atomic>
#include <thread>
#include <cstdint>
#include <condition_variable>

#include "ca/block_compare.h"
#include "global.h"

namespace compator
{
    struct blockTimeAscending
    {
        bool operator()(const CBlock &a, const CBlock &b) const
        {
            if(a.height() == b.height()) return a.time() < b.time();
            else if(a.height() < b.height()) return true;
            return false;
        }
    };
}
struct MissingBlock
{
	std::string _hash;
	uint64_t _time;
    std::shared_ptr<bool> txOrBlockVariant; //  0 is block hash  1 is utxo
    std::shared_ptr<uint64_t> _triggerCount;
    
    MissingBlock(const std::string& hash, const uint64_t& time, const bool& txOrBlock)
    {
        _hash = hash;
        _time = time;
        txOrBlockVariant = std::make_shared<bool>(txOrBlock);
        _triggerCount = std::make_shared<uint64_t>(0);
    }
	bool operator<(const struct MissingBlock & right)const   // Overload<operator
	{
		if (this->_hash == right._hash)
		{
			return false;
		}
		else
		{
			return _time < right._time; // Small top reactor
		}
	}
};

enum CONTRACT_PRE_HASH_STATUS {
    Normal,
    DatabaseBlockException,
    MemoryBlockException,
    Waiting,
    Err
};

enum class DOUBLE_SPEND_TYPE
{
    repeat_double_spend,
    isDoubleSpend,
    previousDoubleSpend,
    isInvalidDoubleSpend,
    err
};

class BlockManager {
public:
    using Timestamp = std::chrono::steady_clock::time_point;

    bool addBlock(const std::string& blockHash) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto result = blocks_.emplace(blockHash, now);
        return result.second; // true if the block was added, false if it already existed
    }

    bool hasBlock(const std::string& blockHash) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return blocks_.find(blockHash) != blocks_.end();
    }

    void remove_expired_blocks_(std::chrono::seconds timeout) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        for (auto it = blocks_.begin(); it != blocks_.end(); ) {
            if (now - it->second > timeout) {
                it = blocks_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::unordered_map<std::string, Timestamp> blocks_;
    std::shared_mutex mutex_;
};

/**
 * @brief       
 */
class BlockHelper
{
    public:
        BlockHelper();

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       blockStatus: 
         * @return      int 
         */
        int verifyFlowedBlockStatus(const CBlock& block, BlockStatus* blockStatus = nullptr,BlockMsg *msg = nullptr);

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       saveType: 
         * @param       computeMeanValue: 
         * @return      int 
         */
        int SaveBlock(const CBlock& block, global::ca::SaveType saveType, global::ca::blockMean computeMeanValue);

        /**
         * @brief       Set the Missing Prehash object
         */
        void set_missing_prehash();
        
        /**
         * @brief       
         */
        void resetMissingPrehash();

        /**
         * @brief       
         * 
         * @param       utxo: 
         */
        void pushMissUtxo(const std::string& utxo);  

        /**
         * @brief       
         */
        void popMissUtxo();

        /**
         * @brief       
         * 
         * @param       chainHeight: 
         * @return      true 
         * @return      false 
         */
        static bool getChainHeight(uint64_t& chainHeight);

        /**
         * @brief       
         */
        void Process(); 

        /**
         * @brief       
         */ 
        void SeekBlockThreadObject();

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       status: 
         */
        void AddBroadcastBlock(const CBlock& block, bool status = false);

        /**
         * @brief       
         * 
         * @param       syncBlockData: 
         * @param       type: 
         */
        void AddSyncBlock(const std::map<uint64_t, std::set<CBlock, BlockComparator>> &syncBlockData, global::ca::SaveType type);
        
        /**
         * @brief       
         * 
         * @param       syncBlockData: 
         * @param       type: 
         */
        void AddFastSyncBlock(const std::map<uint64_t, std::set<CBlock, BlockComparator>> &syncBlockData, global::ca::SaveType type);
        
        /**
         * @brief       
         * 
         * @param       syncBlockData: 
         */
        void rollback_block_(const std::map<uint64_t, std::set<CBlock, BlockComparator>> &syncBlockData);
        
        /**
         * @brief       
         * 
         * @param       block: 
         */
        void AddMissingBlock(const CBlock& block);
        
        /**
         * @brief       
         * 
         * @param       seekBlocks: 
         */
        void add_seek_block(std::vector<std::pair<CBlock,std::string>>& seekBlocks);
        
        /**
         * @brief       
         * 
         * @param       utxo: 
         * @param       shelfHeight: 
         * @param       blockHash: 
         * @return      int 
         */
        int rollbackPreviousBlocks(const std::string utxo, uint64_t shelfHeight, const std::string blockHash);
        
        /**
         * @brief       
         * 
         * @return      true 
         * @return      false 
         */
        bool getWhetherRunSendBlockByUtxoRequest() { return shouldSendBlockByUtxoReq; };
        
        /**
         * @brief       
         * 
         * @param       flag: 
         */
        void setWhetherRunSendBlockByUtxoRequest(bool flag) {shouldSendBlockByUtxoReq = flag;};

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       tx: 
         * @param       missingUtxo: 
         * @return      std::pair<DOUBLE_SPEND_TYPE, CBlock> 
         */
        std::pair<DOUBLE_SPEND_TYPE, CBlock> dealDoubleSpend(const CBlock& block, const CTransaction& tx, const std::string& missingUtxo);
        
        /**
         * @brief       
         */
        void RollbackTest();

        /**
         * @brief       
         * 
         * @param       oldBlock: 
         * @param       newBlock: 
         */
        void transactionStatusMessage(const CBlock &oldBlock, const CBlock &newBlock);
        
        /**
         * @brief       
         * 
         * @param       double_spend_data: 
         * @param       block: 
         */
        void checkDoubleBlooming(const std::pair<DOUBLE_SPEND_TYPE, CBlock>& double_spend_data, const CBlock &block);
        
        /**
         * @brief       
         */
        void stopSaveBlock() { _stopBlocking = false; }

    private:

        /**
         * @brief       
         * 
         * @param       contractAddr: 
         * @param       memContractPreHashResponse: 
         * @param       blockTime: 
         * @param       dbBlockHash: 
         */
        CONTRACT_PRE_HASH_STATUS contractPreHashStatus(const std::string& contractAddr, const std::string& memContractPreHashResponse, const uint64_t blockTime, std::string& dbBlockHash);
        /**
         * @brief       
         * 
         * @param       block: 
         * @param       contractTxPreviousHash: 
         */
        int check_contract_block(const CBlock& block, std::string& contractTxPreviousHash);

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       contractTxPreviousHash: 
         */
        int contractBlockCacheChecker(const CBlock& block, const std::string& contractAddr, const std::string& contractTxPreviousHash);

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       nodeSelfHeight: 
         */
        void AddPendingBlock(const CBlock& block);

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       contractTxPreviousHash: 
         */
        void add_contract_block(const CBlock& block, const std::string& contractAddr, const std::string& contractTxPreviousHash);

       /**
        * @brief       
        * 
        * @param       where: 
        * @return      std::string 
        */
        friend std::string PrintCache(int where);

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       ownblockHeight: 
         * @return      true 
         * @return      false 
         */
        bool VerifyHeight(const CBlock& block, uint64_t ownblockHeight);

        /**
         * @brief       
         */
        bool getMissingBlock();

        /**
         * @brief       
         * 
         * @param       block: 
         */
        void processPostMembershipCancellation(const CBlock &block);

        /**
         * @brief       
         * 
         * @param       block: 
         */
        void processPostTransaction(const CBlock &block);

        /**
         * @brief       
         * 
         * @param       block: 
         */
        void PostSaveProcess(const CBlock &block);

        /**
         * @brief       
         * 
         * @param       block: 
         * @param       saveType: 
         * @param       computeMeanValue: 
         * @return      int 
         */
        int PreSaveProcess(const CBlock& block, global::ca::SaveType saveType, global::ca::blockMean computeMeanValue);
        
        /**
         * @brief       
         * 
         * @return      int 
         */
        int RollbackBlocks();

        int rollbackContractBlock();

        int RollbackBlock(const std::string& blockHash, uint64_t height);

        std::mutex helperMutex;
        std::mutex helperMutexLow1;
        std::atomic<bool> missing_prehash;
        std::mutex missingUtxosMutex;
        std::stack<std::string> missingUtxos;

        std::set<CBlock, compator::blockTimeAscending> broadcastBlocks; //Polling of blocks to be added after broadcasting
        std::set<CBlock, compator::blockTimeAscending> _syncBlocks; // Synchronized block polling
        std::set<CBlock, compator::blockTimeAscending> fastSyncBlocks; //Quickly synchronize the block polling to be added
        std::map<uint64_t, std::set<CBlock, BlockComparator>> rollbackBlocks; // Polling of blocks to be rolled back
        std::map<uint64_t, std::multimap<std::string, CBlock>> _pendingBlocks; // Because there is no block trigger waiting to be added in the previous hash
        std::map<std::string, std::pair<uint64_t, CBlock>> hashPendingBlocks; //  Polling for blocks found by the find block protocol and blocks not found by utxo
        std::vector<CBlock> utxoMissingBlocks; //Poll the block found by finding the protocol of utxo
        std::mutex seekMutex;
        std::set<MissingBlock> _missingBlocks; // Wait for the hash polling to trigger the block finding protocol
        std::map<std::string, CBlock> doubleSpendBlocks;
        std::map<std::string, std::pair<bool, uint64_t>> _duplicateChecker;
        std::unordered_map<std::string, std::pair<std::string, CBlock>> contractBlocks;
        const static int MAX_MISSING_BLOCK_SIZE = 10;
        const static int MAX_MISSING_UXTO_SIZE = 10;
        const static int SYNC_SAVE_FAIL_TOLERANCE = 2;
        std::atomic<bool> _stopBlocking = true;
        std::atomic<bool> shouldSendBlockByUtxoReq = true;

        uint64_t commitCostAfter = 0;
        uint64_t committedPostCount = 0;
};

/**
 * @brief       Get the Utxo Find Node object
 * 
 * @param       num: 
 * @param       nodeSelfHeight: 
 * @param       pledgeAddr: 
 * @param       node_ids_to_send: 
 * @return      int 
 */
int getUtxoAndFindNode(uint32_t num, uint64_t nodeSelfHeight, const std::vector<std::string> &pledgeAddr,
                    std::vector<std::string> &node_ids_to_send);

/**
 * @brief       
 * 
 * @param       utxo: 
 * @return      int 
 */
int sendBlockByUtxoRequest(const std::string &utxo);




/**
 * @brief       
 * 
 * @param       missingHashs: 
 * @return      int 
 */
int sendBlockByHashRequest(const std::map<std::string, bool> &missingHashs);

/**
 * @brief       
 * 
 * @param       blockHashToSeek: 
 * @param       contractBlockString: 
 * @return      int 
 */
int seekBlockByContractPreHashRequest(const std::string &blockHashToSeek, std::string& contractBlockString);




#endif