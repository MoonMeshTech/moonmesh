#include "global.h"

namespace global
{
    std::string g_localIp;
    int cpu_count;
    std::atomic<int> node_list_refresh_time = 100; 
    MsgQueue queueReader("ReadQueue");   // Read queue
    MsgQueue queue_work("WorkQueue");   // Work queue is mainly used to process the queue calling CA code after read
    MsgQueue queue_write_counter("WriteQueue"); // Write queue
    std::list<int> Phone_List; // Store FD connected to mobile phone
    std::mutex phoneListMutex;
    CTimer g_heartTimer;
    std::mutex mutex_listen_thread;
    std::mutex thread_switch_mutex;
    std::mutex mutex_set_fee;
    std::condition_variable_any conditionListenThread;
    bool listen_thread_inited = false;

    std::mutex mutexRequestCountMap;
    std::map<std::string, std::pair<uint32_t, uint64_t>> requestCountMap;

    int broadcast_threshold= 15;
}
