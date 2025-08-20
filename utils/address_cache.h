#ifndef ADDRESS_CACHE_H
#define ADDRESS_CACHE_H

#include <unordered_map>
#include <string>
#include <mutex>

namespace mm 
{
    namespace addr
    {
        class AddressCache 
        {
            private:
            std::unordered_map<std::string, std::string> cache;
            std::mutex mutex;

            public:
            void addPublicKey(const std::string& publicKey, const std::string& address) 
            {
                std::lock_guard<std::mutex> lock(mutex);
                cache[publicKey] = address;
            }

            std::string getAddress(const std::string& publicKey) 
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (cache.count(publicKey) > 0) {
                    return cache[publicKey];
                }
                return "";
            }
        };
    } // addr
} // mm 

#endif //ADDRESS_CACHE_H