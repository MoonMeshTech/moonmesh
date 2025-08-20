
#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include "utils/tmp_log.h"

class AsyncLogger {
public:
    static AsyncLogger& getInstance() {
        static std::once_flag initFlag;
        std::call_once(initFlag, []() {
            instance_.reset(new AsyncLogger());
        });
        return *instance_;
    }

    static void initialize(const std::string& filename, 
                           size_t maxCacheSize = 1000, 
                           unsigned int flushIntervalMs = 1000) {
        std::lock_guard<std::mutex> lock(initMutex_);
        
        if (initialized_) {
            throw std::runtime_error("Logger already initialized");
        }
        
        auto& logger = getInstance();
        logger.initImpl(filename, maxCacheSize, flushIntervalMs);
        initialized_ = true;
    }

    void log(std::thread::id id,const std::string& message) {
        if (!initialized_) {
            throw std::runtime_error("Logger not initialized");
        }
        
        std::unique_lock<std::mutex> lock(mutex_);

        logCache_[id].push_back(message);
        
        if (logCache_.size() >= maxCacheSize_) {
            lock.unlock();
            cond_.notify_one();
        }
    }

    void flush() {
        std::unique_lock<std::mutex> lock(mutex_);
        flushImpl();
    }

    void shutdown() {
        if (initialized_ && !stop_) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stop_ = true;
            }
            cond_.notify_one();
            if (writeThread_.joinable()) {
                writeThread_.join();
            }
            flush();
        }
    }

    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

private:
    AsyncLogger() = default; 

    void initImpl(const std::string& filename, 
                 size_t maxCacheSize, 
                 unsigned int flushIntervalMs) {
        filename_ = filename;
        maxCacheSize_ = maxCacheSize;
        flushInterval_ = flushIntervalMs;

        writeThread_ = std::thread(&AsyncLogger::writeLoop, this);
    }

    void writeLoop() {
        auto nextFlush = std::chrono::steady_clock::now() + 
                         std::chrono::milliseconds(flushInterval_);
        
        while (true) {
            std::unique_lock<std::mutex> lock(mutex_);

            if (cond_.wait_until(lock, nextFlush, [this] { 
                return !logCache_.empty() || stop_; 
            })) {
                if (stop_) return;
                
                flushImpl();  
            }

            nextFlush = std::chrono::steady_clock::now() + 
                        std::chrono::milliseconds(flushInterval_);
        }
    }

    void flushImpl() {
        if (logCache_.empty()) return;

        std::ofstream file(filename_, std::ios::app);
        if (!file.is_open()) {
            std::cerr << "Failed to open log file: " << filename_ << std::endl;
            return;
        }

        for (const auto& msg : logCache_) {
            for(const auto & v:msg.second){
                file << getTimestamp() << " " << v << '\n';
            }
        }
        file.flush(); 
        logCache_.clear();  
    }

    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&t);
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") 
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

private:

    static std::unique_ptr<AsyncLogger> instance_;
    static std::mutex initMutex_;
    static bool initialized_;

    std::string filename_;
    size_t maxCacheSize_ = 1000;
    unsigned int flushInterval_ = 1000;

    std::map<std::thread::id,std::vector<std::string>> logCache_;
    
    std::mutex mutex_;
    std::condition_variable cond_;
    std::thread writeThread_;
    std::atomic<bool> stop_{false};
};





#define TIMLOG(...) \
    do { \
        std::ostringstream oss; \
        oss << __FILE__ << ":" << __LINE__ << " " << __FUNCTION__ << " - "; \
        oss << Sutil::Format(__VA_ARGS__); \
        AsyncLogger::getInstance().log(std::this_thread::get_id(),oss.str()); \
    } while (0)


// int main() {
//     try {
//         AsyncLogger::initialize("app.log", 500, 2000);
        
//         auto& logger = AsyncLogger::getInstance();

//         auto worker = [](int id) {
//             auto& log = AsyncLogger::getInstance();
//             for (int i = 0; i < 100; ++i) {
//                 std::ostringstream ss;
//                 ss << "[Thread " << id << "] Log entry " << i;
//                 log.log(ss.str());
//                 std::this_thread::sleep_for(std::chrono::milliseconds(10));
//             }
//         };
        
//         std::thread threads[4];
//         for (int i = 0; i < 4; ++i) {
//             threads[i] = std::thread(worker, i);
//         }
        
//         for (auto& t : threads) t.join();

//         AsyncLogger::getInstance().shutdown();
//     } catch (const std::exception& e) {
//         std::cerr << "Logging error: " << e.what() << std::endl;
//         return 1;
//     }
    
//     return 0;
// }