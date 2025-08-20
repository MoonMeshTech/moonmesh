/**
 * *****************************************************************************
 * @file        test.hpp
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef NETWORK_TEST_HEADER_GUARD
#define NETWORK_TEST_HEADER_GUARD

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "../common/task_pool.h"

class netTest
{
    
public:
    netTest()
    :netTestTime(-1)
    ,_signal(true)
    ,_flag(false) 
    {}

    /**
     * @brief       Set the Time object
     * 
     * @param       t 
     */
    void SetTime(double t)
    {
        std::unique_lock<std::mutex> lock(_mutexTime);
        _flag = true;
        netTestTime = t;
    }

    void IsValue()
    {
        int i = 0;
        for(; i < 20; i++)
        {
            if(netTestTime == -1)
            {
                sleep(1);
                continue;
            }
            break;
        }

        std::unique_lock<std::mutex> lock(_mutexTime);
        _signal = true;
    }

    double GetTime()
    {
        std::unique_lock<std::mutex> lock(_mutexTime);
        double t = netTestTime;
        netTestTime = -1;
        _flag = false;
        return t;
    }

    bool GetSignal()
    {
        return _signal;
    }
    bool GetFlag()
    {
        return _flag;
    }

    void networkTestRegister()
    {
        _mutexTime.lock();
        _signal = false;
        _mutexTime.unlock();
        MagicSingleton<TaskPool>::GetInstance()->commitCaTask(std::bind(&netTest::IsValue, this));
    }

private:
    double netTestTime;
    bool _signal;
    bool _flag;
    std::mutex _mutexTime;
};

class echoTest
{
public:
    void addEchoCatchVar(std::string time, std::string IP)
    {
        auto find = echoCatch.find(time);
        if(find == echoCatch.end())
        {
            std::unique_lock<std::mutex> lock(echoMutex);
            echoCatch.insert(std::make_pair(time, std::vector<std::string>()));
            echoCatch[time].emplace_back(IP);
        }
        else
        {
            std::unique_lock<std::mutex> lock(echoMutex);
            echoCatch[time].emplace_back(IP);
        }
    }

    bool delete_echo_catch(std::string time)
    {
        auto find = echoCatch.find(time);
        if(find == echoCatch.end())
        {
            return false;
        }
        std::unique_lock<std::mutex> lock(echoMutex);
        echoCatch.erase(time);
        return true;
    }

    void AllClear()
    {
        echoCatch.clear();
    }

    std::map<std::string, std::vector<std::string>> getEchoCatchData()
    {
        std::unique_lock<std::mutex> lock(echoMutex);
        return echoCatch;
    }
private:
friend std::string PrintCache(int where);
std::mutex echoMutex;
std::map<std::string, std::vector<std::string>>  echoCatch;

};



#endif 