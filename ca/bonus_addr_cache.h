#ifndef BONUS_ADDR_CACHE_H
#define BONUS_ADDR_CACHE_H

#include <cstdint>
#include <set>
#include <string>
#include <shared_mutex>
#include <map>

namespace mm
{
    namespace ca
    {
        struct BonusAddrInfo
        {
            bool dirty{true}; 
            uint64_t amount;                //Amount delegatinged
            std::set<std::string> utxos;    //utxos delegatinged
            uint64_t time;                  //Qualified Delegating time
        };

        class BonusAddrCache
        {
        public:
            /**
             * @description: 
             * @param {string&} bonus_addr
             * @param {uint64_t&} amount
             * @return {*}
             */            
            int getAmount(const std::string& bonus_addr, uint64_t& amount);
            /**
             * @description: 
             * @param {string&} bonus_addr
             * @return {*}
             */            
            uint64_t getTime(const std::string& bonus_addr);
            /**
             * @description: 
             * @param {string&} bonus_addr
             * @param {bool} dirty
             * @return {*}
             */            
            void isDirty(const std::string& bonus_addr, bool dirty = true);
            
        private:
            mutable std::shared_mutex bonus_addr_mutex_;
            std::map<std::string, BonusAddrInfo> bonus_addr_map_;
        };

    }
}


#endif // BONUS_ADDR_CACHE_H