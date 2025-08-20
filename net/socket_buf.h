/**
 * *****************************************************************************
 * @file        socket_buf.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef SOCKET_BUFFER_HEADER_GUARD
#define SOCKET_BUFFER_HEADER_GUARD

#include <string>
#include <queue>
#include <utility>
#include <map>
#include <unordered_map>
#include <mutex>
#include <memory>

#include "./msg_queue.h"
#include "./api.h"

#include "../include/logging.h"



extern std::unordered_map<int, std::unique_ptr<std::mutex>> fileDescriptorSetMutex;

/**
 * @brief       Get the fd mutex object
 * 
 * @param       fd 
 * @return      std::mutex& 
 */
std::mutex& fdMutex(int fd);

/**
 * @brief       Get the Mutex Size object
 * 
 * @return      int 
 */
int mutex_size();


// Message length is the first 4 bytes of the message and does not contain these 4 bytes 
class SocketBuf
{
public:
    int fd;
    uint64_t portAndIp;

private:
    std::string _cache;
	std::mutex mutexForRead;

    std::string _sendCache;
    std::mutex sendMutex;
	std::atomic<bool> _isSending;

private:
    /**
     * @brief       
     * 
     * @param       sendData 
     * @return      true 
     * @return      false 
     */
    bool sendPacketToMessageQueue(const std::string& sendData);

public:
	SocketBuf() 
    : fd(0)
    , portAndIp(0)
    , _isSending(false) 
    {};

    /**
     * @brief       
     * 
     * @param       data 
     * @param       len 
     * @return      true 
     * @return      false 
     */
    bool add_data_to_read_buffer(char *data, size_t len);

    /**
     * @brief       
     * 
     */
    void printfCache();

    /**
     * @brief       Get the Send Msg object
     * 
     * @return      std::string 
     */
    std::string GetSendMsg();

    /**
     * @brief       
     * 
     * @param       n 
     */
    void popAndSendMessageRequest(int n);

    /**
     * @brief       
     * 
     * @param       data 
     */
    void PushSendMsg(const std::string& data);

    /**
     * @brief       
     * 
     * @return      true 
     * @return      false 
     */
    bool isSendCacheEmpty();

    /**
     * @brief       
     * 
     * @param       current_message_length 
     */

    /**
     * @brief       Verify the buffer and fix if there are errors

     * 
     * @param       current_message_length 
     */
    void VerifyCache(size_t current_message_length);      

    /**
     * @brief       Fix buffers
     * 
     */
    void CorrectCache();                      
};


class bufferControl
{
public:
    bufferControl() = default;
    ~bufferControl() = default;

private:
    friend std::string PrintCache(int where);
	std::mutex _mutex;
    std::map<uint64_t,std::shared_ptr<SocketBuf>> _BufferMap;

public:

    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @param       buf 
     * @param       len 
     * @return      true 
     * @return      false 
     */
    bool addReadBufferToQueue(uint32_t ip, uint16_t port, char* buf, socklen_t len);

    /**
     * @brief       
     * 
     * @param       portAndIp 
     * @param       buf 
     * @param       len 
     * @return      true 
     * @return      false 
     */
    bool addReadBufferToQueue(uint64_t portAndIp, char* buf, socklen_t len);

    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @param       fd 
     * @return      true 
     * @return      false 
     */
    bool AddBuffer(uint32_t ip, uint16_t port,const int fd);

    /**
     * @brief       
     * 
     * @param       portAndIp 
     * @param       fd 
     * @return      true 
     * @return      false 
     */
    bool AddBuffer(uint64_t portAndIp,const int fd);

    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @return      true 
     * @return      false 
     */
    bool DeleteBuffer(uint32_t ip, uint16_t port);

    /**
     * @brief       
     * 
     * @param       portAndIp 
     * @return      true 
     * @return      false 
     */
    bool DeleteBuffer(uint64_t portAndIp);

    /**
     * @brief       
     * 
     * @param       fd 
     * @return      true 
     * @return      false 
     */
    bool DeleteBuffer(const int fd);

    /**
     * @brief       Get the Write Buffer Queue object
     * 
     * @param       portAndIp 
     * @return      std::string 
     */
    std::string writeBufferQueue(uint64_t portAndIp);

    /**
     * @brief       
     * 
     * @param       portAndIp 
     * @param       n 
     */
    void PopAndWriteBufferQueue(uint64_t portAndIp, int n);
	
    /**
     * @brief       
     * 
     * @param       portAndIp 
     * @param       ios_msg 
     * @return      true 
     * @return      false 
     */
    bool addWritePack_(uint64_t portAndIp, const std::string ios_msg);

    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @param       ios_msg 
     * @return      true 
     * @return      false 
     */
	bool addWritePack_(uint32_t ip, uint16_t port, const std::string ios_msg);

    /**
     * @brief       
     * 
     * @param       portAndIp 
     * @return      true 
     * @return      false 
     */
    bool IsExists(uint64_t portAndIp);

    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @return      true 
     * @return      false 
     */
	bool IsExists(std::string ip, uint16_t port);

    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @return      true 
     * @return      false 
     */
	bool IsExists(uint32_t ip, uint16_t port);

    /**
     * @brief       Get the Socket Buf object
     * 
     * @param       ip 
     * @param       port 
     * @return      std::shared_ptr<SocketBuf> 
     */
    std::shared_ptr<SocketBuf> GetSocketBuf(uint32_t ip, uint16_t port);
    
    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @return      true 
     * @return      false 
     */
    bool IsCacheEmpty(uint32_t ip, uint16_t port);

   
    /**
     * @brief       *test api*
     * 
     */
    void printBuffers();
};


#endif
