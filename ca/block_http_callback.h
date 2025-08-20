/**
 * *****************************************************************************
 * @file        block_http_callback.h
 * @brief       
 * @date        2023-09-25
 * @copyright   mm
 * *****************************************************************************
 */

#ifndef CA_HTTP_CALLBACK_HEADER_GUARD
#define CA_HTTP_CALLBACK_HEADER_GUARD

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "block.pb.h"

/**
 * @brief       
 * 
 */
class BlockHttpCallbackHandler
{
public:
    BlockHttpCallbackHandler();

    /**
     * @brief       
     * 
     * @param       block: 
     * @return      true 
     * @return      false 
     */
    bool AddBlock(const std::string& block);

    /**
     * @brief       
     * 
     * @param       block: 
     * @return      true 
     * @return      false 
     */
    bool AddBlock(const CBlock& block);

    /**
     * @brief       
     * 
     * @param       block: 
     * @return      true 
     * @return      false 
     */
    bool rollback_block_str(const std::string& block);

    /**
     * @brief       
     * 
     * @param       block: 
     * @return      true 
     * @return      false 
     */
    bool RollbackBlock(const CBlock& block);

    /**
     * @brief       
     * 
     * @param       ip: 
     * @param       port: 
     * @param       path: 
     */
    void Start(const std::string& ip, int port, const std::string& path);

    /**
     * @brief       
     * 
     */
    void Stop();

    /**
     * @brief       
     * 
     * @param       method: 
     */
    void addBlockWork(const std::string &method);

    /**
     * @brief       
     * 
     * @param       method: 
     */
    void rollbackBlockWork_(const std::string &method);

    /**
     * @brief       
     * 
     * @return      true 
     * @return      false 
     */
    bool IsRunning();

    /**
     * @brief       
     * 
     */
    void Test();

    /**
     * @brief       
     * 
     * @param       self: 
     * @return      int 
     */
    static int Process(BlockHttpCallbackHandler* self);
     
private:
    /**
     * @brief       
     * 
     * @param       block: 
     * @return      std::string 
     */
    std::string ToJson(const CBlock& block);

    /**
     * @brief       
     * 
     * @param       block: 
     * @param       method: 
     * @return      true 
     * @return      false 
     */
    bool sendBlockHttpRequest(const std::string& block, const std::string& method);

private:
    /**
     * @brief       
     * 
     * @param       where: 
     * @return      std::string 
     */
    friend std::string PrintCache(int where);

    std::vector<std::string>    _addblocks;
    std::vector<std::string>    rollbackBlocks_;
    std::thread                 workAddBlockThread;
    std::thread                 workRollbackThread;
    std::mutex                  _addMutex;
    std::mutex                  rollbackMutex;
    std::condition_variable     _cvadd;
    std::condition_variable     cvRollback;
    volatile std::atomic_bool   _running;

    std::string _ip;
    uint32_t _port;
    std::string _path;
};

#endif