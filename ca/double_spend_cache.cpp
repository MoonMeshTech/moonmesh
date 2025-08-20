#include "ca/double_spend_cache.h"

int doubleSpendCache::AddFromAddr(const std::pair<std::string,doubleSpendCache::doubleSpendSuccess> &usings)
{
    std::unique_lock<std::mutex> lck(doubleSpendMutex);
	auto it =  _pending.find(usings.first);
	if(it != _pending.end())
	{
		std::vector<std::string> usingUtxo(usings.second.utxoVector.begin(), usings.second.utxoVector.end());
		std::vector<std::string> pendingUtxos(it->second.utxoVector.begin(), it->second.utxoVector.end());
		std::sort(usingUtxo.begin(),usingUtxo.end());
		std::sort(pendingUtxos.begin(),pendingUtxos.end());

		std::vector<std::string> vIntersection;
		std::set_intersection(usingUtxo.begin(), usingUtxo.end(), pendingUtxos.begin(), pendingUtxos.end(), std::back_inserter(vIntersection));

		for(auto & diff : vIntersection)
		{
			ERRORLOG("utxo:{} is using!",diff);
			std::cout << "utxo:" << diff << "is using!\n";
			return -1;
		}

	}else{
		_pending.insert(usings);
	}

	return 0;
}


void doubleSpendCache::Remove(const uint64_t& txTimeKey)
{

	for(auto iter = _pending.begin(); iter != _pending.end();)
	{
		if (iter->second.time == txTimeKey)
		{
			iter = _pending.erase(iter);
		}
		else
		{
			iter++;
		}
	}  
	
}

void doubleSpendCache::CheckLoop()
{
	std::unique_lock<std::mutex> lck(doubleSpendMutex);
	std::vector<uint64_t> toRemove;
	for(auto & item : _pending)
	{
	    const int64_t kTenSecond = (int64_t)1000000 * 30;
		uint64_t time = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
		if(item.second.time + kTenSecond < time)
		{
			toRemove.push_back(item.second.time);
		}
	}

	for(auto & txTimeKey: toRemove)
	{
		Remove(txTimeKey);
	}
}



void doubleSpendCache::Detection(const CBlock & block)
{
	std::unique_lock<std::mutex> lck(doubleSpendMutex);
	std::vector<uint64_t> toRemove;
	for(auto & tx : block.txs())
	{	
		for(const auto & utxo: tx.utxos())
		{
			if(!utxo.owner().empty())
			{
			auto it = _pending.find(utxo.owner(0));
			if(it != _pending.end() && tx.time() == it->second.time)
			{	
				toRemove.push_back(it->second.time);
				DEBUGLOG("Remove pending txhash : {}",tx.hash());
				break;
			}
			}
		}
	}

	for(auto & txTimeKey: toRemove)
	{
		Remove(txTimeKey);
	}
}