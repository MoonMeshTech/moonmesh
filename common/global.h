#ifndef _GLOBAL_H
#define _GLOBAL_H
#include <string>
#include <atomic>
#include <iostream>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include "ca/genesis_config.h"

using json = nlohmann::json;

namespace global
{
    // ============== constant definition ==============
    static const std::string kNetVersion = "0.0";
    static const std::string kIOSCompatible = "4.0.4";
    static const std::string ANDROID_COMPATIBLE = "3.1.0";
    static const std::string LINUX_COMPATIBLE = "0.0.0";
    static const std::string WINDOWS_COMPATIBLE = "0.0.0";



    // Thread pool configuration
    static const int CA_THREAD_NUMBER = 15;
    static const int K_NET_THREAD_NUMBER = 15;
    static const int kBraodcastThreadNumber = 10;
    static const int TX_THREAD_COUNT = 50;
    static const int SYNC_BLOCK_THREAD_NUMBER = 25;
    static const int SAVE_BLOCK_THREAD_NUMBER = 50;
    static const int BLOCK_THREAD_COUNT = 50;
    static const int WORK_THREAD_COUNT = 50;

    #if WINDOWS
        static const std::string kSystem = "2";
        static const std::string kCompatibleVersion = WINDOWS_COMPATIBLE;
    #else
        static const std::string kSystem = "1";
        static const std::string kCompatibleVersion = LINUX_COMPATIBLE;
    #endif

    enum class BuildType {
        BUILD_TYPE_PRIMARY,
        K_BUILD_TYPE_TEST,
        BUILD_TYPE_DEV
    };



    // ============== Global Access Interface ==============
inline GenesisConfig::BuildType GetBuildType()
{
    return GenesisConfig::GenesisConfigManager::GetInstance().GetBuildType();
}

inline std::string GetVersion() {
    return GenesisConfig::GenesisConfigManager::GetInstance().GetVersion();
}
}

#endif // !_GLOBAL_H