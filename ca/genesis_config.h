#ifndef __GENESIS_CONFIG_H__
#define __GENESIS_CONFIG_H__

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "iostream"
namespace GenesisConfig {

struct NetworkInfo {
    std::string name;
    std::string type;  // primary, test, dev
};

struct GenesisInfo {
    std::string initAccountAddr;
    uint64_t genesisTime;
    std::map<std::string, uint64_t> initialBalance;  // assetType -> balance
    std::map<std::string, std::string> blockData;    // block data fields
};

struct FullGenesisConfig {
    std::string version;
    NetworkInfo network;
    GenesisInfo genesis;
};
    
enum class BuildType 
{
    BUILD_TYPE_PRIMARY,
    K_BUILD_TYPE_TEST,
    BUILD_TYPE_DEV
};

class GenesisConfigManager {
public:
    static GenesisConfigManager& GetInstance();
    
    // Load configuration file
    bool LoadFromFile(const std::string& configPath);
    
    // Load the built-in configuration
    bool LoadBuiltinConfig(const std::string& networkType);
    
    // Verify configuration
    bool ValidateConfig() const;
    
    // get configuration
    const FullGenesisConfig& GetConfig() const { return config_; }
    const std::string& GetVersion() const { return config_.version; }
    const NetworkInfo& GetNetworkInfo() const { return config_.network; }
    const GenesisInfo& GetGenesisInfo() const { return config_.genesis; }
    
    // Generate a JSON string for configuration
    std::string GenerateConfigJson() const;
    
    // Has the configuration been loaded?
    bool IsConfigLoaded() const { return configLoaded_; }

    BuildType GetBuildType();
    uint64_t GetGenesisTime();
    std::string GetInitAccountAddr();
    std::string GetVersion();
    int SetInitAccountAddr(const std::string &configJson);
    int SetGenesisTime(const std::string &configJson);
    int SetBuildType(const std::string &configJson);
    int SetVersion();

private:
    GenesisConfigManager() = default;
    ~GenesisConfigManager() = default;
    GenesisConfigManager(const GenesisConfigManager&) = delete;
    GenesisConfigManager& operator=(const GenesisConfigManager&) = delete;
    
    bool ParseJsonConfig(const nlohmann::json& jsonConfig);
    
    void SetDefaultValues();
    void SetPrimaryGenesisDefaults();
    
    // Verification function
    bool ValidateGenesisAddress(const std::string& address) const;
    bool ValidateGenesisTime(uint64_t timestamp) const;
    bool ValidateInitialBalance(const std::map<std::string, uint64_t>& balances) const;
    
    // Obtain the built-in configuration
    nlohmann::json GetBuiltinConfigJson(const std::string& networkType);


private:
    FullGenesisConfig config_;
    bool configLoaded_ = false;
    inline static BuildType buildType_ = BuildType::BUILD_TYPE_PRIMARY ;
    inline static std::string kInitAccountAddr_ = "De8AB34f772f21940DE50583A8d2756bFD0e4FD7";
    inline static uint64_t kGenesisTime_ = 1700000000000000;
    inline static std::string kVersion_ = "0.0.0_p";
};
} // namespace ca

#endif // __GENESIS_CONFIG_H__ 