/**
 * *****************************************************************************
 * @file        epoll_mode.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef EPOLL_MODE_HEADER
#define EPOLL_MODE_HEADER

#include <sys/epoll.h>
#include <sys/resource.h>

#include <iostream>
#include <map>
#include <thread>

#include "./peer_node.h"
#include "./socket_buf.h"

#include "../include/logging.h"

class EpollMode
{
public:
    //Listen Thread Func
    /**
     * @brief       
     * 
     * @return      int 
     */
    int EpollLoop();
    /**
     * @brief       
     * 
     * @param       fd 
     * @param       state 
     * @return      true 
     * @return      false 
     */
    bool EpollLoop(int fd, int state);

    /**
     * @brief       
     * 
     * @param       fd 
     * @return      true 
     * @return      false 
     */
    bool DeleteEpollEvent(int fd);

    /**
     * @brief       
     * 
     * @return      true 
     * @return      false 
     */
    bool InitListen();

    /**
     * @brief       
     * 
     * @param       epmd 
     */
    static void EpollWork(EpollMode *epmd);

    /**
     * @brief       
     * 
     * @return      true 
     * @return      false 
     */
    bool EPOL_MODE_START();

    /**
     * @brief       
     * 
     */
    void EpollStop() { haltListening = false; }

    EpollMode();
    ~EpollMode();

public:
    int epollFd;
private:
    std::thread* _listenThread;
private:
    int fdServerMain;
    std::atomic<bool> haltListening = true;
};

#endif
