#ifndef threadHandle
#define threadHandle

#include <thread>

/**
 * @brief       
 * 
 * @param       cpuIndex 
 * @param       tid 
 */
void setThreadCpu(unsigned int cpuIndex);
void setThreadCpu(unsigned int cpuIndex, pthread_t tid);

/**
 * @brief       Get the Cpu Index object
 * 
 * @return      int 
 */
int GetCpuIndex();


#endif threadHandle