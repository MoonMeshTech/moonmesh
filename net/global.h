/**
 * *****************************************************************************
 * @file        global.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <list>

#include "./define.h"
#include "./msg_queue.h"

#include "../proto/net.pb.h"
#include "../utils/timer.hpp"


namespace global
{
    extern MsgQueue queueReader;
    extern MsgQueue queue_work;
    extern MsgQueue queue_write_counter;
    extern std::string g_localIp;
    extern int cpu_count;
    extern std::atomic<int> node_list_refresh_time;
    extern std::list<int> Phone_List;
    extern std::mutex phoneListMutex;
    extern CTimer g_heartTimer;
    extern std::mutex mutex_listen_thread;
    extern std::mutex mutex_set_fee;
    extern std::condition_variable_any conditionListenThread;
    extern bool listen_thread_inited;

    extern std::mutex mutexRequestCountMap;
    extern std::map<std::string, std::pair<uint32_t, uint64_t>> requestCountMap;
    extern int broadcast_threshold;
}

#endif
