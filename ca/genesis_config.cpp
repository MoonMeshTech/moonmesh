#include "genesis_config.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include "include/logging.h"
#include "utils/util.h"
#include "ca/global.h"
#include "utils/account_manager.h"

namespace GenesisConfig
{

    GenesisConfigManager &GenesisConfigManager::GetInstance()
    {
        static GenesisConfigManager instance;
        return instance;
    }

    bool GenesisConfigManager::LoadFromFile(const std::string &configPath)
    {
        try
        {
            std::ifstream configFile(configPath);
            if (!configFile.is_open())
            {
                ERRORLOG("Failed to open config file: {}", configPath);
                return false;
            }

            nlohmann::json jsonConfig;
            configFile >> jsonConfig;
            configFile.close();

            return ParseJsonConfig(jsonConfig);
        }
        catch (const std::exception &e)
        {
            ERRORLOG("Failed to load config from file {}: {}", configPath, e.what());
            return false;
        }
    }

    bool GenesisConfigManager::LoadBuiltinConfig(const std::string &networkType)
    {
        try
        {
            if (networkType == "test" || networkType == "dev")
            {
                ERRORLOG("Test/Dev networks require custom configuration file. Use --network <file> instead of --network-type {}", networkType);
                std::cout << "Error: Test/Dev networks require custom configuration!" << std::endl;
                std::cout << "Please create a JSON config file with your custom genesis parameters." << std::endl;
                std::cout << "Example for " << networkType << " network:" << std::endl;

                // Generate sample configuration and display
                nlohmann::json exampleConfig = GetBuiltinConfigJson(networkType);
                // Provide an actually usable template structure
                exampleConfig["genesis"]["initAccountAddr"] = "";
                exampleConfig["genesis"]["genesisTime"] = 0;
                exampleConfig["genesis"]["initialBalance"].clear();
                exampleConfig["genesis"]["initialBalance"]["Vote"] = 0;
                exampleConfig["genesis"]["initialBalance"]["MM"] = 0;
                exampleConfig["genesis"]["blockData"]["Name"] = networkType + "Chain";
                exampleConfig["genesis"]["blockData"]["Type"] = "Genesis";

                std::cout << exampleConfig.dump(4) << std::endl;
                std::cout << "\nSave this to a file (e.g., " << networkType << "_config.json) and use:" << std::endl;
                std::cout << "  --network " << networkType << "_config.json" << std::endl;
                return false;
            }

            nlohmann::json builtinConfig = GetBuiltinConfigJson(networkType);
            return ParseJsonConfig(builtinConfig);
        }
        catch (const std::exception &e)
        {
            ERRORLOG("Failed to load builtin config for {}: {}", networkType, e.what());
            return false;
        }
    }

    bool GenesisConfigManager::ParseJsonConfig(const nlohmann::json &jsonConfig)
    {
        try
        {
            // Parse version information
            config_.version = jsonConfig.value("version", "1.0.0");

            // Parse network information - Strictly validate required fields
            if (!jsonConfig.contains("network"))
            {
                ERRORLOG("Missing required 'network' section in config");
                return false;
            }

            const auto &networkJson = jsonConfig["network"];

            // Check the necessary network fields
            if (!networkJson.contains("name"))
            {
                ERRORLOG("Missing required field 'network.name' in config");
                return false;
            }
            if (!networkJson.contains("type"))
            {
                ERRORLOG("Missing required field 'network.type' in config");
                return false;
            }

            config_.network.name = networkJson["name"];
            std::string networkType = networkJson["type"];

            std::transform(networkType.begin(), networkType.end(), networkType.begin(), ::tolower);
            config_.network.type = networkType;

            // Determine the genesis block configuration strategy based on the type of network.
            if (config_.network.type == "primary")
            {
                INFOLOG("Primary network detected: using hardcoded genesis parameters");
                SetPrimaryGenesisDefaults();
            }
            else
            {
                // Test/Dev network: Use the genesis configuration in JSON
                INFOLOG("Non-primary network ({}): using configurable genesis parameters", config_.network.type);

                // Analyzing Creation Information - Strictly Verifying Required Fields
                if (!jsonConfig.contains("genesis"))
                {
                    ERRORLOG("Missing required 'genesis' section in config for non-primary network");
                    return false;
                }

                const auto &genesisJson = jsonConfig["genesis"];

                // Check the necessary creation fields
                if (!genesisJson.contains("initAccountAddr"))
                {
                    ERRORLOG("Missing required field 'genesis.initAccountAddr' in config");
                    return false;
                }
                if (!genesisJson.contains("genesisTime"))
                {
                    ERRORLOG("Missing required field 'genesis.genesisTime' in config");
                    return false;
                }

                config_.genesis.initAccountAddr = genesisJson["initAccountAddr"];
                if (config_.genesis.initAccountAddr.substr(0, 2) == "0x")
                {
                    config_.genesis.initAccountAddr = config_.genesis.initAccountAddr.substr(2);
                }
                config_.genesis.genesisTime = genesisJson["genesisTime"];

                // Optional field: initialBalance
                if (genesisJson.contains("initialBalance"))
                {
                    const auto &balanceJson = genesisJson["initialBalance"];
                    for (auto it = balanceJson.begin(); it != balanceJson.end(); ++it)
                    {
                        config_.genesis.initialBalance[it.key()] = it.value();
                    }
                }

                // Optional field: blockData
                if (genesisJson.contains("blockData"))
                {
                    const auto &blockDataJson = genesisJson["blockData"];
                    for (auto it = blockDataJson.begin(); it != blockDataJson.end(); ++it)
                    {
                        config_.genesis.blockData[it.key()] = it.value();
                    }
                }
            }

            // Set default values (only for optional fields)
            SetDefaultValues();

            configLoaded_ = true;
            return ValidateConfig();
        }
        catch (const std::exception &e)
        {
            ERRORLOG("Failed to parse JSON config: {}", e.what());
            return false;
        }
    }

    void GenesisConfigManager::SetDefaultValues()
    {

        if (config_.genesis.initialBalance.empty())
        {
            config_.genesis.initialBalance["Vote"] = 0;
            config_.genesis.initialBalance["MM"] = 0;
        }

        // Set default values for block data (if empty)
        if (config_.genesis.blockData.empty())
        {
            config_.genesis.blockData["Name"] = "MoonMesh";
            config_.genesis.blockData["Type"] = "Genesis";
        }
    }

    void GenesisConfigManager::SetPrimaryGenesisDefaults()
    {

        config_.genesis.initAccountAddr = global::ca::GetInitAccountAddr();
        config_.genesis.genesisTime = global::ca::GetGenesisTime();

        config_.genesis.initialBalance.clear();
        config_.genesis.initialBalance["Vote"] = 0;
        config_.genesis.initialBalance["MM"] = 0;
        config_.genesis.blockData.clear();
        config_.genesis.blockData["Name"] = "MoonMesh";
        config_.genesis.blockData["Type"] = "Genesis";
    }

    bool GenesisConfigManager::ValidateConfig() const
    {
        // Verify the network information
        if (config_.network.name.empty())
        {
            ERRORLOG("Network name is empty");
            return false;
        }

        if (config_.network.type != "primary" && config_.network.type != "test" && config_.network.type != "dev")
        {
            ERRORLOG("Invalid network type: {}", config_.network.type);
            return false;
        }

        // Verify the creation information
        if (!ValidateGenesisAddress(config_.genesis.initAccountAddr))
        {
            return false;
        }

        if (!ValidateGenesisTime(config_.genesis.genesisTime))
        {
            return false;
        }

        if (!ValidateInitialBalance(config_.genesis.initialBalance))
        {
            return false;
        }

        return true;
    }

    bool GenesisConfigManager::ValidateGenesisAddress(const std::string &address) const
    {
        if (address.empty())
        {
            ERRORLOG("Genesis account address is empty");
            std::cout << "❌ Validation Error: Genesis account address is empty" << std::endl;
            std::cout << "   Please provide a valid 40-character hexadecimal address." << std::endl;
            std::cout << "   You can generate a new address using: ./mm --genkey" << std::endl;
            std::cout << "   Or use an existing Ethereum-compatible address." << std::endl;
            return false;
        }

        std::string cleanAddr = address;
        if (cleanAddr.substr(0, 2) == "0x" || cleanAddr.substr(0, 2) == "0X")
        {
            cleanAddr = cleanAddr.substr(2);
        }

        if (!isValidAddress(cleanAddr))
        {
            ERRORLOG("Invalid genesis address format: {}", address);
            std::cout << "❌ Validation Error: Invalid genesis address format" << std::endl;
            std::cout << "   Address: \"" << address << "\"" << std::endl;
            std::cout << "   Expected: 40 hexadecimal characters (0-9, a-f, A-F)" << std::endl;
            std::cout << "   Generate a valid address: ./mm --genkey" << std::endl;
            return false;
        }

        INFOLOG("Genesis address validation passed: {}", address);
        return true;
    }

    bool GenesisConfigManager::ValidateGenesisTime(uint64_t timestamp) const
    {
        if (timestamp == 0)
        {
            ERRORLOG("Genesis time is not set. Please provide a valid timestamp for test/dev network.");
            std::cout << "❌ Validation Error: Genesis time is not set" << std::endl;
            std::cout << "   Please provide a valid timestamp in microseconds." << std::endl;
            uint64_t currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
            std::cout << "   Current time: " << currentTime << std::endl;
            std::cout << "   Generate timestamp: date +%s000000  # Unix timestamp in microseconds" << std::endl;
            std::cout << "   Or use any valid timestamp between 1 and " << currentTime << std::endl;
            return false;
        }

        // Get the current time (in microsecond level)
        uint64_t current_timestamp = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();

        // Check if the timestamp is greater than 1970-01-01 00:00:00 UTC (the minimum timestamp is 0 microseconds)
        uint64_t min_timestamp = 0;

        if (timestamp < min_timestamp)
        {
            ERRORLOG("Genesis timestamp is invalid: {} (must be after 1970-01-01 00:00:00 UTC)", timestamp);
            std::cout << "❌ Validation Error: Genesis timestamp is invalid" << std::endl;
            std::cout << "   Provided: " << timestamp << std::endl;
            std::cout << "   Must be after 1970-01-01 00:00:00 UTC (microseconds)" << std::endl;
            return false;
        }

        // Check if the timestamp is less than or equal to the current time
        if (timestamp > current_timestamp)
        {
            ERRORLOG("Genesis timestamp is in the future: {} (current time: {})", timestamp, current_timestamp);
            std::cout << "❌ Validation Error: Genesis timestamp is in the future" << std::endl;
            std::cout << "   Provided: " << timestamp << std::endl;
            std::cout << "   Current time: " << current_timestamp << std::endl;
            std::cout << "   Please use a timestamp that is not in the future." << std::endl;
            return false;
        }

        INFOLOG("Genesis time validation passed: {}", timestamp);
        return true;
    }

    bool GenesisConfigManager::ValidateInitialBalance(const std::map<std::string, uint64_t> &balances) const
    {
        if (balances.empty())
        {
            ERRORLOG("Initial balance is empty. Please provide at least one asset.");
            std::cout << "❌ Validation Error: Initial balance is empty" << std::endl;
            std::cout << "   Please provide at least one asset with initial balance." << std::endl;
            std::cout << "   Common format: {\"Vote\": 0, \"MM\": 0}" << std::endl;
            return false;
        }

        for (const auto &balance : balances)
        {
            // Check the asset name
            if (balance.first.empty())
            {
                ERRORLOG("Asset name cannot be empty");
                std::cout << "❌ Validation Error: Asset name cannot be empty" << std::endl;
                return false;
            }

            // Asset name verification (only allows letters, numbers, and underscores)
            for (char c : balance.first)
            {
                if (!std::isalnum(c) && c != '_')
                {
                    ERRORLOG("Invalid character in asset name '{}': {}", balance.first, c);
                    std::cout << "❌ Validation Error: Invalid character in asset name" << std::endl;
                    std::cout << "   Asset name: \"" << balance.first << "\"" << std::endl;
                    std::cout << "   Invalid character: '" << c << "'" << std::endl;
                    std::cout << "   Asset names can only contain letters, numbers, and underscores." << std::endl;
                    return false;
                }
            }

            // The balance must not exceed the reasonable limit (to prevent overflow)
            if (balance.second > 1000000000000000000ULL)
            { // 10^18
                ERRORLOG("Initial balance too large for asset '{}': {}", balance.first, balance.second);
                std::cout << "❌ Validation Error: Initial balance too large" << std::endl;
                std::cout << "   Asset: \"" << balance.first << "\"" << std::endl;
                std::cout << "   Balance: " << balance.second << std::endl;
                std::cout << "   Maximum allowed: 1000000000000000000 (10^18)" << std::endl;
                return false;
            }

            INFOLOG("Asset validation passed: {} = {}", balance.first, balance.second);
        }

        return true;
    }

    nlohmann::json GenesisConfigManager::GetBuiltinConfigJson(const std::string &networkType)
    {
        nlohmann::json config;

        // version information
        config["version"] = "1.0.0";

        // basic cofiguration
        config["network"]["name"] = networkType;
        config["network"]["type"] = networkType;

        // Generate different genesis configurations based on the network type
        if (networkType == "primary")
        {
            config["genesis"]["initAccountAddr"] = global::ca::GetInitAccountAddr();
            config["genesis"]["genesisTime"] = global::ca::GetGenesisTime();
            config["genesis"]["initialBalance"]["Vote"] = 0;
            config["genesis"]["initialBalance"]["MM"] = 0;
            config["genesis"]["blockData"]["Name"] = "MoonMesh";
            config["genesis"]["blockData"]["Type"] = "Genesis";
        }
        else
        {
            config["genesis"]["initAccountAddr"] = "";
            config["genesis"]["genesisTime"] = 0;
            config["genesis"]["initialBalance"]["ASSET_NAME"] = 0;
            config["genesis"]["blockData"]["Name"] = "";
            config["genesis"]["blockData"]["Type"] = "";
        }

        return config;
    }

    std::string GenesisConfigManager::GenerateConfigJson() const
    {
        nlohmann::json jsonConfig;

        // version information
        jsonConfig["version"] = config_.version;

        // net information
        jsonConfig["network"]["name"] = config_.network.name;
        jsonConfig["network"]["type"] = config_.network.type;

        // Creation Information
        jsonConfig["genesis"]["initAccountAddr"] = config_.genesis.initAccountAddr;
        jsonConfig["genesis"]["genesisTime"] = config_.genesis.genesisTime;
        for (const auto &balance : config_.genesis.initialBalance)
        {
            jsonConfig["genesis"]["initialBalance"][balance.first] = balance.second;
        }
        for (const auto &data : config_.genesis.blockData)
        {
            jsonConfig["genesis"]["blockData"][data.first] = data.second;
        }

        return jsonConfig.dump();
    }

    BuildType GenesisConfigManager::GetBuildType()
    {
        return buildType_;
    }

    uint64_t GenesisConfigManager::GetGenesisTime()
    {
        return kGenesisTime_;
    }

    std::string GenesisConfigManager::GetInitAccountAddr()
    {
        return kInitAccountAddr_;
    }

    std::string GenesisConfigManager::GetVersion()
    {
        return kVersion_;
    }

    int GenesisConfigManager::SetBuildType(const std::string &configJson)
    {
        GenesisConfig::GenesisConfigManager &configManager = GenesisConfig::GenesisConfigManager::GetInstance();
        try
        {
            std::ifstream configFile(configJson);
            if (!configFile.is_open())
            {
                ERRORLOG("Failed to open config file: {}", configJson);
                return false;
            }

            nlohmann::json config;
            configFile >> config;
            configFile.close();
            std::string networkType = "";
            if (config.contains("network") && config["network"].contains("type"))
            {
                networkType = config["network"]["type"].get<std::string>();

                if (networkType == "primary" || networkType == "mainnet")
                    buildType_ = BuildType::BUILD_TYPE_PRIMARY;
                else if (networkType == "test" || networkType == "testnet")
                    buildType_ = BuildType::K_BUILD_TYPE_TEST;
                else if (networkType == "dev" || networkType == "devnet")
                    buildType_ = BuildType::BUILD_TYPE_DEV;
            }
            else
            {
                std::cerr << "Warning: Missing network type in config. Using dev." << std::endl;
                return -2;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing config JSON: " << e.what() << std::endl;
            return -3;
        }
        return 0;
    }

int GenesisConfigManager::SetGenesisTime(const std::string &configJson)
{
    GenesisConfig::GenesisConfigManager &configManager = GenesisConfig::GenesisConfigManager::GetInstance();
    std::ifstream configFile(configJson);
    if (!configFile.is_open())
    {
        ERRORLOG("Failed to open config file: {}", configJson);
        return false;
    }

    nlohmann::json config;
    configFile >> config;
    configFile.close();
    uint64_t kGenesisTime = 0;
    try
    {
        if (config.contains("genesis") && config["genesis"].contains("genesisTime"))
        {
            kGenesisTime = config["genesis"]["genesisTime"].get<uint64_t>();
        }
    }catch (const std::exception &e)
    {
        ERRORLOG("Error parsing config JSON:  {}", e.what());
        std::cerr << "Error parsing config JSON: " << e.what() << std::endl;
        return -2;
    }
    kGenesisTime_ = kGenesisTime;
    return 0;
}

int GenesisConfigManager::SetInitAccountAddr(const std::string &configJson)
{
    GenesisConfig::GenesisConfigManager &configManager = GenesisConfig::GenesisConfigManager::GetInstance();
    std::string kInitialAccountAddress = "";
    std::ifstream configFile(configJson);
    if (!configFile.is_open())
    {
        ERRORLOG("Failed to open config file: {}", configJson);
        return false;
    }

    nlohmann::json config;
    configFile >> config;
    configFile.close();
    try
    {
        if (config.contains("genesis") && config["genesis"].contains("initAccountAddr"))
        {
            kInitialAccountAddress = config["genesis"]["initAccountAddr"].get<std::string>();
        }
    } catch (const std::exception &e)
    {
        ERRORLOG("Error parsing config JSON:  {}", e.what());
        std::cerr << "Error parsing config JSON: " << e.what() << std::endl;
        return -2;
    }
    kInitAccountAddr_ = kInitialAccountAddress;
    return 0;
}

int GenesisConfigManager::SetVersion()
{

    if (buildType_ == GenesisConfig::BuildType::BUILD_TYPE_PRIMARY)      kVersion_ = global::kSystem + "_" + global::kCompatibleVersion + "_p";
    else if (buildType_ == GenesisConfig::BuildType::K_BUILD_TYPE_TEST)    kVersion_ = global::kSystem + "_" + global::kCompatibleVersion + "_t";
    else if (buildType_ ==  GenesisConfig::BuildType::BUILD_TYPE_DEV)    kVersion_ = global::kSystem + "_" + global::kCompatibleVersion + "_d";
    else kVersion_ = global::kSystem + "_" + global::kCompatibleVersion + "_u";
    
    return 0;
}



}// namespace ca
