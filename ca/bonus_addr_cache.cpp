#include "bonus_addr_cache.h"
#include <memory>
#include "include/logging.h"
#include "ca/global.h"
#include "db/db_api.h"

namespace mm
{
    namespace ca
    {
        int BonusAddrCache::getAmount(const std::string& bonus_addr, uint64_t& amount)
        {
            amount = 0;
            std::unique_lock<std::shared_mutex> lock(bonus_addr_mutex_);
            if(!bonus_addr_map_[bonus_addr].dirty)
            {
                auto it = bonus_addr_map_.find(bonus_addr);
                if(it != bonus_addr_map_.end())
                {
                    amount = it->second.amount;
                }
            }
            
            DEBUGLOG("bonusAddr:{}, amount is {}",bonus_addr, amount);

            if(amount >= global::ca::kMinDelegatingAmt)
            {
                return 0;
            }

            bonus_addr_map_[bonus_addr].utxos.clear();

            DBReader db_reader;
            std::multimap<std::string, std::string> delegating_addrs_currency;
            auto ret = db_reader.getDelegatingAddrByBonusAddr(bonus_addr, delegating_addrs_currency);
            if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND)
            {
                DEBUGLOG("DBStatus is {}",ret);
                return -1;
            }

            if(delegating_addrs_currency.empty())
            {
                return -2;
            }

            uint64_t sum_delegating_amount = 0;
            bool flag = true;
            for (const auto& [delegating_addr, currency] : delegating_addrs_currency)
            {
                std::vector<std::string> utxos;
                ret = db_reader.getBonusAddrDelegatingAddrUtxoByBonusAddr(bonus_addr, delegating_addr, currency, utxos);
                if (ret != DBStatus::DB_SUCCESS && ret != DBStatus::DB_NOT_FOUND)
                {
                    ERRORLOG("getBonusAddrDelegatingAddrUtxoByBonusAddr error, delegatingAddr : {} , currency : {}", delegating_addr, currency);
                    return -2;
                }

                for (const auto &hash : utxos)
                {
                    std::string strTx;
                    if (db_reader.getTransactionByHash(hash, strTx) != DBStatus::DB_SUCCESS)
                    {
                        ERRORLOG("getTransactionByHash error , hash: {}", hash);
                        return -3;
                    }
                    bonus_addr_map_[bonus_addr].utxos.insert(hash);

                    CTransaction tx;
                    if (!tx.ParseFromString(strTx))
                    {
                        DEBUGLOG("parseFromString error");
                        return -4;
                    }
                    for(auto utxo: tx.utxos())
                    {
                        for (int i = 0; i < utxo.vout_size(); i++)
                        {
                            if (utxo.vout(i).addr() == global::ca::kVirtualDelegatingAddr)
                            {
                                sum_delegating_amount += utxo.vout(i).value();
                                break;
                            }
                        }
                    }
                    if(sum_delegating_amount >= global::ca::kMinDelegatingAmt && flag)
                    {
                        bonus_addr_map_[bonus_addr].time = tx.time();
                        flag = false;
                    }
                }
            }
            bonus_addr_map_[bonus_addr].amount = sum_delegating_amount;
            bonus_addr_map_[bonus_addr].dirty = false;
            amount = sum_delegating_amount;
            return 0;
        }

        uint64_t BonusAddrCache::getTime(const std::string& bonus_addr)
        {
            std::shared_lock<std::shared_mutex> lock(bonus_addr_mutex_);
            auto it = bonus_addr_map_.find(bonus_addr);
            if(it != bonus_addr_map_.end())
            {
                return it->second.time;
            }
            else
            {
                return 0;
            }
        }

        void BonusAddrCache::isDirty(const std::string& bonus_addr, bool dirty)
        {
            std::unique_lock<std::shared_mutex> lock(bonus_addr_mutex_);
            bonus_addr_map_[bonus_addr].dirty = dirty;
            return;
        }
    }
}