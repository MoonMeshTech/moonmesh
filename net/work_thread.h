/**
 * *****************************************************************************
 * @file        work_thread.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef WORK_THREAD_HEADER_GUARD
#define WORK_THREAD_HEADER_GUARD

#include <thread>
#include <vector>
#include <list>
#include <deque>

#include "./msg_queue.h"
#include "../common/config.h"

class WorkThreads
{
public:
	WorkThreads() = default;
	~WorkThreads() = default;

	/**
	 * @brief       Processing network messages
	 * 
	 * @param       data 
	 * @return      int 
	 */
	static int handle_net_read(const MsgData& data);

	/**
	 * @brief       
	 * 
	 * @param       data 
	 * @return      int 
	 */
	static int networkReadHandler(MsgData& data);

	/**
	 * @brief       
	 * 
	 * @param       data 
	 * @return      true 
	 * @return      false 
	 */
	static bool handle_net_write(const MsgData& data);

	/**
	 * @brief       
	 * 
	 * @param       num 
	 */
	static void Work(int num);

	/**
	 * @brief       
	 * 
	 * @param       id 
	 */
	static void WorkRead(int id);

	/**
	 * @brief       
	 * 
	 * @param       id 
	 */
	static void WorkWrite(int id);

	/**
	 * @brief       
	 * 
	 */
	void Start();
private:
    friend std::string PrintCache(int where);
	std::vector<std::thread> threadsWorkList;
	std::vector<std::thread> threadsReadList;
	std::vector<std::thread> threadsTransList;
};

#endif