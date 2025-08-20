#include "block.pb.h"
#include "ca/global.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector> 

#include "ca/txhelper.h"
#include "ca/algorithm.h"
#include "ca/sync_block.h"
#include "ca/transaction.h"
#include "ca/block_helper.h"

#include <nlohmann/json.hpp>
#include "utils/console.h"
#include "utils/magic_singleton.h"
#include "utils/account_manager.h"

#include "db/db_api.h"
#include "net/dispatcher.h"
#include "include/logging.h"
#include "common/global_data.h"

const static uint64_t kStabilityTime = 60 * 1000000;
static uint64_t sync_fail_height = 0;
static bool runFastSync = false;
static bool isFastSyncFailed = false;

static bool execute_new_sync = false;
static uint64_t initialSyncStartHeight = 0;

static uint32_t syncHeightCount = 100;
const static uint32_t SYNC_HEIGHT_TIME = 10;
const static uint32_t SYNC_STUCK_OVERTIME_COUNT = 6;
const static double KScalingFactor = 0.95;
static uint64_t syncSendNewSyncNumber = UINT32_MAX;
static uint64_t syncSendFastSyncCount = global::ca::MIN_SYNC_QUAL_NODES;
static uint64_t sync_send_zero_sync_num = global::ca::MIN_SYNC_QUAL_NODES;
const  static int NORMAL_SUM_HASH_NUM = 1;
static uint32_t runFastSyncCount = 0;
static uint32_t runZeroSyncCount = 0;
const static uint32_t RollBackTime = 15 * 1000000;
const uint32_t SUBSTR_LEN = 8;

inline static global::ca::SaveType get_save_sync_type(uint64_t height, uint64_t chainHeight)
{
    if (chainHeight <= global::ca::hashRangeSum)
    {
        return global::ca::SaveType::SyncNormal;
    }
    
    global::ca::SaveType saveType;
    if(ca_algorithm::get_sum_hash_floor_height(chainHeight) - global::ca::hashRangeSum <= height)
    {
        saveType = global::ca::SaveType::SyncNormal;
    }
    else
    {
        saveType = global::ca::SaveType::SyncFromZero;
    }
    return saveType;
}
static bool calculateSumHeightHash(std::vector<std::string> &blockHashes, std::string &hash)
{
    std::sort(blockHashes.begin(), blockHashes.end());
    hash = Getsha256hash(StringUtil::concat(blockHashes, ""));
    return true;
}

bool SyncBlock::sumHeightsHash(const std::map<uint64_t, std::vector<std::string>>& heightBlocks, std::string &hash)
{
    std::vector<std::string> heightToBlockHash;
    for(auto height_block : heightBlocks)
    {
        std::string sumHash;
        calculateSumHeightHash(height_block.second, sumHash);
        heightToBlockHash.push_back(sumHash);
    }

    calculateSumHeightHash(heightToBlockHash, hash);
    return true;
}

static bool getHeightBlockHash(uint64_t startHeight, uint64_t endHeight, std::vector<std::string> &blockHashes)
{
    DBReader dbReader;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(startHeight, endHeight, blockHashes))
    {
        return false;
    }
    return true;
}

static bool getHeightBlockHash(uint64_t startHeight, uint64_t endHeight, std::vector<FastSyncBlockHashs> &height_hashes_of_blocks)
{
    DBReader dbReader;
    uint64_t top = 0;
    if(DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("(getHeightBlockHash) getBlockTop failed !");
        return false;
    }
    
    uint64_t currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    while (startHeight <= endHeight && startHeight <= top)
    {
        std::vector<std::string> hashs;
        if (DBStatus::DB_SUCCESS != dbReader.getBlockHashsByBlockHeight(startHeight, hashs))
        {
            return false;
        }
        std::vector<std::string> blocksRaw;
        if (DBStatus::DB_SUCCESS != dbReader.getBlocksByBlockHash(hashs, blocksRaw))
        {
            return false;
        }
        FastSyncBlockHashs fastSyncBlockHashList;
        fastSyncBlockHashList.set_height(startHeight);
        for(const auto& block_raw : blocksRaw)
        {
            CBlock block;
            if(!block.ParseFromString(block_raw))
            {
                return false;
            }

            if((currentTime - block.time()) < kStabilityTime)
            {
                continue;
            }
            fastSyncBlockHashList.add_hashs(block.hash());
        }
        height_hashes_of_blocks.push_back(fastSyncBlockHashList);
        ++startHeight;
    }
    return true;
}

static bool getHeightBlockHash(uint64_t startHeight, uint64_t endHeight, std::map<uint64_t, std::vector<std::string>> &height_hashes_of_blocks)
{
    DBReader dbReader;
    uint64_t top = 0;
    if(DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("(getHeightBlockHash) getBlockTop failed !");
        return false;
    }
    while (startHeight < endHeight && startHeight <= top)
    {
        std::vector<std::string> hashs;
        if (DBStatus::DB_SUCCESS != dbReader.getBlockHashsByBlockHeight(startHeight, hashs))
        {
            return false;
        }
        height_hashes_of_blocks[startHeight] = hashs;
        ++startHeight;
    }
    return true;
}

void  SyncBlock::ThreadStop(){

    syncThreadRunning=false;
}

void SyncBlock::ThreadStart(bool start)
{
    syncThreadRunning = start; 
}

void SyncBlock::ThreadStart()
{
    if(syncHeightCount > kSynchronizationBoundary)
    {
        syncHeightCount = kSynchronizationBoundary;
    }
    fastSyncHeightCount = 0; // 0 means "this variable doesn't use for now"
    
    syncThreadRunning = true;
    _syncThread = std::thread(
        [this]()
        {
            uint32_t  sleepTime = SYNC_HEIGHT_TIME;
            while (1)
            {
                if(!syncThreadRunning)
                {
                    sleep(sleepTime);
                    continue;
                }
                std::unique_lock<std::mutex> blockThreadRunningLocker(syncThreadRunningMutex);
                syncThreadRunningCondition.wait_for(blockThreadRunningLocker, std::chrono::seconds(sleepTime));
                syncThreadRunningMutex.unlock();
                if (!syncThreadRunning)
                {
                    continue;
                }
                else
                {
                    sleepTime = SYNC_HEIGHT_TIME;
                }
                uint64_t chainHeight = 0;
                if(!MagicSingleton<BlockHelper>::GetInstance()->getChainHeight(chainHeight))
                {
                    continue;
                }
                uint64_t nodeSelfHeight = 0;
                std::vector<std::string> pledgeAddr; // stake and delegatinged addr
                {
                    DBReader dbReader;
                    auto status = dbReader.getBlockTop(nodeSelfHeight);
                    if (DBStatus::DB_SUCCESS != status)
                    {
                        continue;
                    }
                    std::vector<Node> nodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
                    for (const auto &node : nodes)
                    {
                        int ret = VerifyBonusAddr(node.address);
                        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
                        if (stakeTime > 0 && ret == 0)
                        {
                            pledgeAddr.push_back(node.address);
                        }
                    }
                }

                uint64_t syncInitHeight = 0;
                uint64_t endSyncHeight_ = 0;

                if (!syncFromZeroCache.empty())
                {
                    std::lock_guard<std::mutex> lock(cacheSyncMutex);
                    for (auto iter = syncFromZeroCache.begin(); iter != syncFromZeroCache.end();)
                    {
                        if (iter->first < nodeSelfHeight)
                        {
                            iter = syncFromZeroCache.erase(iter);
                        }
                        else
                        {
                            iter++;
                        }
                    }
                }

                if(runFastSyncCount >= 5)
                {
                    if(runFastSync)
                    {
                        runFastSyncCount = 0;
                        runFastSync = false;
                    }
                }

                if(execute_new_sync)
                {
                    runFastSyncCount = 0;
                    runFastSync = false;
                }

                if(runFastSync)
                {
                    ++runFastSyncCount;

                    if (sync_fail_height > fastSyncHeightCount)
                    {
                        syncInitHeight = sync_fail_height - fastSyncHeightCount;
                    }
                    else
                    {
                        syncInitHeight = sync_fail_height;
                    }

                    if(syncInitHeight > nodeSelfHeight)
                    {
                        syncInitHeight = nodeSelfHeight + 1;
                    }

                    endSyncHeight_ = syncInitHeight + fastSyncHeightCount;

                    INFOLOG("begin fast sync {} {} ", syncInitHeight, endSyncHeight_);
                    DEBUGLOG("runFastSyncOnce, chainHeight:{}, pledge_addr_size:{}, syncSendFastSyncCount:{}", chainHeight, pledgeAddr.size(), syncSendFastSyncCount);
                    bool if_success = runFastSyncOnce(pledgeAddr, chainHeight, syncInitHeight, endSyncHeight_, syncSendFastSyncCount);
                    if(!if_success)
                    {
                        syncSendFastSyncCount = UINT32_MAX;
                        DEBUGLOG("fast sync fail");
                    }
                    else
                    {
                        syncSendFastSyncCount = global::ca::MIN_SYNC_QUAL_NODES;
                    }
                    runFastSync = false;
                    if(isFastSyncFailed)
                    {
                        isFastSyncFailed = false;
                        runFastSync = true;
                    }
                }
                else
                { 
                    auto syncType = get_save_sync_type(nodeSelfHeight, chainHeight);

                    if(execute_new_sync || runZeroSyncCount > 2)
                    {
                        syncType = global::ca::SaveType::SyncNormal;
                    }

                    if (syncType == global::ca::SaveType::SyncFromZero)
                    {
                        INFOLOG("begin from zero sync");
                        if(sync_send_zero_sync_num < (pledgeAddr.size() / 2))
                        {
                            sync_send_zero_sync_num = pledgeAddr.size() / 2;
                        }
                        DEBUGLOG("runFromZeroSyncOnce, chainHeight:{}, pledge_addr_size:{}, sync_send_zero_sync_num:{}", chainHeight, pledgeAddr.size(), sync_send_zero_sync_num);
                        int runStatus = runFromZeroSyncOnce(pledgeAddr, chainHeight, nodeSelfHeight, sync_send_zero_sync_num);
                        if(runStatus == 0)
                        {
                            sync_send_zero_sync_num = global::ca::MIN_SYNC_QUAL_NODES;
                        }
                        else if (runStatus != 0)
                        {
                            sync_send_zero_sync_num = UINT32_MAX;
                            ++runZeroSyncCount;
                            ERRORLOG("from zero sync fail ret: {},   runZeroSyncCount: {}", runStatus, runZeroSyncCount);
                        }
                        DEBUGLOG("runFromZeroSyncOnce ret: {}", runStatus);
                    }
                    else
                    {
                        if(sync_fail_height != 0)
                        {
                            nodeSelfHeight = sync_fail_height;
                        }
                        if (nodeSelfHeight > syncHeightCount)
                        {
                            syncInitHeight = nodeSelfHeight - syncHeightCount;
                            if(syncInitHeight <= 0)
                            {
                                syncInitHeight = 1;
                            }
                        }
                        else
                        {
                            syncInitHeight = 1;
                        }
                        endSyncHeight_ = nodeSelfHeight + syncHeightCount;
                        sleepTime = SYNC_HEIGHT_TIME;
                        INFOLOG("begin new sync {} {} ", syncInitHeight, endSyncHeight_);

                        if(execute_new_sync)
                        {
                            if(initialSyncStartHeight > nodeSelfHeight)
                            {
                                initialSyncStartHeight = nodeSelfHeight;
                            }
                            syncInitHeight = initialSyncStartHeight > syncHeightCount ? initialSyncStartHeight - syncHeightCount : 1;
                            endSyncHeight_ = initialSyncStartHeight + syncHeightCount;
                        }
                        DEBUGLOG("runNewSyncOnce, chainHeight:{}, pledge_addr_size:{}, syncSendNewSyncNumber:{}", chainHeight, pledgeAddr.size(), syncSendNewSyncNumber);
                        int runStatus = runNewSyncOnce(pledgeAddr, chainHeight, nodeSelfHeight, syncInitHeight, endSyncHeight_, syncSendNewSyncNumber);
                        if(runStatus < 0)
                        {   
                            sync_fail_height = 0;
                            runFastSync = false;
                        }
                        if(runStatus == 0)
                        {
                            runZeroSyncCount = 0;
                            execute_new_sync = false;
                            initialSyncStartHeight = 0;
                            sync_fail_height = 0;
                            runFastSync = false;
                        }
                        DEBUGLOG("runNewSyncOnce sync return: {}", runStatus);
                    }
                }
            }
        });
    _syncThread.detach();
}




bool SyncBlock::runFastSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t syncInitHeight, uint64_t endSyncHeight_, uint32_t syncNodeCount)
{
    if (syncInitHeight > endSyncHeight_)
    {
        return false;
    }
    std::vector<std::string> node_ids_to_send;
    if (getSyncNode(syncNodeCount, chainHeight, pledgeAddr, node_ids_to_send) != 0)
    {
        ERRORLOG("get sync node fail");
        return false;
    }
    std::vector<std::string> node_ids_returned;
    std::vector<FastSyncBlockHashs> requestHashs;

    INFOLOG("begin get_fast_sync_sum_hash_node {} {} ", syncInitHeight, endSyncHeight_);
    if (!get_fast_sync_sum_hash_node(node_ids_to_send, syncInitHeight, endSyncHeight_, requestHashs, node_ids_returned, chainHeight))
    {
        ERRORLOG("get sync sum hash fail");
        return false;
    }
    bool flag = false;
    for (auto &nodeId : node_ids_returned)
    {
        DEBUGLOG("fast sync block from {}", nodeId);
        if (get_fast_sync_block_data(nodeId,requestHashs, chainHeight))
        {
            flag = true;
            break;
        }
    }
    return flag;
}

int SyncBlock::runNewSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t nodeSelfHeight, uint64_t syncInitHeight, uint64_t endSyncHeight_, uint32_t syncNodeCount)
{
    int ret = 0;
    if (syncInitHeight > endSyncHeight_)
    {
        ret = -1;
        return ret;
    }
    std::vector<std::string> node_ids_to_send;
    if ((ret = getSyncNode(syncNodeCount, chainHeight, pledgeAddr, node_ids_to_send)) != 0)
    {
        ERRORLOG("get sync node fail");
        return ret - 1000;
    }

    std::vector<std::string> secretReturnNodeIds;
    std::vector<std::string> reqHashes;
    ret = getSyncBlockHashVerification(node_ids_to_send, syncInitHeight, endSyncHeight_, nodeSelfHeight, chainHeight, secretReturnNodeIds, reqHashes, syncNodeCount);
    if(ret != 0)
    {
        return -2000;
    }

    if(reqHashes.empty())
    {
        return 0;
    }

    if ((ret = getSyncBlockData(secretReturnNodeIds, reqHashes, chainHeight)) != 0 )
    {
        if (runFastSync)
        {
            return 2;
        }

        return ret -3000;
    }

    return 0;
}

int SyncBlock::runFromZeroSyncOnce(const std::vector<std::string> &pledgeAddr, uint64_t chainHeight, uint64_t nodeSelfHeight, uint32_t syncNodeCount)
{
    int ret = 0;
    std::vector<std::string> node_ids_to_send;
    if ((ret = getSyncNodeSimplified(syncNodeCount, chainHeight, pledgeAddr, node_ids_to_send)) != 0)
    {
        return ret - 1000;
    }
    std::set<std::string> node_ids_returned;
    std::vector<uint64_t> heights;
    if (!syncFromZeroReserveHeights.empty())
    {
        syncFromZeroReserveHeights.swap(heights);
    }
    else
    {
        uint64_t heightCeiling = ca_algorithm::get_sum_hash_floor_height(chainHeight) - global::ca::hashRangeSum;

        auto syncHeight = ca_algorithm::calculateSumHashCeilingHeight(nodeSelfHeight);
        DEBUGLOG("chainHeight:{}, syncHeight:{}, heightCeiling:{}", chainHeight, syncHeight, heightCeiling);
        if(heightCeiling - syncHeight > 1000)
        {
            heightCeiling = syncHeight + 1000;
        }

        heights = {syncHeight};
        for(auto i = node_ids_to_send.size() - 1; i > 0; --i)
        {
            syncHeight += global::ca::hashRangeSum;
            if (syncHeight > heightCeiling)
            {
                break;
            }
            heights.push_back(syncHeight);
        }        
    }

    std::map<uint64_t, std::string> sumHashes;

    std::string requestHeights;
    for(auto height : heights)
    {
        requestHeights += " ";
        requestHeights += std::to_string(height);
    }

    DEBUGLOG("get_from_zero_sync_sum_hash_node begin{}", requestHeights);
    if ((ret = get_from_zero_sync_sum_hash_node(node_ids_to_send, heights, nodeSelfHeight, node_ids_returned, sumHashes)) < 0)
    {
        ERRORLOG("get sync sum hash fail");
        return ret - 2000;
    }
    DEBUGLOG("get_from_zero_sync_sum_hash_node success");

    DEBUGLOG("getSyncBlockDataFromZero begin");
    if ((ret = getSyncBlockDataFromZero(sumHashes, heights, node_ids_returned, nodeSelfHeight)) != 0 )
    {
        return ret -3000;
    }
    DEBUGLOG("getSyncBlockData success");
    return 0;
}

int SyncBlock::getSyncNode(uint32_t num, uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                           std::vector<std::string> &node_ids_to_send)
{
    static bool forceFindBaseChainVar = false;
    DBReader dbReader;
    uint64_t nodeSelfHeight = 0;
    auto status = dbReader.getBlockTop(nodeSelfHeight);
    if (DBStatus::DB_SUCCESS != status)
    {
        ERRORLOG("get block top fail");
        return -10 - status;
    }

    if (nodeSelfHeight == chainHeight)
    {
        static uint64_t previousEqualHeight = 0;
        static uint32_t heightEqualCount = 0;

        if (nodeSelfHeight == previousEqualHeight)
        {
            heightEqualCount++;
            if (heightEqualCount >= SYNC_STUCK_OVERTIME_COUNT)
            {
                std::vector<std::string> selectedAddresses;
                if (needByzantineAdjustment(chainHeight, pledgeAddr, selectedAddresses))
                {
                    auto discardComparisonFunc = std::less<>();
                    auto compareReserve = std::greater_equal<>();
                    int ret = get_sync_node_basic(num, chainHeight, discardComparisonFunc, compareReserve, selectedAddresses,
                                            node_ids_to_send);
                    if (ret != 0)
                    {
                        DEBUGLOG("get sync node fail, ret: {}", ret);
                        forceFindBaseChainVar = true;
                    }
                    else
                    {
                        DEBUGLOG("Preparing to solve the forking problem");
                        return 0;
                    }
                }
            }
        }
        else
        {
            heightEqualCount = 0;
            previousEqualHeight = nodeSelfHeight;
        }
    }

    if (forceFindBaseChainVar || nodeSelfHeight < chainHeight)
    {
        DEBUGLOG("find node base on chain height {}", chainHeight);
        forceFindBaseChainVar = false;
        auto discardComparisonFunc = std::less<>();
        auto compareReserve = std::greater_equal<>();
        return get_sync_node_basic(num, chainHeight, discardComparisonFunc, compareReserve, pledgeAddr, node_ids_to_send);
    }
    else
    {
        DEBUGLOG("find node base on self height {}", nodeSelfHeight);
        forceFindBaseChainVar = true;
        auto discardComparisonFunc = std::less_equal<>();
        auto compareReserve = std::greater<>();
        return get_sync_node_basic(num, nodeSelfHeight, discardComparisonFunc, compareReserve, pledgeAddr,
                                node_ids_to_send);
    }
}

int SyncBlock::getSyncNodeSimplified(uint32_t num, uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                                   std::vector<std::string> &node_ids_to_send)
{
    DBReader dbReader;
    uint64_t nodeSelfHeight = 0;
    auto status = dbReader.getBlockTop(nodeSelfHeight);
    if (DBStatus::DB_SUCCESS != status)
    {
        ERRORLOG("get block top fail");
        return -10 - status;
    }
    DEBUGLOG("find node base on self height {}", nodeSelfHeight);
    auto discardComparisonFunc = std::less<>();
    auto compareReserve = std::greater_equal<>();
    return get_sync_node_basic(num, nodeSelfHeight, discardComparisonFunc, compareReserve, pledgeAddr, node_ids_to_send);
}

int SyncBlock::get_sync_node_basic(uint32_t num, uint64_t heightBaseline,
                                const std::function<bool(uint64_t, uint64_t)>& discardComparisonFunc,
                                const std::function<bool(uint64_t, uint64_t)>& compareReserve,
                                const std::vector<std::string> &pledgeAddr,
                                std::vector<std::string> &node_ids_to_send)
{
    DEBUGLOG("{} Nodes have passed qualification verification, heightBaseline:{}, num:{}", pledgeAddr.size(), heightBaseline, num);
    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("db get top failed!!");
        return -1;
    }

    std::vector<Node> nodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::vector<Node> qualifyingNode;
    uint64_t minHeight = heightBaseline;
    for (const auto &node : nodes)
    {
        int ret = VerifyBonusAddr(node.address);
        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
        if (stakeTime > 0 && ret == 0)
        {
            qualifyingNode.push_back(node);
        }

        if(top < global::ca::MIN_UNSTAKE_HEIGHT)
        {
            if(minHeight > node.height)
            {
                minHeight = node.height;
            }
        }
        else
        {
            if (stakeTime > 0 && ret == 0)
            {
                if(minHeight > node.height)
                {
                    minHeight = node.height;
                }
            }
        }

        DEBUGLOG("stakeTime:{}, ret:{}, addr:{}", stakeTime, ret, node.address);
    }

    DEBUGLOG("qualifyingNode size:{}, nodes size:{}", qualifyingNode.size(), nodes.size());

    if(num >= UINT32_MAX)
    {
        heightBaseline = minHeight;
        if(top < global::ca::MIN_UNSTAKE_HEIGHT)
        {
            num = nodes.size();
        }
        else
        {
            num = qualifyingNode.size();
        }

        DEBUGLOG("Enable Byzantine consensus across the entire network, num:{}", num);
    }
    else if(num < global::ca::MIN_SYNC_QUAL_NODES)
    {
        num = global::ca::MIN_SYNC_QUAL_NODES;
    }

    std::random_device rd;
    std::mt19937 gen(rd());

    std::set<std::string> sendNodeIdsContainer;
    if(top < global::ca::MIN_UNSTAKE_HEIGHT)
    {
        while (sendNodeIdsContainer.size() < num && !qualifyingNode.empty())
        {
            std::uniform_int_distribution<int> qualifyingNodeDistance(0, qualifyingNode.size() - 1);
            int index = qualifyingNodeDistance(gen);
            auto &node = qualifyingNode.at(index);
            if (node.height < heightBaseline)
            {
                qualifyingNode.erase(qualifyingNode.cbegin() + index);
                DEBUGLOG("qualifyingNode.erase, node.height:{}, heightBaseline:{}, addr:{}", node.height, heightBaseline, node.address);
                continue;
            }
            DEBUGLOG("qualifyingNode size:{}, addr:{}, index:{}, nodeHeight:{}",qualifyingNode.size(), node.address, index, node.height);
            sendNodeIdsContainer.insert(node.address);
            qualifyingNode.erase(qualifyingNode.cbegin() + index); 
        }

        while (sendNodeIdsContainer.size() < num && !nodes.empty()) 
        {
            std::uniform_int_distribution<int> distNodes(0, nodes.size() - 1);
            int index = distNodes(gen);
            auto &node = nodes.at(index);
            if (node.height < heightBaseline)
            {
                DEBUGLOG("node.height:{}, heightBaseline:{}, addr:{}", node.height, heightBaseline, node.address);
                nodes.erase(nodes.begin() + index);
                continue;
            }
            DEBUGLOG("sendNodeIdsContainer size:{}, addr:{}, index:{}, nodeHeight:{}",sendNodeIdsContainer.size(), node.address, index, node.height);
            sendNodeIdsContainer.insert(node.address);
            nodes.erase(nodes.begin() + index);
        }
    }
    else if(qualifyingNode.size() < global::ca::MIN_SYNC_QUAL_NODES || num < global::ca::MIN_SYNC_QUAL_NODES)
    {
        ERRORLOG("Insufficient qualifying nodes size:{}, global::ca::MIN_SYNC_QUAL_NODES:{}, num:{}", qualifyingNode.size(), global::ca::MIN_SYNC_QUAL_NODES, num);
        return -2;
    }
    else 
    {
        while (sendNodeIdsContainer.size() < num && !qualifyingNode.empty())
        {
            std::uniform_int_distribution<int> qualifyingNodeDistance(0, qualifyingNode.size() - 1);
            int index = qualifyingNodeDistance(gen);
            auto &node = qualifyingNode.at(index);
            if (node.height < heightBaseline)
            {
                qualifyingNode.erase(qualifyingNode.cbegin() + index);
                continue;
            }
            DEBUGLOG("qualifyingNode size:{}, addr:{}, index:{}, nodeHeight:{}",qualifyingNode.size(), node.address, index, node.height);
            sendNodeIdsContainer.insert(node.address);
            qualifyingNode.erase(qualifyingNode.cbegin() + index); 
        }
    }

    for(const auto& addr : sendNodeIdsContainer)
    {
        node_ids_to_send.push_back(addr);
    }
    if (node_ids_to_send.size() < num * 0.75)
    {
        for(auto &it : nodes)
        {
            DEBUGLOG("Node height:{}", it.height);
        }
        ERRORLOG("node_ids_to_send size: {}, num:{}",node_ids_to_send.size(), num);
        node_ids_to_send.clear();
        return -3;
    }
    return 0;
}

void SyncBlock::SetFastSync(uint64_t syncStartHeight)
{
    runFastSync = true;
    sync_fail_height = syncStartHeight;
}

void SyncBlock::set_new_sync_height(uint64_t height)
{
    execute_new_sync = true;
    if(initialSyncStartHeight == 0) 
    {
        initialSyncStartHeight = height;
    }
    if(initialSyncStartHeight > height)
    {
        initialSyncStartHeight = height;
    }
}

bool SyncBlock::get_fast_sync_sum_hash_node(const std::vector<std::string> &node_ids_to_send, uint64_t syncInitHeight, uint64_t endSyncHeight_,
                                       std::vector<FastSyncBlockHashs> &requestHashs, std::vector<std::string> &node_ids_returned, uint64_t chainHeight)
{
    DEBUGLOG("fast sync syncInitHeight: {}   tend_sync_height: {}", syncInitHeight, endSyncHeight_);
    requestHashs.clear();
    node_ids_returned.clear();
    std::string msgId;
    size_t sendNum = node_ids_to_send.size();
    if (!dataMgrPtr.CreateWait(30, sendNum * 0.8, msgId))
    {
        return false;
    }
    for (auto &nodeId : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, nodeId))
        {
            ERRORLOG("fast sync AddResNode error");
            return false;
        }
        DEBUGLOG("fast sync get block hash from {}", nodeId);
        fastSyncGetHashRequest(nodeId, msgId, syncInitHeight, endSyncHeight_);
    }
    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        if (retDatas.size() < sendNum * 0.5)
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
            return false;
        }
    }
    FastSyncGetHashAck ack;
    std::map<std::string, FastSynchronizationHelper> syncHashs;
    std::map<uint64_t, std::set<std::string>> fastSyncHashes;
    uint32_t ret_num = 0;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        auto heightResult = ack.hashs();

        uint64_t height = 0;
        std::set<std::string> heightToHashes;

        for(const auto& heightHashesResult : heightResult)
        {
            height = heightHashesResult.height();
            for(const auto& hash : heightHashesResult.hashs())
            {
                heightToHashes.insert(hash);
                auto it = syncHashs.find(hash);
                if (syncHashs.end() == it)
                {
                    FastSynchronizationHelper helper = {0, std::set<std::string>(), heightHashesResult.height()};
                    syncHashs.insert(make_pair(hash, helper));
                }
                auto &value = syncHashs.at(hash);
                value.hits = value.hits + 1;
                value.ids.insert(ack.self_node_id());
            }
            fastSyncHashes.insert(std::make_pair(height, heightToHashes));
        }
        ++ret_num;
    }

    std::vector<decltype(syncHashs.begin())> remainingSyncHashes;
    std::vector<std::string> rollbackSyncHashes;
    std::map<uint64_t, std::set<CBlock, BlockComparator>> rollbackBlockData_;

    DBReader dbReader;
    uint64_t currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    std::map<uint64_t, std::vector<std::string>> height_hashes_of_blocks;
    if(!getHeightBlockHash(syncInitHeight, endSyncHeight_ , height_hashes_of_blocks))
    {
        ERRORLOG("query database fail");
        return false;
    }
    
    uint64_t rollbackHeightNeeded = 0;
    for(auto iter = syncHashs.begin(); iter != syncHashs.end(); ++iter)
    {
        std::vector<std::string> localHashs;
        auto result = height_hashes_of_blocks.find(iter->second.height);
        if(result != height_hashes_of_blocks.end())
        {
            localHashs = result->second;
        }
        
        bool isByzantineSuccessful = iter->second.hits >= ret_num * 0.66;
        if (!isByzantineSuccessful)
        {
            std::string block_raw;
            if (DBStatus::DB_SUCCESS == dbReader.getBlockByBlockHash(iter->first, block_raw))
            {
                CBlock block;
                if(!block.ParseFromString(block_raw))
                {
                    ERRORLOG("block parse fail");
                    return false;
                }
                if((currentTime - block.time()) > kStabilityTime)
                {
                    addBlockToMap(block, rollbackBlockData_);
                    rollbackHeightNeeded = block.height();
                    break;
                }
            }
        }
        
        if(isByzantineSuccessful)
        {
            std::vector<std::string> selfBlockHashes_;
            if (DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(iter->second.height, iter->second.height, selfBlockHashes_))
            {
                ERRORLOG("getBlockHashesByBlockHeight error");
                return false;
            }
            std::sort(selfBlockHashes_.begin(), selfBlockHashes_.end());

            auto find = fastSyncHashes.find(iter->second.height);
            if(find != fastSyncHashes.end())
            {
                CBlock block;
                std::vector<std::string> diffHashes;
                std::set_difference(selfBlockHashes_.begin(), selfBlockHashes_.end(), find->second.begin(), find->second.end(), std::back_inserter(diffHashes));
                for(auto diffHash: diffHashes)
                {
                    block.Clear();
                    std::string strblock;
                    auto res = dbReader.getBlockByBlockHash(diffHash, strblock);
                    if (DBStatus::DB_SUCCESS != res)
                    {
                        ERRORLOG("getBlockByBlockHash failed");
                        return false;
                    }
                    block.ParseFromString(strblock);
                    
                    uint64_t tmp_height = block.height();
                    if ((tmp_height < chainHeight) && chainHeight - tmp_height > 10)
                    {
                        addBlockToMap(block, rollbackBlockData_);
                        rollbackHeightNeeded = tmp_height;
                        break;
                    }
                }
            }       
        }

        if (isByzantineSuccessful && (localHashs.empty() || std::find(localHashs.begin(), localHashs.end(), iter->first) == localHashs.end()))
        {
            remainingSyncHashes.push_back(iter);
        }
    }

    if (!rollbackBlockData_.empty())
    {
        std::vector<Node> nodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
        std::vector<Node> qualifyingNode;
        for (const auto &node : nodes)
        {
            int ret = VerifyBonusAddr(node.address);
            int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
            if (stakeTime > 0 && ret == 0)
            {
                qualifyingNode.push_back(node);
            }
        }
        if(syncSendFastSyncCount < qualifyingNode.size())
        {
            ERRORLOG("syncSendFastSyncCount:{} < qualifyingNode.size:{}", syncSendFastSyncCount, qualifyingNode.size());
            sync_fail_height = rollbackHeightNeeded;
            isFastSyncFailed = true;
            syncSendFastSyncCount = UINT32_MAX;
            return false;
        }

        DEBUGLOG("==== fast sync rollback ====");   
        MagicSingleton<BlockHelper>::GetInstance()->rollback_block_(rollbackBlockData_);
        return false;
    }

    auto remainBegin = remainingSyncHashes.begin();
    auto remain_end = remainingSyncHashes.end();
    if(remainBegin == remain_end)
    {
        return false;
    }
    std::set<std::string> ids = (*remainBegin)->second.ids;
    for(++remainBegin; remainBegin != remain_end; ++remainBegin)
    {
        std::set<std::string> intersectIds;
        auto& next_ids = (*remainBegin)->second.ids;
        std::set_intersection(next_ids.begin(), next_ids.end()
                                    , ids.begin(), ids.end()
                                    ,std::inserter(intersectIds, intersectIds.begin())
                                    );
        ids = intersectIds;
    }

    node_ids_returned = std::vector<std::string>(ids.begin(), ids.end());

    for(auto syncHashRemaining : remainingSyncHashes)
    {
        uint64_t height = syncHashRemaining->second.height;
        auto find_result = find_if(requestHashs.begin(), requestHashs.end(), 
                            [height](const FastSyncBlockHashs& entity)
                            {
                                return height == entity.height();
                            } 
                        );
        if(find_result == requestHashs.end())
        {
            FastSyncBlockHashs fastSyncBlockHashList;
            fastSyncBlockHashList.set_height(height);
            fastSyncBlockHashList.add_hashs(syncHashRemaining->first);
            requestHashs.push_back(fastSyncBlockHashList);
        }
        else
        {
            find_result->add_hashs(syncHashRemaining->first);
        }
    }
    return !ids.empty();
}

bool SyncBlock::get_fast_sync_block_data(const std::string &sendNodeId, const std::vector<FastSyncBlockHashs> &requestHashs, uint64_t chainHeight)
{
    if (requestHashs.empty())
    {
        DEBUGLOG("no byzantine data available");
        return true;
    }
    
    std::string msgId;
    if (!dataMgrPtr.CreateWait(30, 1, msgId))
    {
        DEBUGLOG("create wait fail");
        return false;
    }
    if(!dataMgrPtr.AddResNode(msgId, sendNodeId))
    {
        ERRORLOG("get_fast_sync_block_data AddResNode error");
        return false;
    }
    sendFastSyncGetBlockRequest(sendNodeId, msgId, requestHashs);
    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        ERRORLOG("wait fast sync data for {} fail", sendNodeId);
        return false;
    }
    if (retDatas.empty())
    {
        DEBUGLOG("return data empty");
        return false;
    }
    FastSyncGetBlockAck ack;
    if (!ack.ParseFromString(retDatas.at(0)))
    {
        DEBUGLOG("FastSyncGetBlockAck parse fail");
        return false;
    }
    CBlock block;
    CBlock hash_block;
    std::vector<std::string> blockHashes;
    std::map<uint64_t, std::set<CBlock, BlockComparator>> fastSyncBlockData;
    auto fastSyncBlocksCount = ack.blocks();
    std::sort(fastSyncBlocksCount.begin(), fastSyncBlocksCount.end(), [](const FastSyncBlock& b1, const FastSyncBlock& b2){return b1.height() < b2.height();});
    for (auto &blocks : fastSyncBlocksCount)
    {
        for(auto & block_raw : blocks.blocks())
        {
            if (block.ParseFromString(block_raw))
            {
                hash_block = block;
                hash_block.clear_hash();
                hash_block.clear_sign();
                if (block.hash() != Getsha256hash(hash_block.SerializeAsString()))
                {
                    continue;
                }
                DEBUGLOG("FastSync  block.height(): {} \tblock.hash(): {}", block.height(), block.hash());
                addBlockToMap(block, fastSyncBlockData);
                blockHashes.push_back(block.hash());
            }
        }
    }
    DBReader reader;
    uint64_t top;
    if (reader.getBlockTop(top) != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("getBlockTop fail");
        return false;
    }
    
    global::ca::SaveType syncType = get_save_sync_type(top, chainHeight);
    MagicSingleton<BlockHelper>::GetInstance()->AddFastSyncBlock(fastSyncBlockData, syncType);
    return true;
}

int SyncBlock::get_sync_sum_hash_node(uint64_t pledge_addr_size, const std::vector<std::string> &node_ids_to_send, uint64_t syncInitHeight, uint64_t endSyncHeight_,
                                   std::map<uint64_t, uint64_t> &shouldSyncHeights, std::vector<std::string> &node_ids_returned, uint64_t &chainHeight, uint64_t syncSendNum)
{
    DEBUGLOG("get_sync_sum_hash_node syncInitHeight: {}   endSyncHeight_: {}", syncInitHeight, endSyncHeight_);
    int ret = 0;
    shouldSyncHeights.clear();
    node_ids_returned.clear();
    std::string msgId;
    size_t sendNum = node_ids_to_send.size();

    double acceptanceRate = 0.8;
    if(syncSendNum >= UINT32_MAX)
    {
        DEBUGLOG("Enable Byzantine consensus across the entire network, syncSendNum:{}", syncSendNum);
        acceptanceRate = 0.9;
    }
    if (!dataMgrPtr.CreateWait(60, sendNum * acceptanceRate, msgId))
    {
        ret = -1;
        return ret;
    }
    for (auto &nodeId : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, nodeId))
        {
            ERRORLOG("get_sync_sum_hash_node AddResNode error");
            return -2;
        }
        sendSyncGetSumHashRequest(nodeId, msgId, syncInitHeight, endSyncHeight_);
    }
    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        if (retDatas.empty() || retDatas.size() < sendNum / 2)
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
            ret = -3;
            return ret;
        }
    }

    std::vector<uint64_t> node_tops_result;
    SyncGetSumHashAck ack;
    DBReader dbReader;

    //key:sumHash    pair.first:height    pair.second:node list
    std::map<std::string, std::pair<uint32_t, std::vector<std::string> >> consensusMap;
    std::vector<std::pair<std::string, uint32_t>> syncHashDataList;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        node_ids_returned.push_back(ack.self_node_id());
        node_tops_result.push_back(ack.node_block_height());
        for (auto &gSyncSumHash : ack.sync_sum_hashes())
        {
            auto find_sum_by_hash = consensusMap.find(gSyncSumHash.hash());
            if(find_sum_by_hash == consensusMap.end())
            {
                consensusMap.insert(std::make_pair(gSyncSumHash.hash(), std::make_pair(gSyncSumHash.start_height(), std::vector<std::string>() )));
            }
            auto &value = consensusMap.at(gSyncSumHash.hash());
            value.second.push_back(ack.self_node_id());

            std::string key = std::to_string(gSyncSumHash.start_height()) + "_" + std::to_string(gSyncSumHash.end_height()) + "_" + gSyncSumHash.hash();
            auto it = std::find_if(syncHashDataList.begin(), syncHashDataList.end(), [&key](const std::pair<std::string, uint32_t>& syncHashDatum){return key == syncHashDatum.first;});
            if (syncHashDataList.end() == it)
            {
                syncHashDataList.emplace_back(key, 1);
            }
            else
            {
                it->second += 1;
            }
        }
    }
    std::sort(node_tops_result.begin(), node_tops_result.end());
    int verifyNum = node_tops_result.size() * 0.66;
    if (verifyNum >= node_tops_result.size())
    {
        ERRORLOG("get chain height error index:{}:{}", verifyNum, node_tops_result.size());
        ret = -4;
        return ret;
    }
    chainHeight = node_tops_result.at(verifyNum);
    std::set<uint64_t> heights;
    std::string hash;
    uint64_t startHeight = 0;
    uint64_t endHeight = 0;
    std::vector<std::string> block_hashes;
    std::vector<std::string> data_key;

    std::sort(syncHashDataList.begin(),syncHashDataList.end(),[](const std::pair<std::string, uint32_t> v1, const std::pair<std::string, uint32_t> v2){
        std::vector<std::string> v1Height;
        StringUtil::SplitString(v1.first, "_", v1Height);
        std::vector<std::string> v2Height;
        StringUtil::SplitString(v2.first, "_", v2Height);

        return v1Height.at(0) < v2Height.at(0);
    });

    for (auto &syncHashDatum : syncHashDataList)
    {
        data_key.clear();
        StringUtil::SplitString(syncHashDatum.first, "_", data_key);
        if (data_key.size() < 3)
        {
            continue;
        }
        startHeight = std::stoull(data_key.at(0));
        endHeight = std::stoull(data_key.at(1));

        if (syncHashDatum.second < verifyNum)
        {
            DEBUGLOG("verify error, error height: {} syncHashDatum.second: {} verifyNum: {}", startHeight, syncHashDatum.second, verifyNum);
            uint64_t nodeSelfHeight = 0;
            if(DBStatus::DB_SUCCESS != dbReader.getBlockTop(nodeSelfHeight))
            {
                ERRORLOG("(get_sync_sum_hash_node) getBlockTop failed !");
                ret = -5;
                return ret;
            }

            if(syncSendNum >= UINT32_MAX)
            {
                ERRORLOG("syncSendNum{}   retDatas.size(){}", syncSendNum, retDatas.size());
                std::vector<std::pair<std::string, std::vector<std::string>>> nodeLists;
                std::string sumHashLocal;

                std::vector<std::string> selfBlockHashes_;
                auto res = dbReader.getBlockHashsByBlockHeight(startHeight, selfBlockHashes_);
                if(res != DBStatus::DB_SUCCESS)
                {
                    ERRORLOG("get {}  block hash failed !", startHeight);
                }

                if(!calculateSumHeightHash(selfBlockHashes_, sumHashLocal))
                {
                    sumHashLocal = "error";
                    ERRORLOG("get {} block sum hash failed !", startHeight);
                }

                uint64_t currentHeightNodeSize_ = 0;

                for(auto consensus : consensusMap)
                {
                    if(consensus.second.first == startHeight)
                    {
                        currentHeightNodeSize_ += consensus.second.second.size();
                        nodeLists.emplace_back(std::make_pair(consensus.first, std::move(consensus.second.second)));
                    }
                }

                sort(nodeLists.begin(), nodeLists.end(), [](const std::pair<std::string, std::vector<std::string>> list1, const std::pair<std::string, std::vector<std::string>> list2){
                    return list1.second.size() > list2.second.size();
                });

                if(nodeLists.at(0).second.size() < retDatas.size() * 0.51  //The most forks are less than 51%
                        && retDatas.size() >= sendNum * 0.80                 //80% At the highest altitude
                        && startHeight <= nodeSelfHeight)                   //startHeight is the Byzantine error height
                {                
                    ERRORLOG("first: nodeLists.at(0).second.size():{}\tret_datas.size() * 0.51:{}\tret_datas.size():{}\tsend_num * 0.80:{}\terror height:{}\tself_node_height:{}",
                        nodeLists.at(0).second.size(), retDatas.size() * 0.51,retDatas.size(), sendNum * 0.80,startHeight,nodeSelfHeight);
                    std::vector<std::string> errorHeightBlockHashList;
                    std::string strblock;
                    std::map<uint64_t, std::set<CBlock, BlockComparator>> rollbackBlockData_;
                    CBlock block;
                    
                    if(!getHeightBlockHash(startHeight, startHeight, errorHeightBlockHashList))
                    {
                        ERRORLOG("height:{} getHeightBlockHash error", startHeight);
                        continue;
                    }

                    for(const auto& error_hash: errorHeightBlockHashList) 
                    {
                        if(DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(error_hash, strblock))
                        {
                            continue;
                        }
                        
                        if(!block.ParseFromString(strblock))
                        {
                            continue;
                        }
                        uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
                        if(block.time() < nowTime - RollBackTime)
                        {
                            DEBUGLOG("currentHeightNodeSize_: {}\tret_datas.size() / 2:{}",currentHeightNodeSize_, retDatas.size() / 2);
                            DEBUGLOG("addBlockToMap rollback block height: {}\tblock hash:{}",block.height(), block.hash());
                            addBlockToMap(block, rollbackBlockData_);
                        }
                    }
                    if(!rollbackBlockData_.empty())
                    {
                        DEBUGLOG("==== new sync rollback first ====");
                        MagicSingleton<BlockHelper>::GetInstance()->rollback_block_(rollbackBlockData_);
                        return -6;
                    }
                }
                else if(nodeLists.at(0).second.size() >= retDatas.size() * 0.51
                        && retDatas.size() > sendNum * 0.80
                        && (startHeight <= nodeSelfHeight || startHeight == nodeSelfHeight + 1) )
                {
                    if(sumHashLocal != nodeLists.at(0).first)
                    {
                        ERRORLOG("second: nodeLists.at(0).second.size():{}\tret_datas.size() * 0.51:{}\tret_datas.size():{}\tsend_num * 0.80:{}\terror height:{}\tself_node_height:{}",
                            nodeLists.at(0).second.size(), retDatas.size() * 0.51,retDatas.size(), sendNum * 0.80,startHeight,nodeSelfHeight);

                        ret = get_sync_block_by_sum_hash_node(nodeLists.at(0).second, startHeight, endHeight, nodeSelfHeight, chainHeight, syncSendNum);
                        if(ret != 0)
                        {
                            ERRORLOG("get_sync_block_by_sum_hash_node failed !");
                            return -7;
                        }
                    }
                }
                else {
                    ERRORLOG("thread: nodeLists.at(0).second.size():{}\tret_datas.size() * 0.51:{}\tret_datas.size():{}\tsend_num * 0.80:{}\terror height:{}\tself_node_height:{}",
                        nodeLists.at(0).second.size(), retDatas.size() * 0.51,retDatas.size(), sendNum * 0.80,startHeight,nodeSelfHeight);
                    continue;
                }
            }
            else 
            {
                return -8;
            }
        }

        hash.clear();
        block_hashes.clear();
        getHeightBlockHash(startHeight, endHeight, block_hashes);
        calculateSumHeightHash(block_hashes, hash);
        if (data_key.at(2) == hash)
        {
            continue;
        }
        for (uint64_t i = startHeight; i <= endHeight; i++)
        {
            heights.insert(i);
        }
    }

    int unverifiedHeight = 10;
    int64_t start = -1;
    int64_t end = -1;
    for (auto value : heights)
    {
        if (-1 == start && -1 == end)
        {
            start = value;
            end = start;
        }
        else
        {
            if (value != (end + 1))
            {
                shouldSyncHeights.insert(std::make_pair(start, end));
                start = value;
                end = value;
            }
            else
            {
                end = value;
            }
        }
    }
    if (-1 != start && -1 != end)
    {
        shouldSyncHeights.insert(std::make_pair(start, end));
    }
    if (endSyncHeight_ >= (chainHeight - unverifiedHeight))
    {
        if (chainHeight > unverifiedHeight)
        {
            shouldSyncHeights.insert(std::make_pair(chainHeight - unverifiedHeight, chainHeight));
            shouldSyncHeights.insert(std::make_pair(chainHeight, chainHeight + unverifiedHeight));
        }
        else
        {
            shouldSyncHeights.insert(std::make_pair(1, chainHeight + unverifiedHeight));
        }
    }
    uint64_t nodeSelfHeight = 0;

    dbReader.getBlockTop(nodeSelfHeight);

    return 0;
}

int SyncBlock::get_sync_block_by_sum_hash_node(std::vector<std::string> &node_ids_to_send, uint64_t syncInitHeight, uint64_t endSyncHeight_, uint64_t nodeSelfHeight, uint64_t chainHeight, uint64_t syncSendNum)
{
    int ret = 0;
    std::string msgId;
    if (!dataMgrPtr.CreateWait(90, node_ids_to_send.size() * 0.8, msgId))
    {
        ret = -1;
        return ret;
    }
    for (auto &nodeId : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, nodeId))
        {
            ERRORLOG("get_sync_block_by_sum_hash_node AddResNode error");
            return -2;
        }
        DEBUGLOG("new sync get block hash from {}", nodeId);
        sendSyncGetHeightHashRequest(nodeId, msgId, syncInitHeight, endSyncHeight_);
    }
    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        if(retDatas.size() < node_ids_to_send.size() * 0.5)
        {
            ERRORLOG("wait sync block hash time out send:{} recv:{}", node_ids_to_send.size(), retDatas.size());
            ret = -3;
            return ret;
        }
    }

    SyncGetHeightHashAck ack;
    std::set<std::string> verifySet;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        if(ack.code() != 0)
        {
            continue;
        }
        std::string verify_str = "";
        for (auto &key : ack.block_hashes())
        {
            verify_str += key;
        }
        verifySet.insert(verify_str);

    }
    if(verifySet.size() != 1)
    {
        DEBUGLOG("Byzantium failed");
        return -4;
    }

    std::vector<std::string> selfBlockHashes_;
    if(nodeSelfHeight >= syncInitHeight)
    {
        DBReader dbReader;
        
        if(DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(syncInitHeight, endSyncHeight_, selfBlockHashes_))
        {
            ret = -5;
            DEBUGLOG("getBlockHashesByBlockHeight failed");
            return ret;
        }

        std::sort(selfBlockHashes_.begin(), selfBlockHashes_.end());

        std::map<uint64_t, std::set<CBlock, BlockComparator>> rollbackBlockData_;
        CBlock block;
        std::vector<std::string> diffHashes;
        std::set_difference(selfBlockHashes_.begin(), selfBlockHashes_.end(), ack.block_hashes().begin(), ack.block_hashes().end(), std::back_inserter(diffHashes));
        for(auto diffHash: diffHashes)
        {
            block.Clear();
            std::string strblock;
            auto res = dbReader.getBlockByBlockHash(diffHash, strblock);
            if (DBStatus::DB_SUCCESS != res)
            {
                DEBUGLOG("getBlockByBlockHash failed");
                return -6;
            }
            block.ParseFromString(strblock);

            uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
            if(block.time() < nowTime - RollBackTime)
            {
                addBlockToMap(block, rollbackBlockData_);
            }
        }

        if(!rollbackBlockData_.empty())
        {
            DEBUGLOG("==== new sync rollback ====");
            MagicSingleton<BlockHelper>::GetInstance()->rollback_block_(rollbackBlockData_);
            return -7;
        }
    }

    std::vector<std::string> need_sync_hashes;
    std::set_difference(ack.block_hashes().begin(), ack.block_hashes().end(), selfBlockHashes_.begin(), selfBlockHashes_.end(),  std::back_inserter(need_sync_hashes));
    ret = getSyncBlockData(node_ids_to_send, need_sync_hashes, chainHeight);
    if(ret != 0)
    {
        DEBUGLOG("getSyncBlockData failed {}", ret);
        return -8;
    }

    return 0;
}

int SyncBlock::get_from_zero_sync_sum_hash_node(const std::vector<std::string> &node_ids_to_send, const std::vector<uint64_t>& heightsForSending, uint64_t nodeSelfHeight, std::set<std::string> &node_ids_returned, std::map<uint64_t, std::string>& sumHashes)
{
    node_ids_returned.clear();
    std::string msgId;
    size_t sendNum = node_ids_to_send.size();

    double acceptanceRate = 0.8;
    if(sync_send_zero_sync_num > UINT32_MAX)
    {
        acceptanceRate = 0.9;
    }

    if (!dataMgrPtr.CreateWait(90, sendNum * acceptanceRate, msgId))
    {
        return -1;
    }

    for (auto &nodeId : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, nodeId))
        {
            ERRORLOG("get_from_zero_sync_sum_hash_node AddResNode error");
            return -2;
        }
        DEBUGLOG("get from zero sync sum hash from {}", nodeId);
        sendSyncSumHashRequest(nodeId, msgId, heightsForSending);
    }
    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        if (retDatas.empty() || retDatas.size() < sendNum / 2)
        {
            ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
            return -3;
        }
    }
    
    std::map<std::string, std::pair<uint64_t, std::vector<std::string>>> hashDataTotal;
    int success_count = 0;
    for (auto &retData : retDatas)
    {
        SyncFromZeroGetSumHashAck ack;
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        if (ack.code() == 0)
        {
            if(sync_send_zero_sync_num >= UINT32_MAX)
            {
                ++success_count;
            }
            continue;
        }
        ++success_count;
        auto resultSumHashes = ack.sum_hashes();
        for(const auto& sumHash : resultSumHashes)
        {
            const auto& hash = sumHash.hash();
            auto height = sumHash.height();
            

            auto found = hashDataTotal.find(hash);
            if (found == hashDataTotal.end())
            {
                hashDataTotal.insert(std::make_pair(hash, std::make_pair(height, std::vector<std::string>())));
                continue;
            }
            auto& content = found->second;
            content.second.push_back(ack.self_node_id());  
        }
    }

    uint64_t backNum = sendNum * 0.66;
    bool isByzantineSuccessful = success_count > backNum;
    if(sync_send_zero_sync_num >= UINT32_MAX)
    {
        backNum = sendNum * 0.80;
        isByzantineSuccessful = success_count >= backNum;
        DEBUGLOG("backNum = {} ,sendNum:{}, isByzantineSuccessful = {}, sendNum * acceptanceRate:{}", backNum, sendNum, isByzantineSuccessful, sendNum * acceptanceRate);
    }

    if(!isByzantineSuccessful)
    {
        ERRORLOG("checkByzantineFault error, sendNum = {} , success_count = {}", sendNum, success_count);
        return -4;
    }

    //key:sumHash    pair.first:height    pair.second:node list
    std::map<std::string, std::pair<uint32_t, std::vector<std::string> >> byzantine_error_heights;
    for(const auto& hash_data_total : hashDataTotal)
    {
        bool isByzantineSuccess = hash_data_total.second.second.size() >= success_count * 0.66;
        if(isByzantineSuccess)
        {
            sumHashes[hash_data_total.second.first] = hash_data_total.first;
            for(const auto& t : hash_data_total.second.second)
            {
                node_ids_returned.insert(std::move(t));
            }
        }
        else if(sync_send_zero_sync_num >= UINT32_MAX)
        {
            //The whole network Byzantium failed     Only the last 100 heights are synchronized
            if(hash_data_total.second.first > nodeSelfHeight / 100 * 100 + 100)
            {
                continue;
            }
            
            auto find = sumHashes.find(hash_data_total.second.first);
            if(find != sumHashes.end())
            {
                continue;
            }

                                 //sum hash         node ids  
            std::vector<std::pair<std::string, std::vector<std::string>>> nodeIds;
            for(const auto& sumHash: hashDataTotal)
            {
                if(sumHash.second.first == hash_data_total.second.first)
                {
                    nodeIds.emplace_back(make_pair(sumHash.first, std::move(sumHash.second.second)));
                }
            }

            std::sort(nodeIds.begin(), nodeIds.end(), [](const std::pair<std::string, std::vector<std::string>>& list1, const std::pair<std::string, std::vector<std::string>>& list2){
                return list1.second.size() > list2.second.size();
            });

            sumHashes.clear();
            node_ids_returned.clear();
            
            sumHashes[hash_data_total.second.first] = nodeIds[0].first;
            for(const auto& t : nodeIds[0].second)
            {
                node_ids_returned.insert(std::move(t));
            }

            break;
        }
    }

    if (sumHashes.empty())
    {
        return -5;
    }
    
    return 0;
}

int SyncBlock::getSyncBlockDataFromZero(const std::map<uint64_t, std::string>& sumHashes, std::vector<uint64_t> &heightsForSending, std::set<std::string> &set_send_node_ids, uint64_t nodeSelfHeight)
{
    int ret = 0;
    if (set_send_node_ids.empty() || sumHashes.empty())
    {
        return -1;
    }
    if(set_send_node_ids.size() < sumHashes.size())
    {
        DEBUGLOG("node_ids_to_send.size() < sumHashes.size(), {}:{}", set_send_node_ids.size(), sumHashes.size());
        return -1;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::vector<std::string> node_ids_to_send(set_send_node_ids.begin(), set_send_node_ids.end());
    std::shuffle(node_ids_to_send.begin(), node_ids_to_send.end(), gen);
    node_ids_to_send.resize(sumHashes.size());

    if (node_ids_to_send.size() != sumHashes.size())
    {
        DEBUGLOG("node_ids_to_send.size() != sumHashes.size(), {}:{}", node_ids_to_send.size(), sumHashes.size());
        return -2;
    }
    
    std::string msgId;
    if (!dataMgrPtr.CreateWait(90, node_ids_to_send.size(), msgId))
    {
        return -3;
    }

    int sendNodeIndex = 0;
    for(const auto& hashItemSum : sumHashes)
    {
        auto nodeId  = std::next(node_ids_to_send.begin(), sendNodeIndex);
        auto sumHashHeightV2 = hashItemSum.first;
        if(!dataMgrPtr.AddResNode(msgId, *nodeId))
        {
            ERRORLOG("getSyncBlockDataFromZero AddResNode error");
            return -4;
        }
        sendFromZeroSyncGetBlockRequest(*nodeId, msgId, sumHashHeightV2);
        DEBUGLOG("from zero sync get block at height {} from {}", sumHashHeightV2, *nodeId);
        ++sendNodeIndex;
    }
    
    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        DEBUGLOG("wait sync block data time out send:{} recv:{}", node_ids_to_send.size(), retDatas.size());
    }
    
    DBReader dbReader; 
    std::vector<uint64_t> successfulHeights_;
    
    for(const auto& retData : retDatas)
    {
        std::map<uint64_t, std::set<CBlock, BlockComparator>> syncBlockData;
        SyncFromZeroGetBlockAck ack;
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        
        auto blockraws = ack.blocks();
        std::map<uint64_t, std::vector<std::string>> hashCheckDataSum; 
        std::vector<CBlock> hash_data_total;
        for(const auto& block_raw : blockraws)
        {
            CBlock block;
            if(!block.ParseFromString(block_raw))
            {
                ERRORLOG("block parse fail");
                break;
            }

            auto blockHeight = block.height();
            auto found = hashCheckDataSum.find(blockHeight);
            if(found == hashCheckDataSum.end())
            {
                hashCheckDataSum[blockHeight] = std::vector<std::string>();
            }
            auto& hashSumVector = hashCheckDataSum[blockHeight];
            hashSumVector.push_back(block.hash());
            
            hash_data_total.push_back(block);

        }

        std::string calculateSumHashValue;
        sumHeightsHash(hashCheckDataSum, calculateSumHashValue);
        auto found = sumHashes.find(ack.height());
        if(found == sumHashes.end())
        {
            DEBUGLOG("fail to get sum hash at height {}", ack.height());
            continue;
        }
        if (calculateSumHashValue != found->second)
        {
            ERRORLOG("check sum hash at height {} fail, calculateSumHashValue:{}, sumHash:{}", ack.height(), calculateSumHashValue, found->second);
            continue;
        }

        for(const auto& hash_check : hashCheckDataSum)
        {
            std::vector<std::string> blockHashes;
            if(nodeSelfHeight >= hash_check.first)
            {
                if (DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(hash_check.first, hash_check.first, blockHashes))
                {
                    return -5;
                }
                std::string selfSumHash;
                std::string otherSumHashValue;

                auto find_height = hashCheckDataSum.find(hash_check.first);
                if(find_height != hashCheckDataSum.end())
                {
                    if(calculateSumHeightHash(blockHashes, selfSumHash) && calculateSumHeightHash(find_height->second, otherSumHashValue))
                    {
                        if(selfSumHash != otherSumHashValue)
                        {
                            std::map<uint64_t, std::set<CBlock, BlockComparator>> rollbackBlockData_;
                            CBlock block;
                            std::vector<std::string> diffHashes;
                            std::set_difference(blockHashes.begin(), blockHashes.end(), find_height->second.begin(), find_height->second.end(), std::back_inserter(diffHashes));
                            for(auto diffHash: diffHashes)
                            {
                                block.Clear();
                                std::string strblock;
                                auto res = dbReader.getBlockByBlockHash(diffHash, strblock);
                                if (DBStatus::DB_SUCCESS != res)
                                {
                                    DEBUGLOG("getBlockByBlockHash failed");
                                    return -6;
                                }
                                block.ParseFromString(strblock);
                                
                                addBlockToMap(block, rollbackBlockData_);
                            }

                            if(!rollbackBlockData_.empty())
                            {
                                std::vector<Node> nodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
                                std::vector<Node> qualifyingNode;
                                for (const auto &node : nodes)
                                {
                                    int ret = VerifyBonusAddr(node.address);
                                    int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
                                    if (stakeTime > 0 && ret == 0)
                                    {
                                        qualifyingNode.push_back(node);
                                    }
                                }

                                if(sync_send_zero_sync_num < qualifyingNode.size())
                                {
                                    DEBUGLOG("sync_send_zero_sync_num:{} < qualifyingNode.size:{}", sync_send_zero_sync_num, qualifyingNode.size());
                                    sync_send_zero_sync_num = UINT32_MAX;
                                    return -7;
                                }
                                DEBUGLOG("==== getSyncBlockDataFromZero rollback ====");
                                MagicSingleton<BlockHelper>::GetInstance()->rollback_block_(rollbackBlockData_);
                            }
                        }
                    }
                }
            }
        }

        successfulHeights_.push_back(ack.height());
        for(const auto& block : hash_data_total)
        {
            addBlockToMap(block, syncBlockData);
        }
        {
            std::lock_guard<std::mutex> lock(cacheSyncMutex);
            syncFromZeroCache[ack.height()] = syncBlockData;
        }
    }
    if (successfulHeights_.empty())
    {
        return -8;
    }
        
    for(auto height : successfulHeights_)
    {
        auto found = std::find(heightsForSending.begin(), heightsForSending.end(), height);
        if (found != heightsForSending.end())
        {
            heightsForSending.erase(found);
        }
        
    }
    syncFromZeroReserveHeights.clear();
    for(auto fail_height : heightsForSending)
    {
        syncFromZeroReserveHeights.push_back(fail_height);
    }

    if(!heightsForSending.empty())
    {
        return -9;
    }

    if (heightsForSending.empty())
    {
        auto sync_add_thread_from_zero = std::thread(
                [this]()
                {
                    std::lock_guard<std::mutex> lock(cacheSyncMutex);
                    INFOLOG("sync_add_thread_from_zero start");
                    for(const auto& cache : this->syncFromZeroCache)
                    {
                        MagicSingleton<BlockHelper>::GetInstance()->AddSyncBlock(cache.second, global::ca::SaveType::SyncFromZero);
                    }
                    syncFromZeroCache.clear();
                }
        );
        sync_add_thread_from_zero.detach();
    }
    return 0;
}


int SyncBlock::getSyncBlockHashVerification(const std::vector<std::string> &node_ids_to_send, uint64_t syncInitHeight, uint64_t endSyncHeight_, uint64_t nodeSelfHeight, uint64_t chainHeight,
                        std::vector<std::string> &node_ids_returned, std::vector<std::string> &reqHashes, uint64_t syncSendNum)
{
    DEBUGLOG("getSyncBlockHashVerification sendNodeIs num: {}\tstartSyncHeight: {}\tendSyncHeight: {}", node_ids_to_send.size() ,syncInitHeight, endSyncHeight_);
    DEBUGLOG("getSyncBlockHashVerification nodeSelfHeight: {}\tchainHeight: {}\tnewSyncSendNum: {}", nodeSelfHeight, chainHeight, syncSendNum);

    node_ids_returned.clear();
    reqHashes.clear();
    size_t sendNum = node_ids_to_send.size();
    double acceptanceRate = 0.8;

    int ret = 0;
    std::string msgId;
    uint64_t successCounter = 0;
    if (!dataMgrPtr.CreateWait(60, sendNum * acceptanceRate, msgId))
    {
        ret = -1;
        return ret;
    }

    auto beginTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    for (auto &nodeId : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, nodeId))
        {
            ERRORLOG("getSyncBlockHashVerification AddResNode error");
            return -2;
        }
        DEBUGLOG("getSyncBlockHashVerification new sync get block hash from {}", nodeId);
        chainSyncGetBlockHeightAndHashRequest(nodeId, msgId, syncInitHeight, endSyncHeight_);
    }

    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        if (retDatas.size() < sendNum * acceptanceRate - 1)
        {
            ERRORLOG("getSyncBlockHashVerification wait sync height time out send:{}\trecv:{}\tsendNum * acceptanceRate - 1{}", sendNum, retDatas.size(), sendNum * acceptanceRate - 1);
            ret = -3;
            return ret;
        }
    }
    auto endTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    DEBUGLOG("BABABA getSyncBlockHashVerification beginTime:{}, endTime:{}, millisecond:{}", beginTime, endTime, endTime - beginTime);

    //key:blockHash    pair.first:height    pair.second:node list
    std::map<uint64_t, std::map<std::string, std::set<std::string> >> consensusMap;
    DBReader dbReader;

    std::set<uint64_t> blockHeights;

    SyncGetBlockHeightAndHashAck ack;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        if(ack.code() != 0)
        {
            ERRORLOG("getSyncBlockHashVerification startSyncHeitght:{}, endSyncHeight_:{}, nodeSelfHeight:{} ack.code():{}", syncInitHeight, endSyncHeight_, nodeSelfHeight, ack.code());
            continue;
        }

        std::vector<std::string> data_key;
        for(auto& heightAndHash: ack.block_hashes())
        {
            data_key.clear();
            StringUtil::SplitString(heightAndHash, "_", data_key);
            if(data_key.size() != 2)
            {
                continue;
            }

            uint64_t height = std::stoull(data_key[0]);
            std::string hash = data_key[1];
            blockHeights.insert(height);

            auto findHeight = consensusMap.find(height);
            if(findHeight == consensusMap.end())
            {
                consensusMap[height][hash] = {ack.self_node_id()};
            }
            else 
            {
                auto findHash = findHeight->second.find(hash);
                if(findHash == findHeight->second.end())
                {
                    findHeight->second[hash] = {ack.self_node_id()};
                }
                else
                {
                    findHash->second.insert(ack.self_node_id());
                }
            }
        }
    }

    for(const auto& height: blockHeights)
    {
        std::vector<std::string> selfBlockHashes_;
        auto dbRet = dbReader.getBlockHashesByBlockHeight(height, height, selfBlockHashes_);
        if(DBStatus::DB_SUCCESS != dbRet && DBStatus::DB_NOT_FOUND != dbRet)
        {
            ret = -4;
            DEBUGLOG("getSyncBlockHashVerification blockHeights getBlockHashesByBlockHeight failed, dbRet:{}", dbRet);
            return ret;
        }
        for(auto& currentBlockHash: selfBlockHashes_)
        {
            std::string substr = currentBlockHash.substr(0, SUBSTR_LEN);
            auto findHeight = consensusMap.find(height);
            if(findHeight == consensusMap.end())
            {
                continue;
            }
            auto findHash = findHeight->second.find(substr);
            if(findHash == findHeight->second.end())
            {
                findHeight->second[substr] = {"temp"};
            }
            else
            {
                findHash->second.insert("temp");
            }
        }
    }

    int verifyNum = (retDatas.size() + 1) * 0.51;

    std::set<std::string> nodeIds;
    std::vector<std::string> needToSyncBlockHashes;
    std::map<uint64_t, std::set<CBlock, BlockComparator>> rollbackBlockData_;

    for(auto& heightAndHash: consensusMap)
    {
        std::vector<std::string> selfBlockHashes_;
        auto dbRet = dbReader.getBlockHashesByBlockHeight(heightAndHash.first, heightAndHash.first, selfBlockHashes_);
        if(DBStatus::DB_SUCCESS != dbRet && DBStatus::DB_NOT_FOUND != dbRet)
        {
            ret = -5;
            DEBUGLOG("getSyncBlockHashVerification getBlockHashesByBlockHeight failed, dbRet:{}", dbRet);
            return ret;
        }

        for(auto& selfBlockHashValue: selfBlockHashes_)
        {
            selfBlockHashValue = std::move(selfBlockHashValue.substr(0, SUBSTR_LEN));
        }

        for(auto& hash_and_node: heightAndHash.second)
        {
            if(hash_and_node.second.size() >= verifyNum)
            {
                
                auto findHash = std::find(selfBlockHashes_.begin(), selfBlockHashes_.end(), hash_and_node.first);
                if(findHash != selfBlockHashes_.end())
                {
                    continue;
                }

                needToSyncBlockHashes.emplace_back(std::to_string(heightAndHash.first) + "_" + hash_and_node.first);

                if(nodeIds.empty())
                {
                    nodeIds.insert(hash_and_node.second.begin(), hash_and_node.second.end());
                }
                else 
                {
                    std::set<std::string> temp;
                    std::set_intersection(
                        nodeIds.begin(), nodeIds.end(),
                        hash_and_node.second.begin(), hash_and_node.second.end(),
                        std::inserter(temp, temp.begin())
                    );
                    nodeIds.clear();
                    nodeIds.insert(temp.begin(), temp.end());
                }
            }
            else 
            {
                auto findHash = std::find(selfBlockHashes_.begin(), selfBlockHashes_.end(), hash_and_node.first);
                if(findHash == selfBlockHashes_.end())
                {
                    continue;
                }

                std::string strblock;
                CBlock block;
                std::vector<std::string> blockHashes;
                dbRet = dbReader.getBlockHashesByBlockHeight(heightAndHash.first, heightAndHash.first, blockHashes);
                if(DBStatus::DB_SUCCESS != dbRet)
                {
                    ERRORLOG("getSyncBlockHashVerification getBlockHashesByBlockHeight failed, dbRet:{}", dbRet);
                    continue;
                }

                std::string robackHash;
                for(auto& hash: blockHashes)
                {
                    if(hash.substr(0, SUBSTR_LEN) == hash_and_node.first)
                    {
                        robackHash = std::move(hash);
                        break;
                    }
                }

                if(robackHash.empty())
                {
                    ERRORLOG("getSyncBlockHashVerification rollback hash not found");
                    return -6;
                }

                dbRet = dbReader.getBlockByBlockHash(robackHash, strblock);
                if(DBStatus::DB_SUCCESS != dbRet)
                {
                    ERRORLOG("getSyncBlockHashVerification getBlockByBlockHash failed, dbRet:{}", dbRet);
                    continue;
                }
                if(!block.ParseFromString(strblock))
                {
                    continue;
                }
                uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
                if(block.time() < nowTime - RollBackTime)
                {
                    DEBUGLOG("getSyncBlockHashVerification rollback sendNum:{}\tretDatas num:{}\tverifyNum:{}\t,hash_and_node.second.size():{}",
                        sendNum, retDatas.size(), verifyNum, hash_and_node.second.size());
                    DEBUGLOG("getSyncBlockHashVerification rollback addBlockToMap rollback block height: {}\tblock hash:{}",block.height(), block.hash());
                    addBlockToMap(block, rollbackBlockData_);
                }
            }
        }
    }

    if(!rollbackBlockData_.empty())
    {
        DEBUGLOG("==== new sync rollback first ====");
        MagicSingleton<BlockHelper>::GetInstance()->rollback_block_(rollbackBlockData_);
    }

    auto temp = nodeIds.find("temp");
    if(temp != nodeIds.end())
    {
        nodeIds.erase(temp);
    }
    node_ids_returned.insert(node_ids_returned.end(), nodeIds.begin(), nodeIds.end());
    reqHashes = std::move(needToSyncBlockHashes);

    return 0;
}


int SyncBlock::get_sync_block_hash_node(const std::vector<std::string> &node_ids_to_send, uint64_t syncInitHeight,
                                     uint64_t endSyncHeight_, uint64_t nodeSelfHeight, uint64_t chainHeight,
                                     std::vector<std::string> &node_ids_returned, std::vector<std::string> &reqHashes, uint64_t syncSendNum)
{
    int ret = 0;
    std::string msgId;
    uint64_t successCounter = 0;
    if (!dataMgrPtr.CreateWait(60, node_ids_to_send.size() * 0.8, msgId))
    {
        ret = -1;
        return ret;
    }
    for (auto &nodeId : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, nodeId))
        {
            ERRORLOG("get_sync_block_hash_node AddResNode error");
            return -2;
        }
        DEBUGLOG("new sync get block hash from {}", nodeId);
        sendSyncGetHeightHashRequest(nodeId, msgId, syncInitHeight, endSyncHeight_);
    }
    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        if(retDatas.size() < node_ids_to_send.size() * 0.5)
        {
            ERRORLOG("wait sync block hash time out send:{} recv:{}", node_ids_to_send.size(), retDatas.size());
            ret = -3;
            return ret;
        }
    }
    SyncGetHeightHashAck ack;
    std::map<std::string, std::set<std::string>> syncBlockHashList;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        if(ack.code() != 0)
        {
            continue;
        }
        successCounter++;
        for (auto &key : ack.block_hashes())
        {
            auto it = syncBlockHashList.find(key);
            if (syncBlockHashList.end() == it)
            {
                syncBlockHashList.insert(std::make_pair(key, std::set<std::string>()));
            }
            auto &value = syncBlockHashList.at(key);
            value.insert(ack.self_node_id());
        }
    }

    std::set<std::string> nodes;
    std::vector<std::string> intersectionNodes;
    std::set<std::string> verify_hashes;
    reqHashes.clear();
    if(successCounter < (size_t)(retDatas.size() * 0.66))
    {
        ERRORLOG("ret data error successCounter:{}, (uint32_t)(retDatas * 0.66):{}, retDatas.size():{}", successCounter, (size_t)(retDatas.size() * 0.66), retDatas.size());
        return -11;
    }
    size_t verifyNum = successCounter / 5 * 4;
    // Put the block hash greater than 60% into the array
    std::string strblock;
    std::vector<std::string> exitsHashes;
    std::map<uint64_t, std::set<CBlock, BlockComparator>> rollbackBlockData_;
    CBlock block;
    DBReader dbReader;
    for (auto &syncBlockHash : syncBlockHashList)
    {
        strblock.clear();
        auto res = dbReader.getBlockByBlockHash(syncBlockHash.first, strblock);
        if (DBStatus::DB_SUCCESS == res)
        {
            exitsHashes.push_back(syncBlockHash.first);
        }
        else if(DBStatus::DB_NOT_FOUND != res)
        {
            ret = -4;
            return ret;
        }
        if (syncBlockHash.second.size() >= verifyNum)
        {

            if (DBStatus::DB_NOT_FOUND == res)
            {
                verify_hashes.insert(syncBlockHash.first);
                if (nodes.empty())
                {
                    nodes = syncBlockHash.second;
                }
                else
                {
                    std::set_intersection(nodes.cbegin(), nodes.cend(), syncBlockHash.second.cbegin(), syncBlockHash.second.cend(), std::back_inserter(intersectionNodes));
                    nodes.insert(intersectionNodes.cbegin(), intersectionNodes.cend());
                    intersectionNodes.clear();
                }
            }
        }

        // When the number of nodes where the block is located is less than 80%, and the block exists locally, the block is rolled back
        else
        {
            if (DBStatus::DB_SUCCESS == res && block.ParseFromString(strblock))
            {
                uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
                DEBUGLOG("nowTime: {}", MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(nowTime));
                DEBUGLOG("blockTime: {}", MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(block.time()));
                if(block.time() < nowTime - RollBackTime)
                {
                    ERRORLOG("add rollback block, hash:{} height:{}", block.hash(), block.height());
                    addBlockToMap(block, rollbackBlockData_);
                    continue;
                }
            }
        }
    }

    std::vector<std::string> v_diff;
    uint64_t endHeight = endSyncHeight_ > nodeSelfHeight ? nodeSelfHeight : endSyncHeight_;
    if(endHeight >= syncInitHeight)
    {

        //Get all block hashes in the local height range, determine whether they are in the trusted list, and roll them back when they are no longer in the trusted list
        std::vector<std::string> blockHashes;
        if(DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(syncInitHeight, endHeight, blockHashes))
        {
            ERRORLOG("get_sync_block_hash_node getBlockHashesByBlockHeight error, syncInitHeight = {}, endHeight = {}", syncInitHeight, endHeight);
            ret = -5;
            return ret;
        }
        std::sort(blockHashes.begin(), blockHashes.end());
        std::sort(exitsHashes.begin(), exitsHashes.end());
        std::set_difference(blockHashes.begin(), blockHashes.end(), exitsHashes.begin(), exitsHashes.end(), std::back_inserter(v_diff));
    }

    for (const auto & it : v_diff)
    {

        block.Clear();
        std::string().swap(strblock);
        auto ret_status = dbReader.getBlockByBlockHash(it, strblock);
        if (DBStatus::DB_SUCCESS != ret_status)
        {
            continue;
        }
        block.ParseFromString(strblock);

        //It will only be rolled back when the height of the block to be added is less than the maximum height on the chain of -10
        uint64_t tmpHeight = block.height();
        uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
        DEBUGLOG("nowTime: {}", MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(nowTime));
        DEBUGLOG("blockTime: {}", MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(block.time()));
        if ((tmpHeight < chainHeight) && (block.time() < nowTime - RollBackTime))
        {
            ERRORLOG("add rollback block, hash:{} height:{}", block.hash(), block.height());
            addBlockToMap(block, rollbackBlockData_);
            continue;
        }
    }

    if(!rollbackBlockData_.empty())
    {
        std::vector<Node> nodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
        std::vector<Node> qualifyingNode;
        for (const auto &node : nodes)
        {
            int ret = VerifyBonusAddr(node.address);

            int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
            if (stakeTime > 0 && ret == 0)
            {
                qualifyingNode.push_back(node);
            }
        }

        if(syncSendNum < qualifyingNode.size())
        {
            DEBUGLOG("syncSendNum:{} < qualifyingNode.size:{}", syncSendNum, qualifyingNode.size());
            syncSendNum = qualifyingNode.size();
            return -7;
        }
        DEBUGLOG("==== new sync rollback ====");
        MagicSingleton<BlockHelper>::GetInstance()->rollback_block_(rollbackBlockData_);
        return -8;
    }

    if (verify_hashes.empty())
    {
        return 0;
    }

    reqHashes.assign(verify_hashes.cbegin(), verify_hashes.cend());
    node_ids_returned.assign(nodes.cbegin(), nodes.cend());
    if(node_ids_returned.empty())
    {
        ret = -9;
        return ret;
    }
    return 0;
}

int SyncBlock::getSyncBlockData(std::vector<std::string> &node_ids_to_send, const std::vector<std::string> &reqHashes, uint64_t chainHeight)
{
    int ret = 0;
    if (reqHashes.empty() || node_ids_to_send.empty())
    {
        return 0;
    }
    std::string msgId;
    if (!dataMgrPtr.CreateWait(60, 3, msgId))
    {
        ret = -1;
        return ret;
    }

    const uint32_t SEND_MAX_NUM = 10;
    if(node_ids_to_send.size() > SEND_MAX_NUM)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(node_ids_to_send.begin(), node_ids_to_send.end(), gen);
        node_ids_to_send.resize(SEND_MAX_NUM);
    }

    for (auto &nodeId : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, nodeId))
        {
            ERRORLOG("getSyncBlockData AddResNode error");
            return -2;
        }
        sendSyncGetBlockRequest(nodeId, msgId, reqHashes);
        DEBUGLOG("new sync block from {}", nodeId);
    }
    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        if(retDatas.empty())
        {
            for(const auto& t: node_ids_to_send)
            {
                DEBUGLOG("getSyncBlockData send node id:{}", t);
            }
            ERRORLOG("wait sync block data time out send:{} recv:{}", node_ids_to_send.size(), retDatas.size());
            ret = -3;
            return ret;
        }
    }

    CBlock block;
    CBlock hash_block;
    SyncGetBlockAck ack;
    std::map<uint64_t, std::set<CBlock, BlockComparator>> syncBlockData;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        for (auto &block_raw : ack.blocks())
        {
            if (block.ParseFromString(block_raw))
            {
                std::string findKey = std::to_string(block.height()) + "_" + block.hash().substr(0, SUBSTR_LEN);
                if (reqHashes.cend() == std::find(reqHashes.cbegin(), reqHashes.cend(), findKey))
                {
                    continue;
                }
                hash_block = block;
                hash_block.clear_hash();
                hash_block.clear_sign();
                if (block.hash() != Getsha256hash(hash_block.SerializeAsString()))
                {
                    continue;
                }
                addBlockToMap(block, syncBlockData);
            }
        }
    }

    MagicSingleton<BlockHelper>::GetInstance()->AddSyncBlock(syncBlockData, global::ca::SaveType::SyncNormal);

    return 0;
}

void SyncBlock::addBlockToMap(const CBlock &block, std::map<uint64_t, std::set<CBlock, BlockComparator>> &syncBlockData)
{
    if (syncBlockData.end() == syncBlockData.find(block.height()))
    {
        syncBlockData.insert(std::make_pair(block.height(), std::set<CBlock, BlockComparator>()));
    }
    auto &value = syncBlockData.at(block.height());
    value.insert(block);
}

bool SyncBlock::checkByzantineFault(int receiveCount, int hitCount)
{
    const static std::unordered_map<int, std::set<int>> level_table = 
        {
            {1, {1}},
            {2, {2}},
            {3, {2, 3}},
            {4, {3, 4}},
            {5, {3, 4, 5}},
            {6, {4, 5, 6}},
            {7, {4, 5, 6, 7}},
            {8, {5, 6, 7, 8}},
            {9, {5, 6, 7, 8, 9}},
            {10, {6, 7, 8, 9, 10}}
        };
    auto end = level_table.end();
    auto found = level_table.find(receiveCount);
    if(found != end && found->second.find(hitCount) != found->second.end())
    {
        DEBUGLOG("byzantine success total {} hit {}", receiveCount, hitCount);
        return true;
    }
    DEBUGLOG("byzantine fail total {} hit {}", receiveCount, hitCount);
    return false;
}

bool SyncBlock::needByzantineAdjustment(uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                                        std::vector<std::string> &selectedAddr)
{
    std::vector<Node> nodes = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::vector<std::string> stakeQualifyingNodes;
    std::map<std::string, std::pair<uint64_t, std::vector<std::string>>> sumHash;

    return checkRequirementAndFilterQualifyingNodes(chainHeight, pledgeAddr, nodes, stakeQualifyingNodes)
            && get_sync_node_hash_info(nodes, stakeQualifyingNodes, sumHash)
            && _GetSelectedAddr(sumHash, selectedAddr);
}

bool SyncBlock::_GetSelectedAddr(std::map<std::string, std::pair<uint64_t, std::vector<std::string>>> &sumHash,
                                std::vector<std::string> &selectedAddr)
{
    const static double filter_scale = 0.34;
    auto receiveNum = static_cast<double>(sumHash.size());
    DEBUGLOG("receive_num: {}", receiveNum);

    for (auto iter = sumHash.begin(); iter !=  sumHash.end();)
    {
        const auto& [hits, _] = iter->second;
        double scale = static_cast<double>(hits) / receiveNum;
        if (scale < filter_scale)
        {
            DEBUGLOG("erase sum hash {} which scale is {}", iter->first.substr(0, 6), scale);
            iter = sumHash.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    DEBUGLOG("sumHash size: {}", sumHash.size());
    if (sumHash.size() == 2)
    {
        auto& [firstHits, firstAddrs] = sumHash.begin()->second;
        auto& [lastHits, lastAddrs] = sumHash.rbegin()->second;
        DEBUGLOG("first hash {} hits {}; last hash {} hits {}",
                 sumHash.begin()->first.substr(0, 6), firstHits, sumHash.rbegin()->first.substr(0, 6), lastHits);
        if ((static_cast<double>(firstHits) / receiveNum) >= (static_cast<double>(lastHits) / receiveNum))
        {
            std::swap(selectedAddr, firstAddrs);
        }
        else
        {
            std::swap(selectedAddr, lastAddrs);
        }
        return true;
    }

    return false;
}

bool SyncBlock::get_sync_node_hash_info(const std::vector<Node> &nodes, const std::vector<std::string> &stakeQualifyingNodes,
                                       std::map<std::string, std::pair<uint64_t, std::vector<std::string>>> sumHash)
{
    uint64_t byzantineFaultTolerance = 5;
    if (nodes.size() < global::ca::MIN_SYNC_QUAL_NODES)
    {
        byzantineFaultTolerance = 0;
    }
    uint64_t recvRequirement = stakeQualifyingNodes.size() - byzantineFaultTolerance;
    DEBUGLOG("recvRequirement size: {}", recvRequirement);

    std::string msgId;
    if (!dataMgrPtr.CreateWait(60, recvRequirement, msgId))
    {
        ERRORLOG("Create wait fail");
        return false;
    }
    for (const auto& addr : stakeQualifyingNodes)
    {
        if(!dataMgrPtr.AddResNode(msgId, addr))
        {
            ERRORLOG("GetSyncNodeSumhashInfo AddResNode fail");
            return false;
        }
        DEBUGLOG("get sync node hash from {}", addr);
        sendSyncNodeHashRequest(addr, msgId);
    }

    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        ERRORLOG("wait sync node hash time out send:{} recv:{}", stakeQualifyingNodes.size(), retDatas.size());
        return false;
    }

    SyncNodeHashAck ack;
    for (const auto& retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            ERRORLOG("SyncNodeHashAck parse fail");
            continue;
        }
        const auto& ret_hash = ack.hash();
        if (sumHash.find(ret_hash) == sumHash.end())
        {
            sumHash[ret_hash] = {0, std::vector<std::string>()};
        }
        auto& [hits, addrs] = sumHash.at(ret_hash);
        hits += 1;
        addrs.push_back(ack.self_node_id());
    }
    return true;
}


int SyncBlock::fetchRollbackBlock(const std::vector<std::string> &node_ids_to_send, uint64_t syncInitHeight, uint64_t endSyncHeight_, uint64_t nodeSelfHeight, std::vector<CBlock> &retBlocks)
{
    DEBUGLOG("getSyncBlockHashVerification sendNodeIs num: {}\tstartSyncHeight: {}\tendSyncHeight: {}", node_ids_to_send.size() ,syncInitHeight, endSyncHeight_);
    DEBUGLOG("getSyncBlockHashVerification nodeSelfHeight: {}", nodeSelfHeight);

    retBlocks.clear();
    size_t sendNum = node_ids_to_send.size();
    double acceptanceRate = 0.8;

    int ret = 0;
    std::string msgId;
    uint64_t successCounter = 0;
    if (!dataMgrPtr.CreateWait(60, sendNum * acceptanceRate, msgId))
    {
        ret = -1;
        return ret;
    }

    for (auto &nodeId : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, nodeId))
        {
            ERRORLOG("getSyncBlockHashVerification AddResNode error");
            return -2;
        }
        DEBUGLOG("getSyncBlockHashVerification new sync get block hash from {}", nodeId);
        chainSyncGetBlockHeightAndHashRequest(nodeId, msgId, syncInitHeight, endSyncHeight_);
    }

    std::vector<std::string> retDatas;
    if (!dataMgrPtr.WaitData(msgId, retDatas))
    {
        if (retDatas.size() < sendNum * acceptanceRate - 1)
        {
            ERRORLOG("getSyncBlockHashVerification wait sync height time out send:{}\trecv:{}\tsendNum * acceptanceRate - 1{}", sendNum, retDatas.size(), sendNum * acceptanceRate - 1);
            ret = -3;
            return ret;
        }
    }

    //key:blockHash    pair.first:height    pair.second:node list
    std::map<uint64_t, std::map<std::string, std::set<std::string> >> consensusMap;
    DBReader dbReader;

    std::set<uint64_t> blockHeights;

    SyncGetBlockHeightAndHashAck ack;
    for (auto &retData : retDatas)
    {
        ack.Clear();
        if (!ack.ParseFromString(retData))
        {
            continue;
        }
        if(ack.code() != 0)
        {
            ERRORLOG("getSyncBlockHashVerification startSyncHeitght:{}, endSyncHeight_:{}, nodeSelfHeight:{} ack.code():{}", syncInitHeight, endSyncHeight_, nodeSelfHeight, ack.code());
            continue;
        }

        std::vector<std::string> data_key;
        for(auto& heightAndHash: ack.block_hashes())
        {
            data_key.clear();
            StringUtil::SplitString(heightAndHash, "_", data_key);
            if(data_key.size() != 2)
            {
                continue;
            }

            uint64_t height = std::stoull(data_key[0]);
            std::string hash = data_key[1];
            blockHeights.insert(height);

            auto findHeight = consensusMap.find(height);
            if(findHeight == consensusMap.end())
            {
                consensusMap[height][hash] = {ack.self_node_id()};
            }
            else 
            {
                auto findHash = findHeight->second.find(hash);
                if(findHash == findHeight->second.end())
                {
                    findHeight->second[hash] = {ack.self_node_id()};
                }
                else
                {
                    findHash->second.insert(ack.self_node_id());
                }
            }
        }
    }

    for(const auto& height: blockHeights)
    {
        std::vector<std::string> selfBlockHashes_;
        auto dbRet = dbReader.getBlockHashesByBlockHeight(height, height, selfBlockHashes_);
        if(DBStatus::DB_SUCCESS != dbRet && DBStatus::DB_NOT_FOUND != dbRet)
        {
            ret = -4;
            DEBUGLOG("getSyncBlockHashVerification blockHeights getBlockHashesByBlockHeight failed, dbRet:{}", dbRet);
            return ret;
        }
        for(auto& currentBlockHash: selfBlockHashes_)
        {
            std::string substr = currentBlockHash.substr(0, SUBSTR_LEN);
            auto findHeight = consensusMap.find(height);
            if(findHeight == consensusMap.end())
            {
                continue;
            }
            auto findHash = findHeight->second.find(substr);
            if(findHash == findHeight->second.end())
            {
                findHeight->second[substr] = {"temp"};
            }
            else
            {
                findHash->second.insert("temp");
            }
        }
    }

    int verifyNum = (retDatas.size() + 1) * 0.51;

    std::set<std::string> nodeIds;
    std::vector<std::string> needToSyncBlockHashes;
    std::map<uint64_t, std::set<CBlock, BlockComparator>> rollbackBlockData_;

    for(auto& heightAndHash: consensusMap)
    {
        std::vector<std::string> selfBlockHashes_;
        auto dbRet = dbReader.getBlockHashesByBlockHeight(heightAndHash.first, heightAndHash.first, selfBlockHashes_);
        if(DBStatus::DB_SUCCESS != dbRet && DBStatus::DB_NOT_FOUND != dbRet)
        {
            ret = -5;
            DEBUGLOG("getSyncBlockHashVerification getBlockHashesByBlockHeight failed, dbRet:{}", dbRet);
            return ret;
        }

        for(auto& selfBlockHashValue: selfBlockHashes_)
        {
            selfBlockHashValue = std::move(selfBlockHashValue.substr(0, SUBSTR_LEN));
        }

        for(auto& hash_and_node: heightAndHash.second)
        {
            if(hash_and_node.second.size() >= verifyNum)
            {   
                continue;
            }
            else 
            {
                auto findHash = std::find(selfBlockHashes_.begin(), selfBlockHashes_.end(), hash_and_node.first);
                if(findHash == selfBlockHashes_.end())
                {
                    continue;
                }

                std::string strblock;
                CBlock block;
                std::vector<std::string> blockHashes;
                dbRet = dbReader.getBlockHashesByBlockHeight(heightAndHash.first, heightAndHash.first, blockHashes);
                if(DBStatus::DB_SUCCESS != dbRet)
                {
                    ERRORLOG("getSyncBlockHashVerification getBlockHashesByBlockHeight failed, dbRet:{}", dbRet);
                    continue;
                }

                std::string robackHash;
                for(auto& hash: blockHashes)
                {
                    if(hash.substr(0, SUBSTR_LEN) == hash_and_node.first)
                    {
                        robackHash = std::move(hash);
                        break;
                    }
                }

                if(robackHash.empty())
                {
                    ERRORLOG("getSyncBlockHashVerification rollback hash not found");
                    return -6;
                }

                dbRet = dbReader.getBlockByBlockHash(robackHash, strblock);
                if(DBStatus::DB_SUCCESS != dbRet)
                {
                    ERRORLOG("getSyncBlockHashVerification getBlockByBlockHash failed, dbRet:{}", dbRet);
                    continue;
                }
                if(!block.ParseFromString(strblock))
                {
                    continue;
                }
                uint64_t nowTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
                if(block.time() < nowTime - RollBackTime)
                {
                    DEBUGLOG("getSyncBlockHashVerification rollback sendNum:{}\tretDatas num:{}\tverifyNum:{}\t,hash_and_node.second.size():{}",
                        sendNum, retDatas.size(), verifyNum, hash_and_node.second.size());
                    DEBUGLOG("getSyncBlockHashVerification rollback addBlockToMap rollback block height: {}\tblock hash:{}",block.height(), block.hash());
                    retBlocks.emplace_back(block);
                }
            }
        }
    }

    return 0;
}




bool SyncBlock::checkRequirementAndFilterQualifyingNodes(uint64_t chainHeight, const std::vector<std::string> &pledgeAddr,
                                                         const std::vector<Node> &nodes,
                                                         std::vector<std::string> &stakeQualifyingNodes)
{
    const static uint32_t is_chain_height_exceeded = 3;
    uint32_t m_higherThanChainHeightCount = 0;

    for (const auto& node : nodes)
    {
        if (node.height > chainHeight)
        {
            m_higherThanChainHeightCount++;
            if (m_higherThanChainHeightCount > is_chain_height_exceeded)
            {
                DEBUGLOG("chain height isn't top hight in the network");
                return false;
            }
        }
        else if (node.height == chainHeight)
        {
            const auto& node_addr = node.address;
            if (find(pledgeAddr.cbegin(), pledgeAddr.cend(), node_addr) != pledgeAddr.cend())
            {
                stakeQualifyingNodes.push_back(node_addr);
            }
        }
    }
    DEBUGLOG("stakeQualifyingNodes size: ", stakeQualifyingNodes.size());
    return true;
}

void fastSyncGetHashRequest(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    FastSyncGetHashReq req;
    req.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    req.set_msg_id(msgId);
    req.set_start_height(startHeight);
    req.set_end_height(endHeight);
    NetSendMessage<FastSyncGetHashReq>(nodeId, req, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);

}

void fastSyncHashAck(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    if(startHeight > endHeight)
    {
        return;
    }
    if ((endHeight - startHeight) > 100000)
    {
        return;
    }
    FastSyncGetHashAck ack;
    ack.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    ack.set_msg_id(msgId);
    uint64_t nodeBlockHeight = 0;
    if (DBStatus::DB_SUCCESS != DBReader().getBlockTop(nodeBlockHeight))
    {
        ERRORLOG("getBlockTop error");
        return;
    }
    ack.set_node_block_height(nodeBlockHeight);

    std::vector<FastSyncBlockHashs> height_hashes_of_blocks;
    if (getHeightBlockHash(startHeight, endHeight, height_hashes_of_blocks))
    {
        for(auto &blockHeightHash : height_hashes_of_blocks)
        {
            auto heightHash = ack.add_hashs();
            heightHash->set_height(blockHeightHash.height());
            for(auto &hash : blockHeightHash.hashs())
            {
                heightHash->add_hashs(hash);
            }
        }
        NetSendMessage<FastSyncGetHashAck>(nodeId, ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    }
}

void sendFastSyncGetBlockRequest(const std::string &nodeId, const std::string &msgId, const std::vector<FastSyncBlockHashs> &requestHashs)
{
    FastSyncGetBlockReq req;
    req.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    req.set_msg_id(msgId);
    for(auto &blockHeightHash : requestHashs)
    {
        auto heightHash = req.add_hashs();
        heightHash->set_height(blockHeightHash.height());
        for(auto &hash : blockHeightHash.hashs())
        {
            heightHash->add_hashs(hash);
        }
    }

    NetSendMessage<FastSyncGetBlockReq>(nodeId, req, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

void sendFastSyncBlockAcknowledge(const std::string &nodeId, const std::string &msgId, const std::vector<FastSyncBlockHashs> &requestHashs)
{
    FastSyncGetBlockAck ack;
    ack.set_msg_id(msgId);
    DBReader dbReader;
    std::vector<std::string> blockHashes;
    for(const auto& heightToHashes : requestHashs)
    {
        std::vector<std::string> dbHashs;
        if (DBStatus::DB_SUCCESS != dbReader.getBlockHashsByBlockHeight(heightToHashes.height(), dbHashs))
        {
            return ;
        }
        for(auto& dbHash : dbHashs)
        {
            auto hashs = heightToHashes.hashs();
            auto end = hashs.end();
            auto found = find_if(hashs.begin(), hashs.end(), [&dbHash](const std::string& hash){return dbHash == hash;});
            if(found != end)
            {
                blockHashes.push_back(dbHash);
            }
        }
    }

    std::vector<std::string> blocks;
    if (DBStatus::DB_SUCCESS != dbReader.getBlocksByBlockHash(blockHashes, blocks))
    {
        return;
    }
    for (auto &block_raw : blocks)
    {
        CBlock block;
        if(!block.ParseFromString(block_raw))
        {
            return;
        }
        auto height = block.height();
        auto syncBlocks = ack.mutable_blocks();
        auto found = std::find_if(syncBlocks->begin(), syncBlocks->end(), [height](const FastSyncBlock& sync_block){return sync_block.height() == height;});
        if(found == syncBlocks->end())
        {
            auto ack_block = ack.add_blocks();
            ack_block->set_height(height);
            ack_block->add_blocks(block_raw);
        }
        else
        {
                found->add_blocks(block_raw);
        }
    }

    NetSendMessage<FastSyncGetBlockAck>(nodeId, ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

int processFastSyncGetHashRequest(const std::shared_ptr<FastSyncGetHashReq> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("processFastSyncGetHashRequest verifyPeerNodeIdRequest error");
        return -1;
    }
    fastSyncHashAck(msg->self_node_id(), msg->msg_id(), msg->start_height(), msg->end_height());
    return 0;
}

int handleFastSyncHashAcknowledge(const std::shared_ptr<FastSyncGetHashAck> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("handleFastSyncHashAcknowledge verifyPeerNodeIdRequest error");
        return -1;
    }

    dataMgrPtr.waitDataToAdd(msg->msg_id(), msg->self_node_id(), msg->SerializeAsString());
    return 0;
}

int handleFastSyncBlockRequest(const std::shared_ptr<FastSyncGetBlockReq> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("handleFastSyncBlockRequest verifyPeerNodeIdRequest error");
        return -1;
    }

    std::vector<FastSyncBlockHashs> heightToHashes;
    for(int i = 0; i < msg->hashs_size(); ++i)
    {
        auto& hash = msg->hashs(i);
        heightToHashes.push_back(hash);
    }
    sendFastSyncBlockAcknowledge(msg->self_node_id(), msg->msg_id(), heightToHashes);  
    return 0;
}

int handleFastSyncBlockAcknowledge(const std::shared_ptr<FastSyncGetBlockAck> &msg, const MsgData &msgdata)
{
    Node node;
    if(!MagicSingleton<PeerNode>::GetInstance()->locateNodeByFd(msgdata.fd, node))
    {
        ERRORLOG("Invalid message ");
        return -1;
    }
    dataMgrPtr.waitDataToAdd(msg->msg_id(), node.address, msg->SerializeAsString());
    return 0;
}

void sendSyncGetSumHashRequest(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    SyncGetSumHashReq req;
    req.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    req.set_msg_id(msgId);
    req.set_start_height(startHeight);
    req.set_end_height(endHeight);
    NetSendMessage<SyncGetSumHashReq>(nodeId, req, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

void sendSyncSumHashAcknowledgement(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    if(startHeight > endHeight)
    {
        return;
    }
    if (endHeight - startHeight > 1000)
    {
        return;
    }
    SyncGetSumHashAck ack;
    ack.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    DBReader dbReader;
    uint64_t nodeSelfHeight = 0;
    if (0 != dbReader.getBlockTop(nodeSelfHeight))
    {
        ERRORLOG("getBlockTop(txn, top)");
        return;
    }
    ack.set_node_block_height(nodeSelfHeight);
    ack.set_msg_id(msgId);

    uint64_t end = endHeight > nodeSelfHeight ? nodeSelfHeight : endHeight;
    std::string hash;
    uint64_t j = 0;
    std::vector<std::string> block_hashes;
    for (uint64_t i = startHeight; j <= end; ++i)
    {
        j = i + 1;
        j = j > end ? end : j;
        block_hashes.clear();
        hash.clear();
        if (getHeightBlockHash(i, i, block_hashes) && calculateSumHeightHash(block_hashes, hash))
        {
            auto syncSumHashValue = ack.add_sync_sum_hashes();
            syncSumHashValue->set_start_height(i);
            syncSumHashValue->set_end_height(i);
            syncSumHashValue->set_hash(hash);
        }
        else
        {
            return;
        }
        if(i == j) break;
    }
    NetSendMessage<SyncGetSumHashAck>(nodeId, ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

void sendSyncGetHeightHashRequest(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    SyncGetHeightHashReq req;
    req.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    req.set_msg_id(msgId);
    req.set_start_height(startHeight);
    req.set_end_height(endHeight);
    NetSendMessage<SyncGetHeightHashReq>(nodeId, req, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

void sendSyncGetHeightHashAcknowledgment(SyncGetHeightHashAck& ack,const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    if(startHeight > endHeight)
    {
        ack.set_code(-1);
        return;
    }
    if (endHeight - startHeight > 500)
    {
        ack.set_code(-2);
        return;
    }
    ack.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    DBReader dbReader;
    uint64_t nodeSelfHeight = 0;
    if (0 != dbReader.getBlockTop(nodeSelfHeight))
    {
        ack.set_code(-3);
        ERRORLOG("getBlockTop(txn, top)");
        return;
    }
    ack.set_msg_id(msgId);
    std::vector<std::string> blockHashes;
    if(endHeight > nodeSelfHeight)
    {
        endHeight = nodeSelfHeight;
    }
    if(startHeight > endHeight)
    {
        ack.set_code(-4);
        return;
    }
    if (DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(startHeight, endHeight, blockHashes))
    {
        ack.set_code(-5);
        return;
    }
    
    std::sort(blockHashes.begin(), blockHashes.end());
    for (const auto& hash : blockHashes)
    {
        ack.add_block_hashes(hash);
    }
    ack.set_code(0);
}

void chainSyncGetBlockHeightAndHashRequest(const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    SyncGetBlockHeightAndHashReq req;
    req.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    req.set_msg_id(msgId);
    req.set_start_height(startHeight);
    req.set_end_height(endHeight);
    NetSendMessage<SyncGetBlockHeightAndHashReq>(nodeId, req, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

void getBlockHeightAndHashAcknowledge(SyncGetBlockHeightAndHashAck& ack,const std::string &nodeId, const std::string &msgId, uint64_t startHeight, uint64_t endHeight)
{
    if(startHeight > endHeight)
    {
        ack.set_code(-1);
        return;
    }
    if (endHeight - startHeight > 500)
    {
        ack.set_code(-2);
        return;
    }
    ack.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    DBReader dbReader;
    uint64_t nodeSelfHeight = 0;
    if (0 != dbReader.getBlockTop(nodeSelfHeight))
    {
        ack.set_code(-3);
        ERRORLOG("getBlockTop(txn, top)");
        return;
    }
    ack.set_msg_id(msgId);
    std::vector<std::string> blockHashes;
    if(endHeight > nodeSelfHeight)
    {
        endHeight = nodeSelfHeight;
    }
    if(startHeight > endHeight)
    {
        ack.set_code(-4);
        return;
    }

    for(auto i = startHeight; i <= endHeight; i++)
    {
        std::vector<std::string> blockHashesTemp;
        if (DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(i, i, blockHashesTemp))
        {
            ack.set_code(-5);
            return;
        }

        for(auto& hash : blockHashesTemp)
        {
            std::string blockHash = std::to_string(i) + "_" + hash.substr(0, SUBSTR_LEN);
            blockHashes.emplace_back(std::move(blockHash));
        }
    }
    
    std::sort(blockHashes.begin(), blockHashes.end());
    for (const auto& hash : blockHashes)
    {
        ack.add_block_hashes(hash);
    }
    ack.set_code(0);
}

void sendSyncGetBlockRequest(const std::string &nodeId, const std::string &msgId, const std::vector<std::string> &reqHashes)
{
    SyncGetBlockReq req;
    req.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    req.set_msg_id(msgId);
    for (const auto& hash : reqHashes)
    {
        req.add_block_hashes(hash);
    }
    NetSendMessage<SyncGetBlockReq>(nodeId, req, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

void sendSyncGetBlockAcknowledgement(const std::string &nodeId, const std::string &msgId, const std::map<uint64_t, std::vector<std::string>> &blockHashMap)
{

    if (blockHashMap.size() > 2000)
    {
        return;
    }
    SyncGetBlockAck ack;
    ack.set_msg_id(msgId);
    DBReader dbReader;
    std::vector<std::string> reqHashes;

    for(auto &it : blockHashMap)
    {
        std::vector<std::string> blockHashes;
        if(DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(it.first, it.first, blockHashes))
        {
            ERRORLOG("getBlockHashesByBlockHeight error, height: {}", it.first);
            return;
        }

        for(auto &syncHash : it.second)
        {
            DEBUGLOG("opopop syncHash: {}, blockHashes size: {}", syncHash, blockHashes.size());
            for(auto &hash : blockHashes)
            {
                if(syncHash == hash.substr(0, SUBSTR_LEN))
                {
                    DEBUGLOG("opopop hash: {}", hash)
                    reqHashes.emplace_back(hash);
                }
            }
        }
    }

    std::vector<std::string> blocks;
    if (DBStatus::DB_SUCCESS != dbReader.getBlocksByBlockHash(reqHashes, blocks))
    {
        return;
    }
    for (auto &block : blocks)
    {
        ack.add_blocks(block);
    }
    NetSendMessage<SyncGetBlockAck>(nodeId, ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

int processSyncSumHashRequest(const std::shared_ptr<SyncGetSumHashReq> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("processSyncSumHashRequest verifyPeerNodeIdRequest error");
        return -1;
    }

    sendSyncSumHashAcknowledgement(msg->self_node_id(), msg->msg_id(), msg->start_height(), msg->end_height());
    return 0;
}

int syncGetSumHashAcknowledge(const std::shared_ptr<SyncGetSumHashAck> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("syncGetSumHashAcknowledge verifyPeerNodeIdRequest error");
        return -1;
    }

    dataMgrPtr.waitDataToAdd(msg->msg_id(), msg->self_node_id(), msg->SerializeAsString());
    return 0;
}

int handleSyncGetHeightHashRequest(const std::shared_ptr<SyncGetHeightHashReq> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("handleSyncGetHeightHashRequest verifyPeerNodeIdRequest error");
        return -1;
    }

    SyncGetHeightHashAck ack;
    sendSyncGetHeightHashAcknowledgment(ack,msg->self_node_id(), msg->msg_id(), msg->start_height(), msg->end_height());
    NetSendMessage<SyncGetHeightHashAck>(msg->self_node_id(), ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    return 0;
}

int processSyncHeightHashAcknowledgement(const std::shared_ptr<SyncGetHeightHashAck> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("processSyncHeightHashAcknowledgement verifyPeerNodeIdRequest error");
        return -1;
    }

    dataMgrPtr.waitDataToAdd(msg->msg_id(), msg->self_node_id(), msg->SerializeAsString());
    return 0;
}

int syncBlockHeightAndHashRequest(const std::shared_ptr<SyncGetBlockHeightAndHashReq> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("syncBlockHeightAndHashRequest verifyPeerNodeIdRequest error");
        return -1;
    }

    SyncGetBlockHeightAndHashAck ack;
    getBlockHeightAndHashAcknowledge(ack,msg->self_node_id(), msg->msg_id(), msg->start_height(), msg->end_height());
    NetSendMessage<SyncGetBlockHeightAndHashAck>(msg->self_node_id(), ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    return 0;
}

int syncBlockHeightAndHashAck(const std::shared_ptr<SyncGetBlockHeightAndHashAck> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("syncBlockHeightAndHashAck verifyPeerNodeIdRequest error");
        return -1;
    }

    dataMgrPtr.waitDataToAdd(msg->msg_id(), msg->self_node_id(), msg->SerializeAsString());
    return 0;
}


int handleSynchronizationBlockRequest(const std::shared_ptr<SyncGetBlockReq> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("handleSynchronizationBlockRequest verifyPeerNodeIdRequest error");
        return -1;
    }

    std::map<uint64_t, std::vector<std::string>> blockHashMap;
    std::vector<std::string> data_key;
    for (const auto& hash : msg->block_hashes())
    {
        data_key.clear();
        StringUtil::SplitString(hash, "_", data_key);
        if(data_key.size() != 2)
        {
            continue;
        }
        uint64_t height = std::stoull(data_key[0]);
        auto find = blockHashMap.find(height);
        if(find == blockHashMap.end())
        {
            blockHashMap[height] = std::vector<std::string>{data_key[1]};
        }
        else 
        {
            find->second.emplace_back(data_key[1]);
        }
    }
    sendSyncGetBlockAcknowledgement(msg->self_node_id(), msg->msg_id(), blockHashMap);
    return 0;
}

int handleSyncGetBlockAcknowledge(const std::shared_ptr<SyncGetBlockAck> &msg, const MsgData &msgdata)
{
    Node node;
    if(!MagicSingleton<PeerNode>::GetInstance()->locateNodeByFd(msgdata.fd, node))
    {
        ERRORLOG("Invalid message ");
        return -1;
    }
    dataMgrPtr.waitDataToAdd(msg->msg_id(), node.address, msg->SerializeAsString());
    return 0;
}

void sendSyncSumHashRequest(const std::string &nodeId, const std::string &msgId, const std::vector<uint64_t>& heights)
{
    SyncFromZeroGetSumHashReq req;
    req.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    req.set_msg_id(msgId);
    for(auto height : heights)
    {
        req.add_heights(height);
    }
    NetSendMessage<SyncFromZeroGetSumHashReq>(nodeId, req, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);

}

void syncSumHashAcknowledge(const std::string &nodeId, const std::string &msgId, const std::vector<uint64_t>& heights)
{
    DEBUGLOG("handle FromZeroSyncGetSumHashAck from {}", nodeId);
    SyncFromZeroGetSumHashAck ack;
    DBReader dbReader;
    
    for(auto height : heights)
    {
        DEBUGLOG("FromZeroSyncGetSumHashAck get height {}", height);
        std::string sumHash;
        if (DBStatus::DB_SUCCESS != dbReader.getSumHashByHeight(height, sumHash))
        {
            DEBUGLOG("fail to get sum hash height at height {}", height);
            continue;
        }
        SyncFromZeroSumHash* hashItemSum = ack.add_sum_hashes();
        hashItemSum->set_height(height);
        hashItemSum->set_hash(sumHash);
    }

    DEBUGLOG("sum hash size {}:{}", ack.sum_hashes().size(), ack.sum_hashes_size());
    if (ack.sum_hashes_size() == 0) 
    {
        ack.set_code(0);
    }
    else
    {
        ack.set_code(1);
    }
    ack.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    ack.set_msg_id(msgId);
    DEBUGLOG("SyncFromZeroGetSumHashAck: id:{} , msgId:{}", nodeId, msgId);
    NetSendMessage<SyncFromZeroGetSumHashAck>(nodeId, ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

void sendFromZeroSyncGetBlockRequest(const std::string &nodeId, const std::string &msgId, uint64_t height)
{
    SyncFromZeroGetBlockReq req;
    req.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    req.set_msg_id(msgId);
    req.set_height(height);
    NetSendMessage<SyncFromZeroGetBlockReq>(nodeId, req, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

void zeroSyncBlockAcknowledge(const std::string &nodeId, const std::string &msgId, uint64_t height)
{
    if(height < global::ca::hashRangeSum)
    {
        DEBUGLOG("sum height {} less than sum hash range", height);
        return;
    }
    SyncFromZeroGetBlockAck ack;
    DBReader dbReader;
    std::vector<std::string> blockhashes;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockHashesByBlockHeight(height - global::ca::hashRangeSum + 1, height, blockhashes))
    {
        DEBUGLOG("getBlockHashesByBlockHeight at height {}:{} fail", height - global::ca::hashRangeSum + 1, height);
        return;
    }
    std::vector<std::string> blockraws;
    if (DBStatus::DB_SUCCESS != dbReader.getBlocksByBlockHash(blockhashes, blockraws))
    {
        DEBUGLOG("getBlocksByBlockHash fail");
        return;
    }

    for (auto &blockraw : blockraws)
    {
        ack.add_blocks(blockraw);
    }
    ack.set_height(height);
    ack.set_msg_id(msgId);
    ack.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    DEBUGLOG("response sum hash blocks at height {} to {}", height, nodeId);
    NetSendMessage<SyncFromZeroGetBlockAck>(nodeId, ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

int syncGetSumHashRequest(const std::shared_ptr<SyncFromZeroGetSumHashReq> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("syncGetSumHashRequest verifyPeerNodeIdRequest error");
        return -1;
    }

    std::vector<uint64_t> heights;
    for(auto height : msg->heights())
    {
        heights.push_back(height);
    }
    DEBUGLOG("SyncFromZeroGetSumHashReq: id:{}, msgId:{}", msg->self_node_id(), msg->msg_id());
    syncSumHashAcknowledge(msg->self_node_id(), msg->msg_id(), heights);
    return 0;
}

int zeroSyncSumHashAcknowledge(const std::shared_ptr<SyncFromZeroGetSumHashAck> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("zeroSyncSumHashAcknowledge verifyPeerNodeIdRequest error");
        return -1;
    }
    DEBUGLOG("msgid:{}, nodeid:{}",msg->msg_id(), msg->self_node_id());
    dataMgrPtr.waitDataToAdd(msg->msg_id(), msg->self_node_id(), msg->SerializeAsString());
    return 0;
}

int handleSyncGetBlockRequest(const std::shared_ptr<SyncFromZeroGetBlockReq> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("handleSyncGetBlockRequest verifyPeerNodeIdRequest error");
        return -1;
    }

    zeroSyncBlockAcknowledge(msg->self_node_id(), msg->msg_id(), msg->height());
    return 0;
}

int handleZeroSyncBlockAcknowledge(const std::shared_ptr<SyncFromZeroGetBlockAck> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("handleZeroSyncBlockAcknowledge verifyPeerNodeIdRequest error");
        return -1;
    }

    dataMgrPtr.waitDataToAdd(msg->msg_id(), msg->self_node_id(), msg->SerializeAsString());
    return 0;
}

void sendSyncNodeHashRequest(const std::string &nodeId, const std::string &msgId)
{
    SyncNodeHashReq req;
    req.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    req.set_msg_id(msgId);
    NetSendMessage<SyncNodeHashReq>(nodeId, req, net_com::Compress::kCompressDisabled, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

void sendSynchronizationNodeHashConfirmation(const std::string &nodeId, const std::string &msgId)
{
    DEBUGLOG("handle sendSynchronizationNodeHashConfirmation from {}", nodeId);
    SyncNodeHashAck ack;
    ack.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    ack.set_msg_id(msgId);

    DBReadWriter reader;
    uint64_t nodeBlockHeight = 0;
    if (DBStatus::DB_SUCCESS != reader.getBlockTop(nodeBlockHeight))
    {
        ERRORLOG("getBlockTop error");
        return;
    }
    std::string sumHash;
    if (!ca_algorithm::calculate_height_sum_hash(nodeBlockHeight - syncHeightCount, nodeBlockHeight, reader, sumHash))
    {
        ERRORLOG("calculate_height_sum_hash error");
        return;
    }
    ack.set_hash(sumHash);
    NetSendMessage<SyncNodeHashAck>(nodeId, ack, net_com::Compress::kCompressDisabled, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

int syncNodeHashRequest(const std::shared_ptr<SyncNodeHashReq> &msg, const MsgData &msgdata)
{

    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("syncNodeHashRequest verifyPeerNodeIdRequest error");
        return -1;
    }

    sendSynchronizationNodeHashConfirmation(msg->self_node_id(), msg->msg_id());
    return 0;
}

int processSyncNodeHashAcknowledgment(const std::shared_ptr<SyncNodeHashAck> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        ERRORLOG("processSyncNodeHashAcknowledgment verifyPeerNodeIdRequest error");
        return -1;
    }

    dataMgrPtr.waitDataToAdd(msg->msg_id(), msg->self_node_id(), msg->SerializeAsString());
    return 0;
}


int sendBlockByUtxoAcknowledge(const std::string &utxo, const std::string &addr, const std::string &msgId)
{
    DEBUGLOG("handle get missing block utxo {}",utxo);
    DBReader dbReader;

    std::string strBlockHash = "";
    if (DBStatus::DB_SUCCESS != dbReader.getBlockHashByTransactionHash(utxo, strBlockHash))
    {
        ERRORLOG("getBlockHashByTransactionHash fail!");
        return -1;
    }

    std::string blockstr = "";
    if (DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(strBlockHash, blockstr))
    {
        ERRORLOG("getBlockByBlockHash fail!");
        return -2;
    }
    if(blockstr == "")
    {
        ERRORLOG("blockstr is empty fail!");
        return -3;
    }
    GetBlockByUtxoAck ack;
    ack.set_addr(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    ack.set_utxo(utxo);
    ack.set_block_raw(blockstr);
    ack.set_msg_id(msgId);

    NetSendMessage<GetBlockByUtxoAck>(addr, ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    return 0;
}

int handleBlockByUtxoRequest(const std::shared_ptr<GetBlockByUtxoReq> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->addr()))
    {
        return -1;
    }
    sendBlockByUtxoAcknowledge(msg->utxo(), msg->addr(),msg->msg_id());
    return 0;
}


int handleBlockByUtxoAcknowledgment(const std::shared_ptr<GetBlockByUtxoAck> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->addr()))
    {
        return -1;
    }

    dataMgrPtr.waitDataToAdd(msg->msg_id(), msg->addr(), msg->SerializeAsString());
    return 0;
}

int SendBlockByHashAck(const std::map<std::string, bool> &missingHashs, const std::string &addr, const std::string &msgId)
{
    DBReader dbReader;
    GetBlockByHashAck ack;
    for(const auto& it : missingHashs)
    {
        std::string strBlockHash = "";
        if(it.second)
        {
            if (DBStatus::DB_SUCCESS != dbReader.getBlockHashByTransactionHash(it.first, strBlockHash))
            {
                ERRORLOG("getBlockHashByTransactionHash fail!");
                return -1;
            }
        }
        else
        {
            strBlockHash = it.first;
        }
        std::string blockstr = "";
        if (DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(strBlockHash, blockstr))
        {
            ERRORLOG("getBlockByBlockHash fail!");
            return -2;
        }
        if(blockstr == "")
        {
            ERRORLOG("blockstr is empty fail!");
            return -3;
        }
        auto block = ack.add_blocks();
        block->set_hash(it.first);
        block->set_tx_or_block(it.second);
        block->set_block_raw(blockstr);
    }
    
    ack.set_addr(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    ack.set_msg_id(msgId);

    NetSendMessage<GetBlockByHashAck>(addr, ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
    return 0; 
}

int blockByHashRequest(const std::shared_ptr<GetBlockByHashReq> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->addr()))
    {
        return -1;
    }
    std::map<std::string, bool> missingHashs;
    for(const auto& it : msg->missinghashs())
    {
        missingHashs[it.hash()] = it.tx_or_block();
    }
    SendBlockByHashAck(missingHashs, msg->addr(), msg->msg_id());
    return 0;
}

int handleBlockByHashAcknowledgment(const std::shared_ptr<GetBlockByHashAck> &msg, const MsgData &msgdata)
{
     if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->addr()))
    {
        return -1;
    }

    dataMgrPtr.waitDataToAdd(msg->msg_id(), msg->addr(), msg->SerializeAsString());
    return 0;
}

int processCheckVinRequest(const std::shared_ptr<CheckVinReq> &msg, const MsgData &msgdata)
{
    DEBUGLOG("handle check vin req");

    std::set<std::string> utxo_hashes;
    for (const auto &it : msg->utxohash())
    {
        utxo_hashes.insert(it);
    }

    CheckVinAck ack;
    ack.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    ack.set_msg_id(msg->msg_id());

    DBReader reader;
    std::vector<std::string> utxoHashesList;
    auto ret = reader.getUtxoHashsByAddress(msg->self_node_id(), utxoHashesList);
    if (ret != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("getUtxoHashsByAddress error");
        ack.set_utxo_ok(false);
    }
    else
    {
        bool allContained = true;
        for (const auto &hash : utxo_hashes)
        {
            if (std::find(utxoHashesList.begin(), utxoHashesList.end(), hash) == utxoHashesList.end())
            {
                allContained = false;
                break;
            }
        }

        if (!allContained)
        {
            ack.set_utxo_ok(false);
        }

        ack.set_utxo_ok(true);
    }

    NetSendMessage<CheckVinAck>(msg->self_node_id(), ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);

    return 0;
}

int checkVinAcknowledge(const std::shared_ptr<CheckVinAck> &msg, const MsgData &msgdata)
{
    DEBUGLOG("handle check vin ack");
    dataMgrPtr.waitDataToAdd(msg->msg_id(), msg->self_node_id(), msg->SerializeAsString());
    return 0;
}