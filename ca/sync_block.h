/**
 * *****************************************************************************
 * @file        sync_block.h
 * @brief       Synchronize data blocks to other Delegating staking nodes
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef CA_SYNC_BLOCK_HEADER
#define CA_SYNC_BLOCK_HEADER

#include <map>
#include <cstdint>

#include "net/msg_queue.h"
#include "proto/sync_block.pb.h"
#include "utils/timer.hpp"
#include "ca/check_blocks.h"
#include "ca/block_compare.h"

struct FastSynchronizationHelper
{
    uint32_t hits;
    std::set<std::string> ids;
    uint64_t height;
};

/**
 * @brief       
 * sync block
 */
class SyncBlock
{
friend class CheckBlocks;
public:

    SyncBlock() = default;
    ~SyncBlock() = default;
    SyncBlock(SyncBlock &&) = delete;
    SyncBlock(const SyncBlock &) = delete;
    SyncBlock &operator=(SyncBlock &&) = delete;
    SyncBlock &operator=(const SyncBlock &) = delete;

    /**
     * @brief       
     * Start synchronization thread
     */
    void ThreadStart();

    /**
     * @brief    Enable or disable synchronization threads   
     * 
     * @param       start: Enable or disable
     */
    void ThreadStart(bool start);

    /**
     * @brief    disable synchronization threads
     * 
     */
    void ThreadStop();

    /**
     * @brief       Get the Sync Node list
     * 
     * @param       num: sync node number
     * @param       chainHeight: Current whole network chain height
     * @param       pledgeAddr:  Delegating pledge list
     * @param       node_ids_to_send: sync node ids
     * @return      int : Return 0 on success
     */
    static int getSyncNodeSimplified(uint32_t num, uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &node_ids_to_send);
    /**
     * @brief       Set the Fast Sync object
     * 
     * @param       syncStartHeight: sync height
     */
    static void SetFastSync(uint64_t syncStartHeight);

    /**
     * @brief       Set the new Sync object
     * 
     * @param       syncStartHeight: sync begin height
     */
    static void set_new_sync_height(uint64_t height);

    /**
     * @brief   check byzantine    
     * 
     * @param       receiveCount: receive num
     * @param       hitCount:   hit num
     * @return      true    check success
     * @return      false   check fail
     */
    static bool checkByzantineFault(int receiveCount, int hitCount);

    /**
     * @brief       Calculate height sum hash    
     * 
     * @param       heightBlocks: block height
     * @param       hash: block hash
     * @return      true    hash = sum hash
     * @return      false   hash = nullptr
     */
    static bool sumHeightsHash(const std::map<uint64_t, std::vector<std::string>>& heightBlocks, std::string &hash);

    /**
     * @brief       get sync node
     * 
     * @param       num: sync send num
     * @param       heightBaseline: filter height
     * @param       discardComparisonFunc: comparators
     * @param       compareReserve: comparators
     * @param       pledgeAddr: Delegating pledge list
     * @param       node_ids_to_send: send node list ids
     * @return      int return 0 success
     */
    static int get_sync_node_basic(uint32_t num, uint64_t heightBaseline, const std::function<bool(uint64_t, uint64_t)>& discardComparisonFunc, const std::function<bool(uint64_t, uint64_t)>& compareReserve, const std::vector<std::string> &pledgeAddr,
                         std::vector<std::string> &node_ids_to_send);

    static int fetchRollbackBlock(const std::vector<std::string> &node_ids_to_send, uint64_t syncInitHeight, uint64_t endSyncHeight_, uint64_t nodeSelfHeight, std::vector<CBlock> &retBlocks);

private:
    /**
     * @brief       fast sync 
     * 
     * @param       pledgeAddr: Delegating pledge list
     * @param       chainHeight: Current chain height
     * @param       syncInitHeight: fast sync start height
     * @param       endSyncHeight_: fast sync end height
     * @return      true    fast sync success
     * @return      false   fast sync fail
     */
    static bool runFastSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t syncInitHeight, uint64_t endSyncHeight_, uint32_t syncNodeCount);
    
    /**
     * @brief       new sync
     * 
     * @param       pledgeAddr: Delegating pledge list
     * @param       chainHeight: Current chain height
     * @param       nodeSelfHeight: self node height
     * @param       syncInitHeight: new sync start height
     * @param       endSyncHeight_: new sync end height
     * @param       newSendNum: new sync send node num
     * @return      int  return 0 success 
     */
    static int runNewSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t nodeSelfHeight, uint64_t syncInitHeight, uint64_t endSyncHeight_, uint32_t syncNodeCount);
    
    /**
     * @brief       from zero sync 
     * 
     * @param       pledgeAddr: Delegating pledge list
     * @param       chainHeight: Current chain height
     * @param       nodeSelfHeight: self node height
     * @return      int  return 0 success 
     */
    int runFromZeroSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t nodeSelfHeight, uint32_t syncNodeCount);
    /**********************************************************************************************************************************/
    
    /**
     * @brief       fast sync get block hash
     * 
     * @param       node_ids_to_send: send node list ids
     * @param       syncInitHeight: get block start height
     * @param       endSyncHeight_: get block  end height
     * @param       requestHashs: need request block hash
     * @param       node_ids_returned: list of successfully returned nodes
     * @param       chainHeight: current chain height
     * @return      true    success
     * @return      false   fail
     */
    static bool get_fast_sync_sum_hash_node(const std::vector<std::string> &node_ids_to_send, uint64_t syncInitHeight, uint64_t endSyncHeight_,
                                    std::vector<FastSyncBlockHashs> &requestHashs, std::vector<std::string> &node_ids_returned, uint64_t chainHeight);
    
    /**
     * @brief       fast sync get block
     * 
     * @param       sendNodeId: send node list ids
     * @param       requestHashs: need request block hash
     * @param       chainHeight: current chain height
     * @return      true    success
     * @return      false   fail
     */
    static bool get_fast_sync_block_data(const std::string &sendNodeId, const std::vector<FastSyncBlockHashs> &requestHashs, uint64_t chainHeight);

    /**********************************************************************************************************************************/
    
    /**
     * @brief       new sync get block sum hash
     * 
     * @param       pledge_addr_size: Delegating pledge list
     * @param       node_ids_to_send: send node list ids
     * @param       syncInitHeight: get block sum hash start height
     * @param       endSyncHeight_: get block sum hash end height
     * @param       shouldSyncHeights: need sync height
     * @param       node_ids_returned: list of successfully returned nodes
     * @param       chainHeight: current chain height
     * @param       syncSendNum: new sync send node num
     * @return      int     return 0 success 
     */
    static int get_sync_sum_hash_node(uint64_t pledge_addr_size, const std::vector<std::string> &node_ids_to_send, uint64_t syncInitHeight, uint64_t endSyncHeight_,
                            std::map<uint64_t, uint64_t> &shouldSyncHeights, std::vector<std::string> &node_ids_returned, uint64_t &chainHeight, uint64_t syncSendNum);
    /**
     * @brief       synchronize data to the most forks in the entire network
     * 
     * @param       node_ids_to_send: send node list ids
     * @param       syncInitHeight:  get block hash start height
     * @param       endSyncHeight_: get block hash end height
     * @param       nodeSelfHeight: self node height
     * @param       chainHeight: current chain height
     * @param       syncSendNum: new sync send node num
     * @return      int return 0 success 
     */
    static int get_sync_block_by_sum_hash_node(std::vector<std::string> &node_ids_to_send, uint64_t syncInitHeight, uint64_t endSyncHeight_, uint64_t nodeSelfHeight, uint64_t chainHeight, uint64_t syncSendNum);
    
    /**
     * @brief       new sync get block hash
     * 
     * @param       node_ids_to_send: send node list ids
     * @param       syncInitHeight: get block hash start height
     * @param       endSyncHeight_: get block hash end height
     * @param       nodeSelfHeight: self node height
     * @param       chainHeight: current chain height
     * @param       node_ids_returned: list of successfully returned nodes
     * @param       reqHashes: need sync block hash
     * @param       syncSendNum: new sync send node num
     * @return      int int return 0 success 
     */
    static int get_sync_block_hash_node(const std::vector<std::string> &node_ids_to_send, uint64_t syncInitHeight, uint64_t endSyncHeight_, uint64_t nodeSelfHeight, uint64_t chainHeight,
                            std::vector<std::string> &node_ids_returned, std::vector<std::string> &reqHashes, uint64_t syncSendNum);
    
    static int getSyncBlockHashVerification(const std::vector<std::string> &node_ids_to_send, uint64_t syncInitHeight, uint64_t endSyncHeight_, uint64_t nodeSelfHeight, uint64_t chainHeight,
                            std::vector<std::string> &node_ids_returned, std::vector<std::string> &reqHashes, uint64_t syncSendNum);
    /**
     * @brief       new sync get block 
     * 
     * @param       node_ids_to_send: send node list ids
     * @param       reqHashes: need sync block hash
     * @param       chainHeight: current chain height
     * @return      int return 0 success
     */
    static int getSyncBlockData(std::vector<std::string> &node_ids_to_send, const std::vector<std::string> &reqHashes, uint64_t chainHeight);
    /**********************************************************************************************************************************/
    
    /**
     * @brief       from zero sync get sum hash
     * 
     * @param       node_ids_to_send: send node list ids
     * @param       heightsForSending: get block sum hash start height
     * @param       nodeSelfHeight: get block sum hash end height
     * @param       node_ids_returned: list of successfully returned nodes
     * @param       sumHashs: need sync block sum hash
     * @return      int return 0 success
     */
    static int get_from_zero_sync_sum_hash_node(const std::vector<std::string> &node_ids_to_send, const std::vector<uint64_t>& heightsForSending, uint64_t nodeSelfHeight, std::set<std::string> &node_ids_returned, std::map<uint64_t, std::string>& sumHashs);
    
    /**
     * @brief       from zero sync get block
     * 
     * @param       sumHashes: need sync block sum hash
     * @param       heightsForSending: send node list ids
     * @param       set_send_node_ids: send node list ids
     * @param       nodeSelfHeight: get block sum hash end height
     * @return      int return 0 success
     */
    int getSyncBlockDataFromZero(const std::map<uint64_t, std::string>& sumHashes, std::vector<uint64_t> &heightsForSending, std::set<std::string> &set_send_node_ids, uint64_t nodeSelfHeight);
    /**********************************************************************************************************************************/
    
    /**
     * @brief       get sync node
     * 
     * @param       num: sync send num
     * @param       chainHeight: current chain height
     * @param       pledgeAddr: Delegating pledge list
     * @param       node_ids_to_send: send node list ids
     * @return      int return 0 success
     */
    static int getSyncNode(uint32_t num, uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                    std::vector<std::string> &node_ids_to_send);

    /**
     * @brief       add blocks that need to be rolled back
     * 
     * @param       block: need to be rolled back
     * @param       syncBlockData: rolled back block set
     */
    static void addBlockToMap(const CBlock &block, std::map<uint64_t, std::set<CBlock, BlockComparator>> &syncBlockData);
    
    /**
     * @brief       need byzantine adjustment
     * 
     * @param       chainHeight: current chain height
     * @param       pledgeAddr: Delegating pledge list
     * @param       selectedAddr: selected addr
     * @return      true    success
     * @return      false   fail
     */
    static bool needByzantineAdjustment(uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                                 std::vector<std::string> &selectedAddr);

    /**
     * @brief       Check Requirement And Filter Qualifying Nodes
     * 
     * @param       chainHeight: current chain height
     * @param       pledgeAddr: Delegating pledge list
     * @param       nodes: node
     * @param       stakeQualifyingNodes: qualify stake nodes
     * @return      true    success
     * @return      false   fail
     */
    static bool checkRequirementAndFilterQualifyingNodes(uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                const std::vector<Node> &nodes,std::vector<std::string> &stakeQualifyingNodes);

    /**
     * @brief       Get Sync Node Sumhash Info
     * 
     * @param       nodes: node 
     * @param       stakeQualifyingNodes: qualify stake nodes
     * @param       sumHash: sum hash
     * @return      true    success
     * @return      false   fail
     */
    static bool get_sync_node_hash_info(const std::vector<Node> &nodes, const std::vector<std::string> &stakeQualifyingNodes,
                                       std::map<std::string, std::pair<uint64_t, std::vector<std::string>>> sumHash);

    /**
     * @brief        Get selected Addr
     * 
     * @param       sumHash: sum hash
     * @param       selectedAddr: selected addr
     * @return      true    success
     * @return      false   fail
     */
    static bool _GetSelectedAddr(std::map<std::string, std::pair<uint64_t, std::vector<std::string>>> &sumHash,
                                std::vector<std::string> &selectedAddr);
    std::thread _syncThread; 
    std::atomic<bool> syncThreadRunning;
    std::mutex syncThreadRunningMutex;
    std::condition_variable syncThreadRunningCondition;

    uint32_t fastSyncHeightCount{};
    bool _syncing{} ;

    std::map<uint64_t, std::map<uint64_t, std::set<CBlock, BlockComparator>>> syncFromZeroCache;
    std::vector<uint64_t> syncFromZeroReserveHeights;
    std::mutex cacheSyncMutex;
    const int kSynchronizationBoundary = 200;

};

void fastSyncGetHashRequest(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void fastSyncHashAck(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void sendFastSyncGetBlockRequest(const std::string &nodeId, const std::string &msgId, const std::vector<FastSyncBlockHashs> &requestHashs);
void sendFastSyncBlockAcknowledge(const std::string &nodeId, const std::string &msgId, const std::vector<FastSyncBlockHashs> &requestHashs);

int processFastSyncGetHashRequest(const std::shared_ptr<FastSyncGetHashReq> &msg, const MsgData &msgdata);
int handleFastSyncHashAcknowledge(const std::shared_ptr<FastSyncGetHashAck> &msg, const MsgData &msgdata);
int handleFastSyncBlockRequest(const std::shared_ptr<FastSyncGetBlockReq> &msg, const MsgData &msgdata);
int handleFastSyncBlockAcknowledge(const std::shared_ptr<FastSyncGetBlockAck> &msg, const MsgData &msgdata);

void sendSyncGetSumHashRequest(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void sendSyncSumHashAcknowledgement(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void sendSyncGetHeightHashRequest(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void sendSyncGetHeightHashAcknowledgment(SyncGetHeightHashAck& ack,const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void chainSyncGetBlockHeightAndHashRequest(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void getBlockHeightAndHashAcknowledge(SyncGetBlockHeightAndHashAck& ack,const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);
void sendSyncGetBlockRequest(const std::string &nodeId, const std::string &msgId, const std::vector<std::string> &reqHashes);
void sendSyncGetBlockAcknowledgement(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight);

int processSyncSumHashRequest(const std::shared_ptr<SyncGetSumHashReq> &msg, const MsgData &msgdata);
int syncGetSumHashAcknowledge(const std::shared_ptr<SyncGetSumHashAck> &msg, const MsgData &msgdata);
int handleSyncGetHeightHashRequest(const std::shared_ptr<SyncGetHeightHashReq> &msg, const MsgData &msgdata);
int syncBlockHeightAndHashRequest(const std::shared_ptr<SyncGetBlockHeightAndHashReq> &msg, const MsgData &msgdata);
int syncBlockHeightAndHashAck(const std::shared_ptr<SyncGetBlockHeightAndHashAck> &msg, const MsgData &msgdata);
int processSyncHeightHashAcknowledgement(const std::shared_ptr<SyncGetHeightHashAck> &msg, const MsgData &msgdata);
int handleSynchronizationBlockRequest(const std::shared_ptr<SyncGetBlockReq> &msg, const MsgData &msgdata);
int handleSyncGetBlockAcknowledge(const std::shared_ptr<SyncGetBlockAck> &msg, const MsgData &msgdata);


void sendSyncSumHashRequest(const std::string &nodeId, const std::string &msgId, const std::vector<uint64_t>& heights);
void syncSumHashAcknowledge(const std::string &nodeId, const std::string &msgId, const std::vector<uint64_t>& heights);
void sendFromZeroSyncGetBlockRequest(const std::string &nodeId, const std::string &msgId, uint64_t height);
void zeroSyncBlockAcknowledge(const std::string &nodeId, const std::string &msgId, uint64_t height);

int syncGetSumHashRequest(const std::shared_ptr<SyncFromZeroGetSumHashReq> &msg, const MsgData &msgdata);
int zeroSyncSumHashAcknowledge(const std::shared_ptr<SyncFromZeroGetSumHashAck> &msg, const MsgData &msgdata);
int handleSyncGetBlockRequest(const std::shared_ptr<SyncFromZeroGetBlockReq> &msg, const MsgData &msgdata);
int handleZeroSyncBlockAcknowledge(const std::shared_ptr<SyncFromZeroGetBlockAck> &msg, const MsgData &msgdata);

void sendSyncNodeHashRequest(const std::string &nodeId, const std::string &msgId);
void sendSynchronizationNodeHashConfirmation(const std::string &nodeId, const std::string &msgId);
int syncNodeHashRequest(const std::shared_ptr<SyncNodeHashReq> &msg, const MsgData &msgdata);
int processSyncNodeHashAcknowledgment(const std::shared_ptr<SyncNodeHashAck> &msg, const MsgData &msgdata);

int handleBlockByUtxoRequest(const std::shared_ptr<GetBlockByUtxoReq> &msg, const MsgData &msgdata);
int handleBlockByUtxoAcknowledgment(const std::shared_ptr<GetBlockByUtxoAck> &msg, const MsgData &msgdata);
int blockByHashRequest(const std::shared_ptr<GetBlockByHashReq> &msg, const MsgData &msgdata);
int handleBlockByHashAcknowledgment(const std::shared_ptr<GetBlockByHashAck> &msg, const MsgData &msgdata);

int processCheckVinRequest(const std::shared_ptr<CheckVinReq> &msg, const MsgData &msgdata);
int checkVinAcknowledge(const std::shared_ptr<CheckVinAck> &msg, const MsgData &msgdata);

#endif
