/**
 * *****************************************************************************
 * @file        double_spend_cache.h
 * @brief       
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */

#include "ca/block_stroage.h"
#include "utils/bench_mark.h"
#include "common/global_data.h"
/**
 * @brief       
 * 
 */
class doubleSpendCache
{
public:
    doubleSpendCache(){	
        _timer.AsyncLoop(1000, [this](){
        CheckLoop();
	});
        
    };
    ~doubleSpendCache() = default;
public:

    struct doubleSpendSuccess
    {
        uint64_t time;
        std::vector<std::string> utxoVector;
    };
    
    /**
     * @brief       
     * 
     */
    void StopTimer(){_timer.Cancel();}

    /**
     * @brief       
     * 
     * @param       usings: 
     * @return      int 
     */
    int AddFromAddr(const std::pair<std::string,doubleSpendCache::doubleSpendSuccess> &usings);


    /**
     * @brief       
     * 
     */
    void CheckLoop();
    void Remove(const uint64_t& txTimeKey);
    void Detection(const CBlock & block);



private:


    CTimer _timer;
	std::mutex doubleSpendMutex;
    std::map<std::string,doubleSpendCache::doubleSpendSuccess> _pending;

};
