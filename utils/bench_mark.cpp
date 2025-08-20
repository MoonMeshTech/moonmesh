#include "bench_mark.h"
#include "time_util.h"
#include "magic_singleton.h"
#include "db/db_api.h"
#include "include/logging.h"
#include <sys/time.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <sys/sysinfo.h>

static const double CONVERSION_NUMBER = 1000000.0;
static const uint64_t kConversionNumberUnit = 1000000;
static const std::string BENCHMARK_FILE_NAME = "benchmark.json";
static const std::string BENCHMARK_FILE_NAME_2 = "benchmark2.json";

Benchmark::Benchmark() : benchmarkSwitch(false), transactionInitiateAmount(0), transactionInitiateHeight_(0)
{
    auto memoryMonitorThread = std::thread(
            [this]()
            {
                while (_threadFlag)
                {
                    struct sysinfo sysInfo;
                    if (!sysinfo(&sysInfo))
                    {
                        uint64_t memFreeTotal = sysInfo.freeram / 1024 / 1024; //unit MB
                        DEBUGLOG("memory left {} MB could be used", memFreeTotal);
                    }
                    sleep(60);
                }
            }
    );
    memoryMonitorThread.detach();
};

void Benchmark::OpenBenchmark()
{
    benchmarkSwitch = true;
    std::ofstream filestream;
    filestream.open(BENCHMARK_FILE_NAME, std::ios::trunc);
    if (!filestream)
    {
        std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
        return;
    }
    nlohmann::json initContent = nlohmann::json::array();
    filestream << initContent.dump();
    filestream.close();
}

void Benchmark::OpenBenchmarkAlt()
{
    benchmarkSwitch2 = true;
    std::ofstream filestream;
    filestream.open(BENCHMARK_FILE_NAME_2, std::ios::trunc);
    if (!filestream)
    {
        std::cout << "Open benchmark2 file failed!can't print benchmark to file" << std::endl;
        return;
    }
    nlohmann::json initContent = nlohmann::json::array();
    filestream << initContent.dump();
    filestream.close();
}

void Benchmark::Clear()
{
    _threadFlag=false;
    if (!benchmarkSwitch)
    {
        return;
    }
    benchmarkSwitch = false;
    std::cout << "please wait" << std::endl;
    sleep(5);
    transactionInitiateMap.clear();
    transactionInitCache.clear();
    transactionVerificationMap.clear();
    agentTransactionReceiveMap_.clear();
    transactionSignReceiveMap_.clear();
    transactionSignatureReceiveCache.clear();
    blockHasTransactionAmountMap.clear();
    blockVerificationMap.clear();
    blockPoolStorageMap.clear();
    transactionInitiateAmount = 0;
    transactionInitiateHeight_ = 0;
    std::cout << "clear finish" << std::endl;
    benchmarkSwitch = true;

}
void Benchmark::transactionInitiateBatchSize(uint32_t amount)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    _batchSize = amount;
}

void Benchmark::addTransactionInitiateMap(uint64_t start, uint64_t end)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionInitiateMapMutex);
    transactionInitiateMap.push_back({start, end});
    if (transactionInitiateMap.size() == _batchSize)
    {
        calculateTransactionInitiateAmountPerSecond();
    }
    
}

void Benchmark::calculateTransactionInitiateAmountPerSecond()
{
    if (!benchmarkSwitch)
    {
        return;
    }
    if (transactionInitiateMap.empty())
    {
        return;
    }
    
    uint64_t totalTimeDifference = 0;
    for(auto timeRecord : transactionInitiateMap)
    {
        totalTimeDifference = (timeRecord.second - timeRecord.first) + totalTimeDifference;
    }

    double transactionInitCostPerTx = (double)totalTimeDifference / (double)transactionInitiateMap.size();
    double transaction_initiates_per_second = (double)transactionInitiateMap.size() / ((double)totalTimeDifference / CONVERSION_NUMBER); 
    transactionInitCache[transactionInitiateMap.front().first] = {transactionInitCostPerTx, transaction_initiates_per_second};
    transactionInitiateMap.clear();
}

void Benchmark::clearTransactionInitiateMap()
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionInitiateMapMutex);
    transactionInitiateMap.clear();
}

void Benchmark::transactionMemVerifyMap(const std::string& txHash, uint64_t costTime)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionVerifyMapMutex_);
    auto found = transactionVerificationMap.find(txHash);
    if (found == transactionVerificationMap.end())
    {
        transactionVerificationMap[txHash] = verifyTimeRecord();
    }

    auto& record = transactionVerificationMap.at(txHash);
    record.memVerifyTimestamp = costTime;
    record.memVerifyAmountPerSecond = (double)1 / ((double)costTime / CONVERSION_NUMBER);
    if (record.dbVerifyTime != 0)
    {
        record.totalVerifyTime = record.memVerifyTimestamp + record.dbVerifyTime;
        record.totalVerifyAmountPerSecond = (double)1 / ((double) record.totalVerifyTime / CONVERSION_NUMBER);
    }
    
}

void Benchmark::transactionDbVerifyMap(const std::string& txHash, uint64_t costTime)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionVerifyMapMutex_);
    auto found = transactionVerificationMap.find(txHash);
    if (found == transactionVerificationMap.end())
    {
        transactionVerificationMap[txHash] = verifyTimeRecord();
    }

    auto& record = transactionVerificationMap.at(txHash);
    record.dbVerifyTime = costTime;
    record.dbVerifyAmountPerSecond = (double)1 / ((double)costTime / CONVERSION_NUMBER);

    if (record.memVerifyTimestamp != 0)
    {
        record.totalVerifyTime = record.memVerifyTimestamp + record.dbVerifyTime;
        record.totalVerifyAmountPerSecond = (double)1 / ((double) record.totalVerifyTime / CONVERSION_NUMBER);
    }
}

void Benchmark::add_agent_transaction_receive_map(const std::shared_ptr<TxMsgReq> &msg)
{
    if (!benchmarkSwitch)
    {
        return;
    }
	CTransaction txn_benchmark_temp;
	if (txn_benchmark_temp.ParseFromString(msg->txmsginfo().tx()) && txn_benchmark_temp.verifysign_size() == 0)
	{
        std::lock_guard<std::mutex> lock(m_agentTransactionReceiveMapMutex);
        auto& txHash = txn_benchmark_temp.hash();
        auto found = agentTransactionReceiveMap_.find(txHash);
        if (found != agentTransactionReceiveMap_.end())
        {
            return;
        }
        agentTransactionReceiveMap_[txHash] = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	}

}

void Benchmark::addTransactionSignReceiveMap(const std::string& txHash)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionSignReceiveMapMutex_);
    auto found = transactionSignReceiveMap_.find(txHash);
    if (found == transactionSignReceiveMap_.end())
    {
        transactionSignReceiveMap_[txHash] = {};
    }
    auto& time_record = transactionSignReceiveMap_.at(txHash);
    time_record.push_back(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
}

void Benchmark::transaction_sign_receive_per_second(const std::string& txHash, uint64_t composeTime)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(transactionSignReceiveMapMutex_);
    auto found = transactionSignReceiveMap_.find(txHash);
    if (found == transactionSignReceiveMap_.end())
    {
        return;
    }
    auto& timeRecord = transactionSignReceiveMap_.at(txHash);
    auto spanTime = composeTime - timeRecord.front();
    transactionSignatureReceiveCache[txHash] = {spanTime, (double)1 / ((double)spanTime / CONVERSION_NUMBER)};
}

void Benchmark::blockTransactionAmountMap(const std::string& blockHash, int txAmount)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(blockTxAmountMapMutex);
    blockHasTransactionAmountMap[blockHash] = txAmount;
}

void Benchmark::addBlockVerifyMap(const std::string& blockHash, uint64_t costTime)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(blockVerifyMapMutex_);
    auto found = blockVerificationMap.find(blockHash);
    if (found != blockVerificationMap.end())
    {
        return;
    }
    
    blockVerificationMap[blockHash] = {costTime, (double)1 / ((double) costTime / CONVERSION_NUMBER) };
}

void Benchmark::increase_transaction_initiate_amount()
{
    if (!benchmarkSwitch)
    {
        return;
    }
    ++transactionInitiateAmount;
    if(transactionInitiateHeight_ == 0)
    {
        DBReader dBReader;
        uint64_t top = 0;
        if (DBStatus::DB_SUCCESS != dBReader.getBlockTop(top))
        {
            ERRORLOG("getBlockTop fail");
        }
        transactionInitiateHeight_ = top + 1;
    }
}

void Benchmark::printTxCount()
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::cout << "there're " << transactionInitiateAmount << 
                " simple transactions hash been initiated since height " << transactionInitiateHeight_ << std::endl;
}

void Benchmark::addBlockPoolSaveMapStart(const std::string& blockHash)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(blockPoolSaveMapMutex_);
    auto found = blockPoolStorageMap.find(blockHash);
    if (found == blockPoolStorageMap.end())
    {
        blockPoolStorageMap[blockHash] = {MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp(), 0};
    }
}

void Benchmark::add_block_pool_save_map_end(const std::string& blockHash)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(blockPoolSaveMapMutex_);
    auto found = blockPoolStorageMap.find(blockHash);
    if (found == blockPoolStorageMap.end())
    {
        return;
    }
    auto& record = blockPoolStorageMap.at(blockHash);
    if (record.first == 0)
    {
        return;
    }
    record.second = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
}

void Benchmark::set_block_pending_time(uint64_t pendingTime)
{
    if (!benchmarkSwitch2)
    {
        return;
    }

    newBlockPendingTime = pendingTime;
}

void Benchmark::setByTxHash(const std::string& TxHash, void* arg , uint16_t type)
{
    if (!benchmarkSwitch2)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(txHandlingMutex);
    switch (type)
    {
    case 1:
        _TV[TxHash].StartTime = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 2:
        _TV[TxHash].Verify_2 = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 3:
        _TV[TxHash].Verify_3 = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 4:
        _TV[TxHash].EndTime = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 5:
        _TV[TxHash].Timeout = true;
        break;
    default:
        break;
    }
}
void Benchmark::setByBlockHash(const std::string& BlockHash, void* arg , uint16_t type, void* arg2, void* arg3, void* arg4)
{
    if (!benchmarkSwitch2)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(txHandlingMutex);
    switch (type)
    {
    case 1:
        _BV[BlockHash].Time = *reinterpret_cast<uint64_t*>(arg);
        _BV[BlockHash].Verify_4 = *reinterpret_cast<uint64_t*>(arg2);
        _BV[BlockHash].TxNumber = *reinterpret_cast<uint64_t*>(arg3);
        _BV[BlockHash].Hight = *reinterpret_cast<uint64_t*>(arg4);
        break;
    case 2:
        _BV[BlockHash].BroadcastTime = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 3:
        _BV[BlockHash].Verify_5 = *reinterpret_cast<uint64_t*>(arg);
        break;
    case 4:
        _VN[BlockHash].Hight =  *reinterpret_cast<uint64_t*>(arg);
        _VN[BlockHash].TxNumber =  *reinterpret_cast<uint64_t*>(arg2);
        break;
    case 5:
        _VN[BlockHash].memVerifyTime =  *reinterpret_cast<uint64_t*>(arg);
        _VN[BlockHash].transactionVerifyTimeRequest =  *reinterpret_cast<uint64_t*>(arg2);
        break;
    case 6:
        _BV[BlockHash].blockPendingTime = newBlockPendingTime;
        _BV[BlockHash].BuildBlockTime = *reinterpret_cast<uint64_t*>(arg);
        _BV[BlockHash].checkConflictTime = *reinterpret_cast<uint64_t*>(arg2);
        break;
    case 7:
        _BV[BlockHash].nodeSearchDuration = *reinterpret_cast<uint64_t*>(arg);
        _BV[BlockHash].totalTime = *reinterpret_cast<uint64_t*>(arg2);
        break;
    case 8:
        _BV[BlockHash].seekPrehashTimeValue = *reinterpret_cast<uint64_t*>(arg);
        break;
    default:
        break;
    }
}

void Benchmark::setTransactionHashByBlockHash(const std::string& BlockHash, const std::string& TxHash)
{
    if (!benchmarkSwitch2)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(txHandlingMutex);
    _BT.insert({BlockHash,TxHash});
}
void Benchmark::PrintBenchmarkSummary(bool exportToFile)
{
    if (!benchmarkSwitch)
    {
        return;
    }
    
    nlohmann::json benchmarkJson;
    if (exportToFile)
    {
         std::ifstream readfilestream;
        readfilestream.open(BENCHMARK_FILE_NAME);
        if (!readfilestream)
        {
            std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
            return;
        }
        std::string content;
        readfilestream >> content;
        try
        {
            benchmarkJson = nlohmann::json::parse(content);
        }
        catch(const std::exception& e)
        {
            std::cout << "benchmark json parse fail" << std::endl;
            return;
        }
        readfilestream.close();
    } 

    nlohmann::json benchmark_item_json;
    if (!transactionInitCache.empty())
    {
        std::lock_guard<std::mutex> lock(transactionInitiateMapMutex);
        double costSum = 0;
        for(auto record : transactionInitCache)
        {
            costSum += record.second.first;
        }
        double averageTransactionTimeCost = costSum / transactionInitCache.size();
        double transactionAmountPerSecond_ = (double) 1 / (averageTransactionTimeCost / CONVERSION_NUMBER);
        
        if (exportToFile)
        {
            std::ostringstream stream;
            stream << transactionAmountPerSecond_;
            benchmark_item_json["one-to-one_transactions_can_be_initiated_per_second"] = stream.str();
        }
        else
        {
            std::cout << "one-to-one transactions can be initiated per second: " << transactionAmountPerSecond_ << std::endl;
        }
    }
    else
    {
        if (exportToFile)
        {
            benchmark_item_json["one-to-one_transactions_can_be_initiated_per_second"] = "";
        }
    }

    if (!transactionVerificationMap.empty())
    {
        std::lock_guard<std::mutex> lock(transactionVerifyMapMutex_);
        uint64_t memoryCostSum = 0;
        uint64_t databaseCostTotal = 0;
        uint64_t totalCostSum = 0;
        int skipCount = 0;
        for(auto record : transactionVerificationMap)
        {
            if (record.second.memVerifyTimestamp == 0 
                || record.second.dbVerifyTime == 0
                || record.second.totalVerifyTime == 0)
            {
                skipCount++;
                continue;
            }
            
            memoryCostSum += record.second.memVerifyTimestamp;
            databaseCostTotal += record.second.dbVerifyTime;
            totalCostSum += record.second.totalVerifyTime;
        }
        double memoryCostAvg = (double)memoryCostSum / (double)(transactionVerificationMap.size() - skipCount);
        double averageDbCost = (double)databaseCostTotal / (double)(transactionVerificationMap.size() - skipCount);
        double total_cost_avg = (double)totalCostSum / (double)(transactionVerificationMap.size() - skipCount);
        double mem_verify_per_second = (double) 1 / (memoryCostAvg / CONVERSION_NUMBER);
        double dbVerifyPerSecond_ = (double) 1 / (averageDbCost / CONVERSION_NUMBER);
        double total_verify_per_second = (double) 1 / (total_cost_avg / CONVERSION_NUMBER);
        if (exportToFile)
        {
            std::ostringstream totalStream;
            totalStream << total_verify_per_second;
            std::ostringstream memStream;
            memStream << mem_verify_per_second;
            std::ostringstream dbStream;
            dbStream << dbVerifyPerSecond_;
            benchmark_item_json["Number_of_verifiable_transactions_per_second"] = totalStream.str();
            benchmark_item_json["Number_of_verifiable_transactions_per_second_in_memory"] = memStream.str();
            benchmark_item_json["Number_of_verifiable_transactions_per_second_in_db"] = dbStream.str();
        }
        else
        {
            std::cout << "Number of verifiable transactions per second: " << total_verify_per_second 
                  << " (mem verify: " << mem_verify_per_second << " db verify: " << dbVerifyPerSecond_ << ")" << std::endl;
        }

    }
    else
    {
        if (exportToFile)
        {
            benchmark_item_json["Number_of_verifiable_transactions_per_second"] = "";
            benchmark_item_json["Number_of_verifiable_transactions_per_second_in_memory"] = "";
            benchmark_item_json["Number_of_verifiable_transactions_per_second_in_db"] = "";
        }
    }

    if (!agentTransactionReceiveMap_.empty())
    {
        std::lock_guard<std::mutex> lock(m_agentTransactionReceiveMapMutex);
        std::map<uint64_t, uint64_t> hitCache;
        for(auto record : agentTransactionReceiveMap_)
        {
            uint64_t time = record.second / kConversionNumberUnit;
            auto found = hitCache.find(time);
            if (found == hitCache.end())
            {
                hitCache[time] = 1;
            }
            auto& hitTimes = found->second;
            hitTimes += 1;
        }

        uint64_t maxHitTimes = 0;
        for(auto hits : hitCache)
        {
            if (hits.second > maxHitTimes)
            {
                maxHitTimes = hits.second;
            }
        }
        if (exportToFile)
        {
            std::ostringstream stream;
            stream << maxHitTimes;
            benchmark_item_json["Number_of_transactions_per_second"] = stream.str();
        }
        else
        {
            std::cout << "Number of transactions per second from internet: " << maxHitTimes << std::endl;
        }
    }
    else
    {
        if (exportToFile)
        {
            benchmark_item_json["Number_of_transactions_per_second"] = "";
        }
    }

    if(!transactionSignReceiveMap_.empty())
    {
        std::lock_guard<std::mutex> lock(transactionSignReceiveMapMutex_);

        uint64_t transactionComposeTotalCost = 0;
        for(auto record : transactionSignatureReceiveCache)
        {
            transactionComposeTotalCost += record.second.first;
        }

        double averageTransactionComposeCost = (double)transactionComposeTotalCost / (double)transactionSignatureReceiveCache.size();
        double transactionComposeAmountPerSecond = (double)1 / (averageTransactionComposeCost / CONVERSION_NUMBER);
        if (exportToFile)
        {
            std::ostringstream stream;
            stream << transactionComposeAmountPerSecond;
            benchmark_item_json["signature_per_second_can_be_collected_from_the_network_and_combined_into_a_complete_transaction_body"] = stream.str();
        }
        else
        {
            std::cout << "signature per second can be collected from the network and combined into a complete transaction body: " << transactionComposeAmountPerSecond << std::endl;
        }
    }
    else
    {
        if (exportToFile)
        {
            benchmark_item_json["signature_per_second_can_be_collected_from_the_network_and_combined_into_a_complete_transaction_body"] = "";
        }
    }

    if (!blockHasTransactionAmountMap.empty())
    {
        std::lock_guard<std::mutex> lock(blockTxAmountMapMutex);
        uint64_t total_transaction_amount = 0;
        for(auto record : blockHasTransactionAmountMap)
        {
            total_transaction_amount += record.second;
        }
        double txAmountAverage = (double)total_transaction_amount / (double)blockHasTransactionAmountMap.size();
        if (exportToFile)
        {
            std::ostringstream stream;
            stream << txAmountAverage;
            benchmark_item_json["transaction_count_can_be_packed_into_a_full_block_per_second"] = stream.str();
        }
        else
        {
            std::cout << "transaction count can be packed into a full block per second: " << txAmountAverage << std::endl;
        }
    }
    else
    {
        if (exportToFile)
        {
            benchmark_item_json["transaction_count_can_be_packed_into_a_full_block_per_second"] = "";
        }
    }

    if (!blockVerificationMap.empty())
    {
        std::lock_guard<std::mutex> lock(blockVerifyMapMutex_);
        uint64_t blockVerificationCostTotal = 0;
        for(auto record : blockVerificationMap)
        {
            blockVerificationCostTotal += record.second.first;
        }
        double blockVerifyCostAvg = (double)blockVerificationCostTotal / (double)blockVerificationMap.size();
        if (exportToFile)
        {
            std::ostringstream stream;
            stream << blockVerifyCostAvg;
            benchmark_item_json["Block_validation_time_in_the_block_pool"] = stream.str();
        }
        else
        {
            std::cout << "Block validation time in the block pool: " << blockVerifyCostAvg << std::endl;
        }
    }
    else
    {
        if (exportToFile)
        {
            benchmark_item_json["Block_validation_time_in_the_block_pool"] = "";
        }
    }
    
    if (!blockPoolStorageMap.empty())
    {
        std::lock_guard<std::mutex> lock(blockPoolSaveMapMutex_);
        uint64_t blockSaveTimeSum_ = 0;
        int fail_count = 0;
        for(auto record : blockPoolStorageMap)
        {
            if (record.second.second <= record.second.first)
            {
                fail_count++;
                continue;
            }
            
            blockSaveTimeSum_ += (record.second.second - record.second.first);
        }
        double averageBlockSaveTime = (double)blockSaveTimeSum_ / (double)(blockPoolStorageMap.size() - fail_count);
        if (exportToFile)
        {
            std::ostringstream stream;
            stream << averageBlockSaveTime;
            benchmark_item_json["Time_for_blocks_in_the_block_pool_to_be_stored_in_the_database"] = stream.str();
        }
        else
        {
            std::cout << "Time for blocks in the block pool to be stored in the database: " << averageBlockSaveTime << std::endl;
        }
    }
    else
    {
        if (exportToFile)
        {
            benchmark_item_json["Time_for_blocks_in_the_block_pool_to_be_stored_in_the_database"] = "";
        }
    }

    if (exportToFile)
    {
        std::ofstream filestream;
        filestream.open(BENCHMARK_FILE_NAME, std::ios::trunc);
        if (!filestream)
        {
            std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
            return;
        }
        benchmarkJson.push_back(benchmark_item_json);
        filestream << benchmarkJson.dump();
        filestream.close();
    } 
    return ;

}

void Benchmark::print_benchmark_summary_handle_tx(bool exportToFile)
{
    if (!benchmarkSwitch2)
    {
        return;
    }
    
    nlohmann::json benchmarkJson;
    if (exportToFile)
    {
         std::ifstream readfilestream;
        readfilestream.open(BENCHMARK_FILE_NAME_2);
        if (!readfilestream)
        {
            std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
            return;
        }
        std::string content;
        readfilestream >> content;
        try
        {
            benchmarkJson = nlohmann::json::parse(content);
        }
        catch(const std::exception& e)
        {
            std::cout << "benchmark json parse fail" << std::endl;
            return;
        }
        readfilestream.close();
    } 

    nlohmann::json benchmark_item_json;
    std::map<uint64_t, std::pair<uint64_t, double>> HT;
    std::map<uint64_t, uint64_t> HS;
    
    if(!_BV.empty())
    {
        std::lock_guard<std::mutex> lock(txHandlingMutex);
        for(auto& it : _BV)
        {
            benchmark_item_json["BlockHash"] = it.first.substr(0,6);
            auto targetBegin = _BT.lower_bound(it.first);
            auto targetEnd = _BT.upper_bound(it.first);
            uint64_t totalVerifyTime = 0;
            uint64_t TxTimeMin = it.second.Time;
            
            uint64_t averageStartTxTime = 0;
            for (; targetBegin != targetEnd ; targetBegin++)
            {
                if(_TV.find(targetBegin->second) != _TV.end())
                {
                    HS[it.second.Hight]++;
                    auto &tx = _TV[targetBegin->second];
                    if(TxTimeMin > tx.StartTime) TxTimeMin = tx.StartTime;
                    
                    averageStartTxTime += tx.StartTime;
                    totalVerifyTime = totalVerifyTime + tx.Verify_2 + tx.Verify_3;
                }
            }
            benchmark_item_json["BlockTime"] = it.second.Time / 1000000;
            benchmark_item_json["BlockHight"] = it.second.Hight;
            benchmark_item_json["BlockTxNumber"] = it.second.TxNumber;
            benchmark_item_json["TxAverageCompositionTime"] = 0;
            benchmark_item_json["TxVerifyTime_2345"] = (double)totalVerifyTime / 1000000;
            benchmark_item_json["TxVerifyTime_5"] = (double)it.second.Verify_5 / 1000000;
            //Time to successful block construction
            benchmark_item_json["BuildBlockSuccessTime"] = (double)(it.second.Time - TxTimeMin) / 1000000;
            benchmark_item_json["BuildBlockSuccessAverageTime"] = (double)(it.second.Time - (averageStartTxTime / it.second.TxNumber)) / 1000000;
            //The time to build into a block and the time to start broadcasting a SAVE message for a successful block flow
            benchmark_item_json["BroadcastSaveBlockTime"] = (double)(it.second.BroadcastTime - it.second.Time) / 1000000;
            //Time to wait for block creation
            benchmark_item_json["blockPendingTime"] = (double)(it.second.blockPendingTime - (averageStartTxTime / it.second.TxNumber)) / 1000000;
            //Time from creation of block to start of handleBlock
            benchmark_item_json["BuildBlockTime"] = (double)it.second.BuildBlockTime / 1000000;
            //Time to find broadcast node
            benchmark_item_json["nodeSearchDuration"] = (double)it.second.nodeSearchDuration / 1000000;
            //Time of the whole process
            benchmark_item_json["totalTime"] = (double)(it.second.totalTime - (averageStartTxTime / it.second.TxNumber)) / 1000000;

            benchmark_item_json["seekPrehashTimeValue"] = (double)(it.second.seekPrehashTimeValue - TxTimeMin) / 1000000;
            benchmark_item_json["checkConflictTime"] = (double)it.second.checkConflictTime / 1000000;
            
            HT[it.second.Hight].second += (double)(it.second.Time - TxTimeMin) / 1000000;
            HT[it.second.Hight].first += it.second.TxNumber;
            benchmarkJson.push_back(benchmark_item_json);
        }
        int i = 0;
        for(auto& it : HT)
        {
            benchmarkJson.at(i)["Hight"] = it.first;
            benchmarkJson.at(i)["HightTxNumber"] = it.second.first;
            benchmarkJson.at(i)["HighBuildTime"] = it.second.second;
            i++;
        }

        uint64_t tx_success_total = 0;
        i = 0;
        for(auto& it : HS)
        {
            benchmarkJson.at(i)["TxHight"] = it.first;
            benchmarkJson.at(i)["TxNumber"] = it.second;
            tx_success_total += it.second;
            i++;
        }
        benchmarkJson.at(0)["TxTotal"] = _TV.size();
        benchmarkJson.at(0)["tx_success_total"] = tx_success_total;
        benchmarkJson.at(0)["TxFailTotal"] = _TV.size() - tx_success_total;
    }
    else
    {
        if (exportToFile)
        {
            benchmark_item_json["BlockHash"] = "";
            benchmark_item_json["TxNumber"] = "";
            benchmark_item_json["Hight"] = "";
            benchmark_item_json["TxAverageCompositionTime"] = "";
            benchmark_item_json["TxVerifyTime_2345"] = "";
            benchmark_item_json["BuildBlockSuccessTime"] = "";
            benchmark_item_json["BuildBlockBroadcastTime"] = "";
            benchmark_item_json["NumberFailTx"] = "";
            benchmarkJson.push_back(benchmark_item_json);
            benchmark_item_json["BlockHash"] = "1";
            benchmark_item_json["TxNumber"] = "2";
            benchmark_item_json["Hight"] = "3";
            benchmark_item_json["TxAverageCompositionTime"] = "4";
            benchmark_item_json["TxVerifyTime_2345"] = "5";
            benchmark_item_json["BuildBlockSuccessTime"] = "6";
            benchmark_item_json["BuildBlockBroadcastTime"] = "7";
            benchmark_item_json["NumberFailTx"] = "8";
            benchmarkJson.push_back(benchmark_item_json);
            benchmark_item_json["BlockHash"] = "11";
            benchmark_item_json["TxNumber"] = "22";;
            benchmark_item_json["Hight"] = "33";
            benchmark_item_json["TxAverageCompositionTime"] = "44";
            benchmark_item_json["TxVerifyTime_2345"] = "55";
            benchmark_item_json["BuildBlockSuccessTime"] = "66";
            benchmark_item_json["BuildBlockBroadcastTime"] = "77";
            benchmark_item_json["NumberFailTx"] = "88";
            benchmarkJson.push_back(benchmark_item_json);

            benchmark_item_json.clear();
            benchmark_item_json["NumberFailTx"] = 100;
            benchmarkJson.push_back(benchmark_item_json);

            benchmark_item_json.clear();
            benchmark_item_json["NumberFailTx"] = 200;
            benchmarkJson.push_back(benchmark_item_json);
        }
    }

    if(!_VN.empty())
    {
        benchmarkJson.at(0)["VN_Hight"] = "";
        benchmarkJson.at(0)["VN_TxNumber"] = "";
        benchmarkJson.at(0)["VN_MemVerifyTime"] = "";
        benchmarkJson.at(0)["VN_TxVerifyTime"] = "";

        for(auto& it : _VN)
        {
            benchmark_item_json.clear();
            benchmark_item_json["VN_Hight"] = it.second.Hight;
            benchmark_item_json["VN_TxNumber"] = it.second.TxNumber;
            benchmark_item_json["VN_MemVerifyTime"] = (double)it.second.memVerifyTime / 1000000;
            benchmark_item_json["VN_TxVerifyTime"] = (double)it.second.transactionVerifyTimeRequest / 1000000;

            benchmarkJson.push_back(benchmark_item_json);
        }


    }
    if (exportToFile)
    {
        std::ofstream filestream;
        filestream.open(BENCHMARK_FILE_NAME_2, std::ios::trunc);
        if (!filestream)
        {
            std::cout << "Open benchmark file failed!can't print benchmark to file" << std::endl;
            return;
        }
        filestream << benchmarkJson.dump();
        filestream.close();
    }
    _TV.clear();
    _BV.clear();
    _BT.clear();
    _VN.clear();
    return ;
}
