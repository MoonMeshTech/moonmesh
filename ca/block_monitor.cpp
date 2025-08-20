#include "block_monitor.h"

#include "utils/console.h"
#include "utils/vrf.hpp"
#include "db/db_api.h"
#include "transaction.h"

uint32_t BlockMonitor::_maxSendSize = 100;
void BlockMonitor::transactionSuccessRateChecker()
{
	std::unique_lock<std::mutex> lck(mutex_);
	DBReader reader;
	int DROP_SHIPPING_TX_VEC_FAIL = 0;
	std::vector<std::string> failedTxHashes;
	for(const auto& txHash : dropshipping_tx_vec)
	{
		std::string blockHash;
		reader.getBlockHashByTransactionHash(txHash, blockHash);

		if (blockHash.empty())
		{
			failedTxHashes.push_back(txHash);
			DEBUGLOG("failedTxHashes, txHash:{}", txHash);
			DROP_SHIPPING_TX_VEC_FAIL++;
			continue;
		}
	}
	
	std::ofstream outputFile("./failTxhash.txt");
	if (outputFile.is_open())
	{
		for(const auto& it : failedTxHashes)
		{
			outputFile << it << "\n";
		}
		outputFile.close();
	}
	double successRate = (1 - static_cast<double>(DROP_SHIPPING_TX_VEC_FAIL) / dropshipping_tx_vec.size()) * 100;
	DEBUGLOG(GREEN "dropshippingTxVec Txsize: {}, DROP_SHIPPING_TX_VEC_FAIL size:{}, SuccessRate:{}%" RESET, dropshipping_tx_vec.size(), DROP_SHIPPING_TX_VEC_FAIL, successRate );
	std::cout<< GREEN << "dropshippingTxVec Txsize:" << dropshipping_tx_vec.size() << RESET << std::endl;
	std::cout<< GREEN <<"DROP_SHIPPING_TX_VEC_FAIL size:" << DROP_SHIPPING_TX_VEC_FAIL<< RESET << std::endl;
	std::cout<< GREEN <<"SuccessRate:" << successRate << "%" << RESET << std::endl;
	dropshipping_tx_vec.clear();

	int handleTransactionVectorFailure = 0;
	for(const auto& txHash : handleTransactionVector)
	{
		std::string blockHash;
		reader.getBlockHashByTransactionHash(txHash, blockHash);

		if (blockHash.empty())
		{
			handleTransactionVectorFailure++;
			continue;
		}
	}
	DEBUGLOG(GREEN "addDoHandleTransactionVector Txsize: {}, DoHandleTxVecFailFail size:{}" RESET, handleTransactionVector.size(), handleTransactionVectorFailure);
	handleTransactionVector.clear();
}

void BlockMonitor::dropshippingTxVec(const std::string& txHash)
{
	std::unique_lock<std::mutex> lck(mutex_);
	dropshipping_tx_vec.push_back(txHash);
}
void BlockMonitor::addDoHandleTransactionVector(const std::string& txHash)
{
	std::unique_lock<std::mutex> lck(mutex_);
	handleTransactionVector.push_back(txHash);
}


int BlockMonitor::sendBroadcastAddBlockRequest(std::string strBlock, uint64_t blockHeight)
{
    std::vector<Node> list;
	list = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
	{
		// Filter nodes that meet the height
		std::vector<Node> tmpList;
		if (blockHeight != 0)
		{
			for (auto & item : list)
			{
				if (item.height >= blockHeight - 1)
				{
					tmpList.push_back(item);
				}
			}
			list = tmpList;
		}
	}

    BuildBlockBroadcastMsg constructBlockBroadcastMessage;
    constructBlockBroadcastMessage.set_version(global::GetVersion());
	constructBlockBroadcastMessage.set_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
	constructBlockBroadcastMessage.set_blockraw(strBlock);
    constructBlockBroadcastMessage.set_flag(1);
	
    bool isSucceed = net_com::broadcastMessage(constructBlockBroadcastMessage);
	if(isSucceed == false)
	{
        ERRORLOG("Broadcast BuildBlockBroadcastMsg failed!");
        return -3;
	}

	std::set<std::string> cast_address;
	for(auto addr : constructBlockBroadcastMessage.castaddrs()){
		cast_address.insert(addr);
	}

	CBlock block;
    block.ParseFromString(strBlock);
	MagicSingleton<BlockStorage>::GetInstance()->AddBlockStatus(block.hash(), block, cast_address);

	DEBUGLOG("***********net broadcast time{}",MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
	return 0;
}
