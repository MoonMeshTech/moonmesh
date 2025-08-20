/**
 * *****************************************************************************
 * @file        bench_mark.h
 * @brief       
 * @date        2023-09-28
 * @copyright   mm
 * *****************************************************************************
 */

#ifndef BENCHMARK_H_
#define BENCHMARK_H_

#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <memory>

#include "proto/ca_protomsg.pb.h"

/**
 * @brief       
 * 
 */
class Benchmark
{
public:
    Benchmark();
    /**
     * @brief       
     * 
     */
    void OpenBenchmark();

    /**
     * @brief       
     * 
     */
    void OpenBenchmarkAlt();

    /**
     * @brief       
     * 
     */
    void Clear();

    /**
     * @brief       Set the Transaction Initiate Batch Size object
     * 
     * @param       amount: 
     */
    void transactionInitiateBatchSize(uint32_t amount);

    /**
     * @brief       
     * 
     * @param       start: 
     * @param       end: 
     */
    void addTransactionInitiateMap(uint64_t start, uint64_t end);

    /**
     * @brief       
     * 
     */
    void clearTransactionInitiateMap();

    /**
     * @brief       
     * 
     * @param       txHash: 
     * @param       costTime: 
     */
    void transactionMemVerifyMap(const std::string& txHash, uint64_t costTime);

    /**
     * @brief       
     * 
     * @param       txHash: 
     * @param       costTime: 
     */
    void transactionDbVerifyMap(const std::string& txHash, uint64_t costTime);

    /**
     * @brief       
     * 
     * @param       msg: 
     */
    void add_agent_transaction_receive_map(const std::shared_ptr<TxMsgReq> &msg);

    /**
     * @brief       
     * 
     * @param       txHash: 
     */
    void addTransactionSignReceiveMap(const std::string& txHash);

    /**
     * @brief       
     * 
     * @param       txHash: 
     * @param       composeTime: 
     */
    void transaction_sign_receive_per_second(const std::string& txHash, uint64_t composeTime);

    /**
     * @brief       
     * 
     * @param       blockHash: 
     * @param       txAmount: 
     */
    void blockTransactionAmountMap(const std::string& blockHash, int txAmount);

    /**
     * @brief       
     * 
     * @param       blockHash: 
     * @param       costTime: 
     */
    void addBlockVerifyMap(const std::string& blockHash, uint64_t costTime);

    /**
     * @brief       
     * 
     */
    void increase_transaction_initiate_amount();

    /**
     * @brief       
     * 
     */
    void printTxCount();

    /**
     * @brief       
     * 
     * @param       blockHash: 
     */
    void addBlockPoolSaveMapStart(const std::string& blockHash);

    /**
     * @brief       
     * 
     * @param       blockHash: 
     */
    void add_block_pool_save_map_end(const std::string& blockHash);

    /**
     * @brief       
     * 
     * @param       exportToFile: 
     */
    void PrintBenchmarkSummary(bool exportToFile);


    /**
     * @brief       Set the Block Pending Time object
     * 
     * @param       pendingTime: 
     */
    void set_block_pending_time(uint64_t pendingTime);

    /**
     * @brief       Set the By Tx Hash object
     * 
     * @param       TxHash: 
     * @param       arg: 
     * @param       type: 
     */
    void setByTxHash(const std::string& TxHash, void* arg, uint16_t type);

    /**
     * @brief       Set the By Block Hash object
     * 
     * @param       BlockHash: 
     * @param       arg: 
     * @param       type: 
     * @param       arg2: 
     * @param       arg3: 
     * @param       arg4: 
     */
    void setByBlockHash(const std::string& BlockHash, void* arg, uint16_t type, void* arg2 = nullptr, void* arg3 = nullptr, void* arg4 = nullptr);
    
    /**
     * @brief       Set the Tx Hash By Block Hash object
     * 
     * @param       BlockHash: 
     * @param       TxHash: 
     */
    void setTransactionHashByBlockHash(const std::string& BlockHash, const std::string& TxHash);

    /**
     * @brief       
     * 
     * @param       exportToFile: 
     */
    void print_benchmark_summary_handle_tx(bool exportToFile);

private:
    bool benchmarkSwitch;
    bool benchmarkSwitch2{false};
    std::mutex transactionInitiateMapMutex;
    uint32_t _batchSize;
    std::vector<std::pair<uint64_t, uint64_t>> transactionInitiateMap;
    std::map<uint64_t, std::pair<double, double>> transactionInitCache;

    /**
     * @brief       
     * 
     */
    void calculateTransactionInitiateAmountPerSecond();
    std::atomic<bool> _threadFlag=true;

    struct verifyTimeRecord
    {
        verifyTimeRecord() : memVerifyTimestamp(0), dbVerifyTime(0){};
        uint64_t memVerifyTimestamp;
        double memVerifyAmountPerSecond;
        uint64_t dbVerifyTime;
        double dbVerifyAmountPerSecond;
        uint64_t totalVerifyTime;
        double totalVerifyAmountPerSecond;
    };
    std::mutex transactionVerifyMapMutex_;
    std::map<std::string, verifyTimeRecord> transactionVerificationMap;

    std::mutex m_agentTransactionReceiveMapMutex;
    std::map<std::string, uint64_t> agentTransactionReceiveMap_;

    std::mutex transactionSignReceiveMapMutex_;
    std::map<std::string, std::vector<uint64_t>> transactionSignReceiveMap_;
    std::map<std::string, std::pair<uint64_t, double>> transactionSignatureReceiveCache;

    std::mutex blockTxAmountMapMutex;
    std::map<std::string, int> blockHasTransactionAmountMap;

    std::mutex blockVerifyMapMutex_;
    std::map<std::string, std::pair<uint64_t, double>> blockVerificationMap;

    std::atomic_uint64_t transactionInitiateAmount;
    std::atomic_uint64_t transactionInitiateHeight_;

    std::mutex blockPoolSaveMapMutex_;
    std::map<std::string, std::pair<uint64_t, uint64_t>> blockPoolStorageMap;

    
    struct TxVerify{
        TxVerify(): Verify_2(0), Verify_3(0), Timeout(false){};

        uint64_t StartTime; 

        uint64_t Verify_2;
        uint64_t Verify_3;

        uint64_t EndTime; 

        bool Timeout;   
    };
    struct BlockVerify{
        BlockVerify(): Verify_4(0), Verify_5(0){};

        uint64_t Time;
        uint64_t BroadcastTime;

        uint64_t Verify_4;
        uint64_t Verify_5;

        uint64_t TxNumber;
        uint64_t Hight;

        uint64_t blockPendingTime;
        uint64_t BuildBlockTime;
        uint64_t checkConflictTime;
        uint64_t nodeSearchDuration;
        uint64_t totalTime;

        uint64_t seekPrehashTimeValue;
    };
    struct ValidateNode
    {
        uint64_t Hight;
        uint64_t TxNumber;
        uint64_t memVerifyTime;
        uint64_t transactionVerifyTimeRequest;
    };
    std::mutex txHandlingMutex;
    std::map<std::string,TxVerify> _TV;
    std::multimap<std::string,std::string> _BT;
    std::map<std::string,BlockVerify> _BV;
    std::map<std::string,ValidateNode> _VN;
    uint64_t newBlockPendingTime;

};

#endif