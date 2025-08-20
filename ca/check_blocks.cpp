#include <cstdint>
#include <fstream> 
#include <mutex>
#include <string>
#include <utility>

#include "ca/algorithm.h"
#include "ca/transaction.h"
#include "ca/check_blocks.h"
#include "ca/block_helper.h"
#include "ca/sync_block.h"

#include "include/logging.h"
#include "common/global_data.h"

CheckBlocks::CheckBlocks()
{
    _Init();
    isRunning = false;
}

void CheckBlocks::_Init()
{
    DBReader dbReader;
    auto ret = dbReader.getTopThousandSumHash(this->topBlockHeight);
    if(ret != DBStatus::DB_SUCCESS)
    {
        this->topBlockHeight = 0;
        this->topBlockHash = "";
    }

    setTemporaryTopData(0, "");
}

void CheckBlocks::StartTimer()
{
	_timer.AsyncLoop(30000, [this](){
        int ret = _ToCheck();
        if(ret != 0)
        {
            ERRORLOG("_ToCheck error, error num: {}", ret);
        }
	});
}

//get stake and delegatinged addr
int CheckBlocks::fetchPledgeAddress(DBReadWriter& dbReader, std::vector<std::string>& pledgeAddr)
{
    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();

    for (const auto &node : nodelist)
    {
        int ret = VerifyBonusAddr(node.address);

        int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(node.address, global::ca::StakeType::STAKE_TYPE_NODE);
        if (stakeTime > 0 && ret == 0)
        {
            pledgeAddr.push_back(node.address);
        }
    }
    return 0;
}

std::pair<uint64_t, std::string> CheckBlocks::fetchTemporaryTopData()
{
    std::unique_lock<std::mutex> lock(topDateMutex);
    return this->tempTopDate;
}

void CheckBlocks::setTemporaryTopData(uint64_t height, std::string hash)
{
    std::unique_lock<std::mutex> lock(topDateMutex);
    tempTopDate.first = height;
    tempTopDate.second = hash;
}

int CheckBlocks::_ToCheck()
{
    if(isRunning)
    {
        DEBUGLOG("_ToCheck is running");
        return 0;
    }

    ON_SCOPE_EXIT{
        isRunning = false;
    };

    isRunning = true;

    _Init();

    uint64_t nodeSelfHeight = 0;
    DBReadWriter dbReaderWrite;
    auto status = dbReaderWrite.getBlockTop(nodeSelfHeight);
    if (DBStatus::DB_SUCCESS != status)
    {
        ERRORLOG("getBlockTop error, error num:{}", -1);
        return -1;
    }

    if(topBlockHeight == 0 && nodeSelfHeight < 1100)
    {
        DEBUGLOG("Currently less than 1100 height");
        return 0;
    }
    else if(nodeSelfHeight < topBlockHeight + 1100) 
    {
        DEBUGLOG("nodeSelfHeight:{} less than top_block_height + 100:{}", nodeSelfHeight, topBlockHeight + 1100);
        return 0;
    }

    while(true)
    {
        std::string tempHash;
        ca_algorithm::calculateSumHashOf1000Heights(topBlockHeight + 1000, dbReaderWrite, tempHash);
        DEBUGLOG("self tempHash: {}", tempHash);
        if(tempHash.empty())
        {
            ERRORLOG("calculateSumHashOf1000Heights error, error num:{}", -1);
            return -2;
        }
        setTemporaryTopData(topBlockHeight + 1000, tempHash);

        std::vector<std::string> pledgeAddr;
        int ret = fetchPledgeAddress(dbReaderWrite, pledgeAddr);
        if(ret != 0)
        {
            ERRORLOG("fetchPledgeAddress error, error num:{}", -2);
            return -3;
        }

        std::vector<std::string> node_ids_to_send;
        ret = MagicSingleton<SyncBlock>::GetInstance()->getSyncNode(UINT32_MAX, topBlockHeight + 1000, pledgeAddr, node_ids_to_send);
        if(ret != 0)
        {
            ERRORLOG("getSyncNode error, error num:{}", ret);
            return -4;
        }

        std::string msgId;
        size_t sendNum = node_ids_to_send.size();
        if (!dataMgrPtr.CreateWait(90, sendNum * 0.9, msgId))
        {
            return -5;
        }
        for (auto &nodeId : node_ids_to_send)
        {
            if(!dataMgrPtr.AddResNode(msgId, nodeId))
            {
                return -6;
            }
            checksumHashRequest(nodeId, msgId, topBlockHeight + 1000);
        }

        std::vector<std::string> retDatas;
        if (!dataMgrPtr.WaitData(msgId, retDatas))
        {
            if (retDatas.empty() || retDatas.size() < sendNum / 2)
            {
                ERRORLOG("wait sync height time out send:{} recv:{}", sendNum, retDatas.size());
                return -7;
            }
        }

        std::map<std::string, uint64_t> consensusMap;
        GetCheckSumHashAck ack;
        uint64_t successNum = 0;
        for (auto &ret_data : retDatas)
        {
            ack.Clear();
            if (!ack.ParseFromString(ret_data))
            {
                continue;
            }

            if(ack.success() == false)
            {
                continue;
            }

            if(ack.hash().empty())
            {
                continue;
            }

            auto find = consensusMap.find(ack.hash());
            if(find == consensusMap.end())
            {
                consensusMap.insert(std::make_pair(ack.hash(), 1));
            }
            else
            {
                ++find->second;
            }

            ++successNum;
        }

        bool back = successNum >= retDatas.size() * 0.8;
        if(!back)
        {
            ERRORLOG("success num:{} less than retDatas.size() * 0.8:{}", successNum, retDatas.size() * 0.8);
            return -8;
        }

        auto compare = [](const std::pair<std::string, uint64_t>& a, const std::pair<std::string, uint64_t>& b) {
            return a.second < b.second;
        };

        auto maxIterator = std::max_element(consensusMap.begin(), consensusMap.end(), compare);

        if (maxIterator != consensusMap.end())
        {
            DEBUGLOG("maxIterator hash: {}", maxIterator->first);
            bool byzantine = maxIterator->second >= successNum * 0.9;
            if(byzantine)
            {
                auto [tempHeight, timpHash] = fetchTemporaryTopData();
                if(maxIterator->first == timpHash)
                {
                    DBReadWriter dbWriter;
                    if (DBStatus::DB_SUCCESS !=  dbWriter.setCheckBlockHashsByBlockHeight(tempHeight, timpHash))
                    {
                        ERRORLOG("setCheckBlockHashsByBlockHeight failed !");
                        return -9;
                    }

                    uint64_t lastHeight;
                    if(DBStatus::DB_SUCCESS != dbWriter.getTopThousandSumHash(lastHeight))
                    {
                        ERRORLOG("getTopThousandSumHash failed !");
                        lastHeight = 0;
                    }

                    if(lastHeight != tempHeight - 1000)
                    {
                        ERRORLOG("lastHeight is: {} != tempHeight - 1000, tempHeight is: {} !", lastHeight, tempHeight);
                        return -10;
                    }

                    if ( DBStatus::DB_SUCCESS != dbWriter.setTopThousandSumHash(tempHeight))
                    {
                        return -11; 
                    }
                    if (DBStatus::DB_SUCCESS != dbWriter.transactionCommit())
                    {
                        ERRORLOG("(rocksdb init) transactionCommit failed !");
                        return -12;
                    }
                    return 0;
                }
                else 
                {     
                    int res = performNewSynchronization(node_ids_to_send,pledgeAddr, nodeSelfHeight);
                    DEBUGLOG("first performNewSynchronization return num: {}", res);
                    continue;
                }
            }
            int res = performNewSynchronization(node_ids_to_send, pledgeAddr, nodeSelfHeight);
            DEBUGLOG("second performNewSynchronization return num: {}", res);
            continue;
        }

    }

}


int CheckBlocks::performNewSynchronization(std::vector<std::string> node_ids_to_send, std::vector<std::string>& pledgeAddr, uint64_t nodeSelfHeight)
{
    std::vector<uint64_t> heights;
    for(int i = 1; i <= 10; i++)
    {
        heights.push_back(topBlockHeight + i * 100); 
    }

    std::vector<uint64_t> shouldSyncHeights;
    int ret = calculateByzantineSumHash(node_ids_to_send, heights, shouldSyncHeights);
    if(ret != 0)
    {
        ERRORLOG("calculateByzantineSumHash fail:{}", ret);
        return -1;
    }

    uint64_t chain_height = 0;
    if(!MagicSingleton<BlockHelper>::GetInstance()->getChainHeight(chain_height))
    {
        return -2;
    }
    for(const auto& syncHeightAlternative: shouldSyncHeights)
    {
        DEBUGLOG("shouldSyncHeights: {}",syncHeightAlternative);
        ret = MagicSingleton<SyncBlock>::GetInstance()->runNewSyncOnce(pledgeAddr, chain_height, nodeSelfHeight, syncHeightAlternative - 100, syncHeightAlternative, UINT32_MAX);
        sleep(10); 
        if(ret != 0)
        {
            ERRORLOG("runNewSyncOnce fail:{}", ret);
            return -3;
        }
    }
    return 0;
}


int CheckBlocks::calculateByzantineSumHash(const std::vector<std::string> &node_ids_to_send, const std::vector<uint64_t>& heightsForSending, std::vector<uint64_t>& shouldSyncHeights)
{
    shouldSyncHeights.clear();
    std::string msgId;
    size_t sendNum = node_ids_to_send.size();

    double acceptance_rate = 0.9;

    if (!dataMgrPtr.CreateWait(90, sendNum * acceptance_rate, msgId))
    {
        return -1;
    }

    for (auto &nodeId : node_ids_to_send)
    {
        if(!dataMgrPtr.AddResNode(msgId, nodeId))
        {
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
    
    std::map<uint64_t/*height*/, std::map<std::string/*sumhash*/, uint64_t/*num*/>>hashDataTotal;

    int successCount = 0;
    for (auto &ret_data : retDatas)
    {
        SyncFromZeroGetSumHashAck ack;
        if (!ack.ParseFromString(ret_data))
        {
            continue;
        }
        if (ack.code() == 0)
        {
            ++successCount;
            continue;
        }
        ++successCount;
        auto sumHashesResult = ack.sum_hashes();
        for(const auto& sum_hash : sumHashesResult)
        {
            const auto hash = sum_hash.hash();
            auto height = sum_hash.height();
            
            auto found = hashDataTotal.find(height);
            if (found == hashDataTotal.end())
            {
                std::map<std::string, uint64_t> temp;
                temp.insert(make_pair(hash, 1));
                hashDataTotal.insert(std::make_pair(height, temp));
                continue;
            }
            auto& content = found->second;
            auto findHash = content.find(hash);
            if(findHash == content.end())
            {
                content.insert(make_pair(hash, 1));
                continue;
            }
            findHash->second++;
        }

    }

    uint64_t backNum = sendNum * 0.8;
    bool isByzantineSuccessful = successCount >= backNum;
    if(!isByzantineSuccessful)
    {
        ERRORLOG("checkByzantineFault error, sendNum = {} , successCount = {}", sendNum, successCount);
        return -4;
    }

    DBReader dbReader;

    auto compare = [](const std::pair<std::string, uint64_t>& a, const std::pair<std::string, uint64_t>& b) {
        return a.second < b.second;
    };

    for(const auto& sumHashDataValue: hashDataTotal)
    {
        std::string shelfSumHash;
        if (DBStatus::DB_SUCCESS != dbReader.getSumHashByHeight(sumHashDataValue.first, shelfSumHash))
        {
            DEBUGLOG("fail to get sum hash height at height {}", sumHashDataValue.first);
            shouldSyncHeights.push_back(sumHashDataValue.first);
            continue;
        }

        auto maxIterator = std::max_element(sumHashDataValue.second.begin(), sumHashDataValue.second.end(), compare);

        if (maxIterator != sumHashDataValue.second.end())
        {
           bool isByzantineSuccessful = maxIterator->second >= successCount * 0.85;
           if(isByzantineSuccessful)
           {
                if(shelfSumHash == maxIterator->first)
                {
                    continue;
                }
           }
           shouldSyncHeights.push_back(sumHashDataValue.first);
        }
        else 
        {
            shouldSyncHeights.push_back(sumHashDataValue.first);
        }
    }

    return 0;
}

void checksumHashRequest(const std::string &nodeId, const std::string &msgId, uint64_t height)
{
    GetCheckSumHashReq req;
    req.set_height(height);
    req.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    req.set_msg_id(msgId);
    NetSendMessage<GetCheckSumHashReq>(nodeId, req, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

void sendChecksumHashAcknowledgment(const std::string &nodeId, const std::string &msgId, uint64_t height)
{
    GetCheckSumHashAck ack;
    DBReader dbReader;
    std::string hash;
    bool success = true;
    ack.set_height(height);
    ack.set_msg_id(msgId);
    ack.set_self_node_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    if(DBStatus::DB_SUCCESS != dbReader.getCheckBlockHashsByBlockHeight(height, hash))
    {
        auto [tempHeight, timpHash] = MagicSingleton<CheckBlocks>::GetInstance()->fetchTemporaryTopData();
        if(tempHeight == height)
        {
            ack.set_success(success);
            ack.set_hash(timpHash);
            NetSendMessage<GetCheckSumHashAck>(nodeId, ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
            return;
        }

        ERRORLOG("getCheckBlockHashsByBlockHeight Error");
        success = false;
    }
    ack.set_success(success);
    ack.set_hash(hash);
    NetSendMessage<GetCheckSumHashAck>(nodeId, ack, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::PRIORITY_HIGH_LEVEL_1);
}

int handleGetCheckSumHashRequest(const std::shared_ptr<GetCheckSumHashReq> &msg, const MsgData &msgdata)
{
    if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        return -1;
    }
    sendChecksumHashAcknowledgment(msg->self_node_id(), msg->msg_id(),msg->height());
    return 0;
}

int handleChecksumHashAcknowledge(const std::shared_ptr<GetCheckSumHashAck> &msg, const MsgData &msgdata)
{
     if(!PeerNode::verifyPeerNodeIdRequest(msgdata.fd, msg->self_node_id()))
    {
        return -1;
    }

    dataMgrPtr.waitDataToAdd(msg->msg_id(), msg->self_node_id(), msg->SerializeAsString());
    return 0;
}
