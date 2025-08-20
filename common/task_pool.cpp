#include "task_pool.h"
#include "../common/bind_thread.h"

std::mutex getThreadIdMutex;
std::set<boost::thread::id> threadIds;

void Gettid()
{
    std::lock_guard<std::mutex> lock(getThreadIdMutex);
    boost::thread::id tid = boost::this_thread::get_id();
    for(int i=0;i<100000;i++){};
    if((threadIds.find(tid)) == threadIds.end())
    {
        threadIds.insert(tid);
    }
}

void TaskPool::initializeTaskPool()
{
    for(int i=0; i < global::CA_THREAD_NUMBER * 100; i++) caTaskPool_.schedule(&Gettid);
    for(int i=0; i < global::K_NET_THREAD_NUMBER * 100; i++) netTaskPool.schedule(&Gettid);
    for(int i=0; i < global::kBraodcastThreadNumber * 100; i++) broadcastTaskPool.schedule(&Gettid);
    for(int i=0; i < global::TX_THREAD_COUNT * 100; i++) txTaskPool.schedule(&Gettid);
    for(int i=0; i < global::SYNC_BLOCK_THREAD_NUMBER * 100; i++) syncTaskPool.schedule(&Gettid);
    for(int i=0; i < global::SAVE_BLOCK_THREAD_NUMBER * 100; i++) saveBlockTaskPool.schedule(&Gettid);
    for(int i=0; i < global::BLOCK_THREAD_COUNT * 100; i++) blockTaskPool.schedule(&Gettid);
    for(int i=0; i < global::WORK_THREAD_COUNT * 100; i++) workTaskPool.schedule(&Gettid);

    caTaskPool_.wait();
    netTaskPool.wait();
    broadcastTaskPool.wait();
    txTaskPool.wait();
    syncTaskPool.wait();
    saveBlockTaskPool.wait();
    blockTaskPool.wait();
    workTaskPool.wait();

    std::cout << "ThreadNumber:" << threadIds.size() << std::endl;

    for(auto &it : threadIds)
    {
        int index = GetCpuIndex();
        std::ostringstream tid;
        tid << it;
        uint64_t Utid = std::stoul(tid.str(), nullptr, 16);
        setThreadCpu(index, Utid);
    }

    threadIds.clear();
}