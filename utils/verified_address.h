#ifndef VERIFIED_ADDRESS_H
#define VERIFIED_ADDRESS_H

#include <unordered_set>
#include <string>
#include <mutex>

namespace mm
{
    namespace addr
    {
        class VerifiedAddress
        {
            private:
                std::unordered_set<std::string> verifiedAddresses;
                std::mutex mutex;

            public:
                void markAddressAsVerified(const std::string& address) 
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    verifiedAddresses.insert(address);
                }

                bool isAddressVerified(const std::string& address) 
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    return verifiedAddresses.count(address) > 0;
                }
        };
    }
}



#endif // VERIFIED_ADDRESS_H