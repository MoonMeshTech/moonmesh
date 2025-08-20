/**
 * *****************************************************************************
 * @file        block_stroage.h
 * @brief       
 * @date        2023-09-26
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef _BLOCK_STORAGE_
#define _BLOCK_STORAGE_

#include "ca/block_monitor.h"
#include "utils/magic_singleton.h"
#include "utils/vrf.hpp"
#include <future>
#include <unordered_map>
#include <shared_mutex>
#include <chrono>
#include <thread>

using RetType = std::pair<std::string, uint16_t>;

enum class BroadcastType
{
	verifyBroadcast,
	level1BroadcastMessage
};
struct BlockStatusWrapper
{
	std::string blockHash;
	CBlock block;
	BroadcastType broadcastType;
	uint32_t nodeVerificationCount;
	uint32_t level1_node_count;
	std::vector<BlockStatus> blockStatusList = {};
	std::set<std::string> verifyNodes = {};
	std::set<std::string> level1Nodes = {};
};
class BlockStorage
{
public:
    BlockStorage(){ _StartTimer(); };
    ~BlockStorage() = default;
    BlockStorage(BlockStorage &&) = delete;
    BlockStorage(const BlockStorage &) = delete;
    BlockStorage &operator=(BlockStorage&&) = delete;
    BlockStorage &operator=(const BlockStorage &) = delete;

public:
	/**
	 * @brief       
	 * 
	 */
	void StopTimer(){_blockTimer.Cancel();}

	/**
	 * @brief       
	 * 
	 * @param       msg: 
	 * @return      int 
	 */
	int AddBlock(const BlockMsg &msg);

	/**
	 * @brief       
	 * 
	 * @param       msg: 
	 * @return      int 
	 */
	int UpdateBlock(const BlockMsg &msg);
	/**
	 * @brief       Get the Prehash object
	 * 
	 * @param       height: 
	 * @return      std::shared_future<RetType> 
	 */
	std::shared_future<RetType> GetPrehash(const uint64_t height);

	/**
	 * @brief       
	 * 
	 * @param       seekHeight: 
	 */
	void commitLookupTask(uint64_t seekHeight);

	/**
	 * @brief       
	 * 
	 * @param       seekHeight: 
	 */
	void ForceCommitSeekJob(uint64_t seekHeight);

	/**
	 * @brief       
	 * 
	 */
	void clearPreHashMap();

	/**
	 * @brief       
	 * 
	 * @param       seekHeight: 
	 * @return      true 
	 * @return      false 
	 */
	bool isSeekTask(uint64_t seekHeight);

	/**
	 * @brief       
	 * 
	 * @param       blockHash: 
	 */
	void newBuildBlockByBlockStatus(const std::string blockHash);

	/**
	 * @brief       
	 * 
	 * @param       blockStatus: 
	 */
	void AddBlockStatus(const BlockStatus& blockStatus);

	/**
	 * @brief       
	 * 
	 * @param       blockHash: 
	 * @param       Block: 
	 * @param       level1Nodes: 
	 */
	void AddBlockStatus(const std::string& blockHash, const CBlock& Block, const std::set<std::string>& level1Nodes);
	
	/**
	 * @brief       
	 * 
	 * @param       blockHash: 
	 * @param       Block: 
	 * @param       verifyNodes: 
	 * @param       vrf: 
	 */
	void AddBlockStatus(const std::string& blockHash, const CBlock& Block, const std::vector<std::string>& verifyNodes);
	
	/**
	 * @brief       
	 * 
	 */
	void checkExpiredDelete();

	/**
	 * @brief       
	 * 
	 * @param       blockStatus: 
	 * @return      int 
	 */
	int CheckData(const BlockStatus& blockStatus);

	/**
	 * @brief       
	 * 
	 * @param       oldBlock: 
	 * @param       newBlock: 
	 * @return      int 
	 */
	int InitNewBlock(const CBlock& oldBlock, CBlock& newBlock);

private:

	/**
	 * @brief       
	 * 
	 */
	void _StartTimer();

	/**
	 * @brief       
	 * 
	 */
	void _BlockCheck();

	/**
	 * @brief       
	 * 
	 * @param       msgVec: 
	 * @param		outMsg:
	 * @param		isVrf
	 * @return		int
	 */
	int composeEndBlockMessage(const std::vector<BlockMsg> &msgVec, BlockMsg & outMsg , bool isVrf);

	/**
	 * @brief       
	 * 
	 * @param       hash: 
	 */
	void _Remove(const std::string &hash);

	/**
	 * @brief       
	 * 
	 * @param       seekHeight: 
	 * @return      RetType 
	 */
	RetType seekPreHashThread(uint64_t seekHeight);
	/**
	 * @brief       
	 * 
	 * @param       blockMsg: 
	 * @return      int 
	 */
	int BlockFlowSignVerifier(const BlockMsg & blockMsg);

	/**
	 * @brief       
	 * 
	 * @param       node_ids_to_send: 
	 * @param       seekHeight: 
	 * @param       nodeSelfHeight: 
	 * @return      RetType 
	 */
	RetType seekPreHashByNode(
		const std::vector<std::string> &node_ids_to_send, uint64_t seekHeight, const uint64_t &nodeSelfHeight);
private:
	/**
	 * @brief       
	 * 
	 * @param       where: 
	 * @return      std::string 
	 */
    friend std::string PrintCache(int where);
	CTimer _blockTimer;
	mutable std::shared_mutex _blockMutex;
	std::map<std::string , std::vector<BlockMsg>> _blockCnt;

	mutable std::shared_mutex prehashMutex;
	std::map<uint64_t, std::shared_future<RetType>> preHashMap;

	std::mutex _statusMutex;
	std::map<std::string, BlockStatusWrapper> blockStatusMap;

	double _failureRate = 0.75;
};
/**
 * @brief       
 * 
 * @param       num:
 * @param       nodeSelfHeight:
 * @param       pledgeAddr:
 * @param       node_ids_to_send:
 * @return      int 
 */
int get_prehash_find_node(uint32_t num, uint64_t nodeSelfHeight, const std::vector<std::string> &pledgeAddr,
                            std::vector<std::string> &node_ids_to_send);
/**
 * @brief       
 * 
 * @param       nodeId:
 * @param       msgId: 
 * @param       seekHeight:  
 */
void sendSeekGetPreHashRequest(const std::string &nodeId, const std::string &msgId, uint64_t seekHeight);
/**
 * @brief       
 * 
 * @param       ack:
 * @param       nodeId: 
 * @param       msgId:
 * @param       seekHeight:    
 */
void sendPreHashAcknowledgment(SeekPreHashByHightAck& ack, const std::string &nodeId, const std::string &msgId, uint64_t seekHeight);
/**
 * @brief       
 * 
 * @param       msg:
 * @param       msgData:
 * @return      int
 */
int handleSeekGetPreHashRequest(const std::shared_ptr<SeekPreHashByHightReq> &msg, const MsgData &msgData);
/**
 * @brief       
 * 
 * @param       msg:
 * @param       msgData: 
 * @return      int 
 */
int preHashAcknowledgment(const std::shared_ptr<SeekPreHashByHightAck> &msg, const MsgData &msgData);
/**
 * @brief       
 * 
 * @param       msg:
 * @param       destNode: 
 * @return      int  
 */
int protocolBlockStatus(const BlockStatus& blockStatus, const std::string destNode);
/**
 * @brief       
 * 
 * @param       msg:
 * @param       msgData: 
 * @return      int 
 */
int blockStatusMessageHandler(const std::shared_ptr<BlockStatus> &msg, const MsgData &msgData);
#endif