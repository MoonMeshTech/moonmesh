#include "string.h"
#include "unistd.h"

#include <iostream>
#include <fstream>
#include <ctime>
#include <sys/wait.h>
#include <cstdint>
#include <algorithm>
#include <cctype>

#include "main/main.h"
#include "utils/time_util.h"
#include "utils/console.h"
#include "common/config.h"
#include "common/global.h"
#include "ca/ca.h"
#include "ca/global.h"
#include "ca/transaction.h"
#include "utils/single.hpp"
#include "utils/bench_mark.h"
#include "ca/test.h"
#include "utils/file_manager.h"
#include "ca/genesis_config.h"
#include "ca/genesis_block_generator.h"


namespace IpFactor
{
    extern bool kOptionUsed;
    extern bool KM_LOCALHOST_USED;
    extern bool PUBLIC_IP_USED_FLAG;
    extern std::string kPublicIpAddress;
}

bool isDaemonMode = false;

void startDaemon(const char *programName, const char *programPath)
{
	if (daemon(1, 0) == -1)
	{
		std::cerr << "Failed to daemonize process." << std::endl;
		exit(-1);
	}

	signal(SIGCHLD, SIG_IGN);
	while (true)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			chdir(programPath);
			execlp(programName, programName, "-m", NULL);
			exit(0);
		}
		else if (pid < 0)
		{
			perror("fork failed");
			exit(-1);
		}

		int status;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
		{
			std::cerr << "Child process crashed, restarting..." << std::endl;
		}
		sleep(10);
	}
}

int main(int argc, char *argv[])
{
	if (-1 == InitPidFile())
	{
		exit(-1);
	}

	/* Ctrl + C */
	if (signal(SIGINT, sigHandler) == SIG_ERR)
	{
		exit(-1);
	}

	/* kill pid / killall name */
	if (signal(SIGTERM, sigHandler) == SIG_ERR)
	{
		exit(-1);
	}

	struct sigaction sa;
	sa.sa_handler = signalHandlerOther;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGABRT, &sa, NULL);
	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGFPE, &sa, NULL);
	sigaction(SIGILL, &sa, NULL);

	// Modify process time UTC
	setenv("TZ", "Universal", 1);
	tzset();


	if (argc > 6)
	{
		ERRORLOG("Too many parameters!");
		std::cout << "Too many parameters!" << std::endl;
		return 2;
	}

	bool showMenu = false;
	bool needGenKey = false;
	std::string networkConfigFile = "";
	std::string networkType = "";
	bool generateGenesisOnly = false;
	bool validateConfigOnly = false;
	bool generateTemplate = false;
	std::string templateType = "";

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--menu") == 0)
		{
			showMenu = true;
			IpFactor::kOptionUsed = true;
			if (i + 1 < argc)
			{
				i++;
				if (strcmp(argv[i], "localhost") == 0)
				{
					IpFactor::KM_LOCALHOST_USED = true;
				}
				else if (strcmp(argv[i], "publicip") == 0)
				{
					IpFactor::PUBLIC_IP_USED_FLAG = true;
					if (i + 1 < argc)
					{
						i++;
						IpFactor::kPublicIpAddress = argv[i];
					}
				}
				else
				{
					ERRORLOG("Parameter parsing error!");
					std::cout << "Parameter parsing error!" << std::endl;
					return 2;
				}
			}
		}
		else if (strcmp(argv[i], "--daemon") == 0)
		{
			isDaemonMode = true;
		}
		else if (strcmp(argv[i], "--network") == 0)
		{
			if (i + 1 < argc)
			{
				i++;
				networkConfigFile = argv[i];
			}
			else
			{
				std::cout << "Error: --network requires a config file path" << std::endl;
				return 3;
			}
		}
		else if (strcmp(argv[i], "--network-type") == 0)
		{
			if (i + 1 < argc)
			{
				i++;
				networkType = argv[i];
				
				std::string lowerNetworkType = networkType;
				std::transform(lowerNetworkType.begin(), lowerNetworkType.end(), lowerNetworkType.begin(), ::tolower);
				
				if (lowerNetworkType != "primary" && lowerNetworkType != "test" && lowerNetworkType != "dev")
				{
					std::cout << "Error: Invalid network type. Must be primary, test, or dev" << std::endl;
					return 4;
				}
				
				networkType = lowerNetworkType;
			}
			else
			{
				std::cout << "Error: --network-type requires a type (primary|test|dev)" << std::endl;
				return 3;
			}
		}
		else if (strcmp(argv[i], "--genesis-only") == 0)
		{
			generateGenesisOnly = true;
		}
		else if (strcmp(argv[i], "--validate-config") == 0)
		{
			validateConfigOnly = true;
		}
		else if (strcmp(argv[i], "--generate-template") == 0)
		{
			if (i + 1 < argc)
			{
				i++;
				templateType = argv[i];
				
				std::string lowerTemplateType = templateType;
				std::transform(lowerTemplateType.begin(), lowerTemplateType.end(), lowerTemplateType.begin(), ::tolower);
				
				if (lowerTemplateType != "test" && lowerTemplateType != "dev")
				{
					std::cout << "Error: Template type must be 'test' or 'dev'" << std::endl;
					return 4;
				}
				
				templateType = lowerTemplateType;
				generateTemplate = true;
			}
			else
			{
				std::cout << "Error: --generate-template requires a type (test|dev)" << std::endl;
				return 3;
			}
		}
		else if (strcmp(argv[i], "--config") == 0)
		{
			if (strcmp(argv[i + 1], "-ip") == 0)
			{
				std::string newIP = argv[i + 2];
				if (!isValidIPv4(newIP))
				{
					std::cerr << "Invalid IP format. Use IPv4 (e.g., 192.168.1.1)" << std::endl;
					return 3;
				}
				update_config_ip(newIP);
				return 0;
			}
		}
		else if (strcmp(argv[i], "--help") == 0)
		{
			ERRORLOG("The parameter is Help!");
			std::cout << "\n============================================" << std::endl;
			std::cout << "  " << argv[0] << " - Version: " << global::kCompatibleVersion << std::endl;
			std::cout << "============================================" << std::endl;
			
			std::cout << "\nUSAGE:" << std::endl;
			std::cout << "  " << argv[0] << " [COMMAND] [OPTIONS]" << std::endl;
			
			std::cout << "\nBASIC COMMANDS:" << std::endl;
			std::cout << "  --menu [localhost|publicip <ip>]  - Start interactive menu (default mode)" << std::endl;
			std::cout << "  --genkey                          - Generate new account keys (standalone)" << std::endl;
			std::cout << "  --daemon                          - Run in daemon mode" << std::endl;
			std::cout << "  --config -ip <ipaddress>          - Configure IP address" << std::endl;
			std::cout << "  --test                            - Run test suite" << std::endl;
			std::cout << "  --benchmark                       - Run benchmark tests" << std::endl;
			std::cout << "  --help                            - Show this help information" << std::endl;
			
			std::cout << "\nNETWORK CONFIGURATION:" << std::endl;
			std::cout << "  --network-type primary          - Use primary/mainnet (hardcoded genesis)" << std::endl;
			std::cout << "  --network <file>                - Use custom JSON config file" << std::endl;
			std::cout << "  --generate-template <type>      - Generate JSON template (test|dev)" << std::endl;
			
			std::cout << "\nGENESIS BLOCK TOOLS:" << std::endl;
			std::cout << "  --genesis-only                  - Generate genesis block and exit" << std::endl;
			std::cout << "  --validate-config               - Validate configuration and exit" << std::endl;
			
			std::cout << "\nNETWORK TYPES:" << std::endl;
			std::cout << "  primary     - Primary/Mainnet (uses hardcoded genesis parameters)" << std::endl;
			std::cout << "  test        - Test network (requires custom JSON configuration)" << std::endl;
			std::cout << "  dev         - Development network (requires custom JSON configuration)" << std::endl;
			
			std::cout << "\nWORKFLOW FOR CUSTOM NETWORKS:" << std::endl;
			std::cout << "  1. Generate template:           " << argv[0] << " --generate-template dev" << std::endl;
			std::cout << "  2. Edit dev_genesis.json with your parameters" << std::endl;
			std::cout << "  3. Validate configuration:      " << argv[0] << " --network dev_genesis.json --validate-config" << std::endl;
			std::cout << "  4. Generate genesis block:      " << argv[0] << " --network dev_genesis.json --genesis-only" << std::endl;
			std::cout << "  5. Start network:               " << argv[0] << " --network dev_genesis.json menu" << std::endl;
			
			std::cout << "\nCOMMON EXAMPLES:" << std::endl;
			std::cout << "  " << argv[0] << " --genkey                                  # Generate account keys" << std::endl;
			std::cout << "  " << argv[0] << " --network-type primary menu             # Start mainnet" << std::endl;
			std::cout << "  " << argv[0] << " --generate-template dev                 # Create dev template" << std::endl;
			std::cout << "  " << argv[0] << " --network my_config.json menu           # Start with custom config" << std::endl;
			
			std::cout << "\nCONFIGURATION NOTES:" << std::endl;
			std::cout << "  • Primary network: Genesis parameters are hardcoded for consistency" << std::endl;
			std::cout << "  • Test/Dev networks: Require custom JSON with your genesis parameters" << std::endl;
			std::cout << "  • Use --generate-template to create a starting JSON configuration" << std::endl;
			std::cout << "  • Genesis addresses must be 40 hex characters (with/without 0x)" << std::endl;
			std::cout << "  • Timestamps are in microseconds since Unix epoch" << std::endl;
			std::cout << "  • Asset names: letters, numbers, underscores only" << std::endl;
			std::cout << "\n============================================" << std::endl;
			return 4;
		}
		else if (strcmp(argv[i], "--test") == 0)
		{
			showMenu = true;
			MagicSingleton<Benchmark>::GetInstance()->OpenBenchmark();
		}
		else if (strcmp(argv[i], "--benchmark") == 0)
		{
			showMenu = true;
			MagicSingleton<Benchmark>::GetInstance()->OpenBenchmarkAlt();
		}
		else if (strcmp(argv[i], "--genkey") == 0)
		{
			// Mark that key generation is needed
			needGenKey = true;
		}
		else
		{
			ERRORLOG("Parameter parsing error!");
			std::cout << "Parameter parsing error!" << std::endl;
			return 5;
		}
	}


	//Handle genkey command before network config loading
	if (needGenKey)
	{
		srand(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
		
		if(!InitConfig())
		{
			ERRORLOG("Failed to initialize config for genkey!");
			std::cout << "Failed to initialize config for genkey!" << std::endl;
			return -1;
		}
		
		if (!InitLog())
		{
			std::cout << "Warning: Failed to initialize log for genkey!" << std::endl;
		}
		
		// Ensure that the keystore directory exists (genkey needs to save the key file)
		if (!GetGlobalFileManager().EnsureDirectoryExists(path_constants::KEYSTORE_PATH)) {
			std::cout << "Failed to create keystore directory for genkey!" << std::endl;
			return -1;
		}
		
		GenKey();
		return 0;
	}

	//Handle template generation before network config loading
	if (generateTemplate)
	{
		std::string filename = templateType + "_genesis.json";
		
		// Create sample configuration
		nlohmann::json templateConfig;
		templateConfig["version"] = "1.0.0";
		templateConfig["network"]["name"] = templateType + "net";
		templateConfig["network"]["type"] = templateType;
		
		if (templateType == "test") {
			templateConfig["genesis"]["initAccountAddr"] = "";
			templateConfig["genesis"]["genesisTime"] = 0;
			templateConfig["genesis"]["initialBalance"]["Vote"] = 0;
			templateConfig["genesis"]["initialBalance"]["MM"] = 0;
			templateConfig["genesis"]["blockData"]["Name"] = "TestChain";
			templateConfig["genesis"]["blockData"]["Type"] = "Genesis";
		} else { // dev
			templateConfig["genesis"]["initAccountAddr"] = "";
			templateConfig["genesis"]["genesisTime"] = 0;
			templateConfig["genesis"]["initialBalance"]["Vote"] = 0;
			templateConfig["genesis"]["initialBalance"]["MM"] = 0;
			templateConfig["genesis"]["blockData"]["Name"] = "DevChain";
			templateConfig["genesis"]["blockData"]["Type"] = "Genesis";
		}
		
		try {
			std::ofstream file(filename);
			if (!file.is_open()) {
				std::cout << "Failed to create template file: " << filename << std::endl;
				return 2;
			}
			
			file << templateConfig.dump(4); 
			file.close();
			
			std::cout << "Template configuration created: " << filename << std::endl;
			std::cout << "\nConfiguration Details:" << std::endl;
			std::cout << "  Network Type: " << templateType << std::endl;
			std::cout << "  Genesis Address: " << templateConfig["genesis"]["initAccountAddr"] << std::endl;
			std::cout << "  Genesis Time: " << templateConfig["genesis"]["genesisTime"] << " (microseconds)" << std::endl;
			
			std::cout << "\nNext Steps:" << std::endl;
			std::cout << "1. Edit " << filename << " with your custom parameters:" << std::endl;
			std::cout << "   - Change initAccountAddr to your genesis account address" << std::endl;
			std::cout << "   - Set genesisTime to your desired timestamp (microseconds)" << std::endl;
			std::cout << "   - Customize initialBalance with your assets and amounts" << std::endl;
			std::cout << "   - Update blockData fields as needed" << std::endl;
			std::cout << "\n2. Use the configuration:" << std::endl;
			std::cout << "   " << argv[0] << " --network " << filename << " --validate-config" << std::endl;
			std::cout << "   " << argv[0] << " --network " << filename << " --genesis-only" << std::endl;
			std::cout << "   " << argv[0] << " --network " << filename << " menu" << std::endl;
			
			std::cout << "\nValidation Notes:" << std::endl;
			std::cout << "  - Address: 40 hex characters with EIP-55 checksum (0x prefix optional)" << std::endl;
			std::cout << "  - Timestamp: microseconds since Unix epoch (~" << (std::time(nullptr) * 1000000ULL) << ")" << std::endl;
			std::cout << "  - Asset names: letters, numbers, underscores only (Vote, MM are standard)" << std::endl;
			std::cout << "  - Balances: non-negative integers (< 10^18)" << std::endl;
			
		} catch (const std::exception& e) {
			std::cout << "Failed to write template file: " << e.what() << std::endl;
			return 2;
		}
		
		return 0;
	}

	// Handle network configuration loading
	GenesisConfig::GenesisConfigManager &configManager = GenesisConfig::GenesisConfigManager::GetInstance();

	if (!networkConfigFile.empty() && !networkType.empty())
	{
		std::cout << "Error: Cannot specify both --network and --network-type" << std::endl;
		return 3;
	}
	
	// Load network configuration
	bool configLoaded = false;
	if (!networkConfigFile.empty())
	{
		std::cout << "Loading network config from file: " << networkConfigFile << std::endl;
		configLoaded = configManager.LoadFromFile(networkConfigFile);
		if(configManager.SetBuildType(networkConfigFile) != 0)
		{
			ERRORLOG("SetBuildType Error!");
			return 2;
		}
		if(configManager.SetGenesisTime(networkConfigFile)!=0)
		{
			ERRORLOG("SetGenesisTime Error!");
			return 2;
		}
		if(configManager.SetInitAccountAddr(networkConfigFile)!=0)
		{
			ERRORLOG("SetInitAccountAddr Error!");
			return 2;
		}
		if (configManager.SetVersion() != 0)
		{
			ERRORLOG("SetVersion Error!");
			return 2;
		}
		GenesisConfig::GenesisConfigManager &configManager2 = GenesisConfig::GenesisConfigManager::GetInstance();
		if (!configLoaded)
		{
			std::cout << "\n❌ Failed to load configuration from file: " << networkConfigFile << std::endl;
			std::cout << "\nPossible issues:" << std::endl;
			std::cout << "  1. File does not exist or cannot be read" << std::endl;
			std::cout << "  2. Invalid JSON syntax (check commas, quotes, brackets)" << std::endl;
			std::cout << "  3. Missing required fields or invalid field values" << std::endl;
			std::cout << "\nFor detailed validation, try:" << std::endl;
			std::cout << "  " << argv[0] << " --generate-template dev  # Generate a valid template" << std::endl;
			std::cout << "  jsonlint " << networkConfigFile << "           # Check JSON syntax" << std::endl;
			return 2;
		}
	}
	else if (!networkType.empty())
	{
		std::cout << "Loading builtin network config for: " << networkType << std::endl;
		configLoaded = configManager.LoadBuiltinConfig(networkType);
		
		if (!configLoaded)
		{
			std::cout << "\n❌ Failed to load builtin configuration for: " << networkType << std::endl;
			if (networkType == "test" || networkType == "dev")
			{
				std::cout << "\nTest/Dev networks require custom configuration files." << std::endl;
				std::cout << "Generate a template first:" << std::endl;
				std::cout << "  " << argv[0] << " --generate-template " << networkType << std::endl;
			}
			return 2;
		}
	}
	else
	{
		// use dev config
		std::cout << "Using default dev network config" << std::endl;
		configLoaded = configManager.LoadBuiltinConfig("dev");
		
		if (!configLoaded)
		{
			std::cout << "\n❌ Failed to load default dev configuration!" << std::endl;
			std::cout << "This should not happen. Please generate a custom config:" << std::endl;
			std::cout << "  " << argv[0] << " --generate-template dev" << std::endl;
			return 2;
		}
	}
	
	std::cout << "Network configuration loaded successfully" << std::endl;
	std::cout << "Network: " << configManager.GetNetworkInfo().name 
			  << " (" << configManager.GetNetworkInfo().type << ")" << std::endl;
	
	GenesisConfig::GenesisBlockGenerator& generator = GenesisConfig::GenesisBlockGenerator::GetInstance();
	if (!generator.ValidateConfigGenesisHash()) {
		std::cout << "Genesis block validation failed!" << std::endl;
		return 2;
	}
	
	if (validateConfigOnly)
	{
		std::cout << "=== Configuration Validation Report ===" << std::endl;
		
		std::cout << "\nNetwork Configuration:" << std::endl;
		std::cout << "  Network Type: " << configManager.GetNetworkInfo().type << std::endl;
		std::cout << "  Network Name: " << configManager.GetNetworkInfo().name << std::endl;
		std::cout << "  Version: " << configManager.GetVersion() << std::endl;
		
		const auto& genesisInfo = configManager.GetGenesisInfo();
		std::cout << "\nGenesis Configuration:" << std::endl;
		std::cout << "  Genesis Account: " << genesisInfo.initAccountAddr << std::endl;
		std::cout << "  Genesis Time: " << genesisInfo.genesisTime << " (microseconds)" << std::endl;
		
		// Convert the timestamp to a readable format
		time_t timeSeconds = genesisInfo.genesisTime / 1000000;
		struct tm* timeInfo = gmtime(&timeSeconds);
		char timeBuffer[100];
		strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S UTC", timeInfo);
		std::cout << "  Genesis Time (readable): " << timeBuffer << std::endl;
		
		// Display the initial balance
		std::cout << "\nInitial Balances:" << std::endl;
		if (genesisInfo.initialBalance.empty()) {
			std::cout << "  (None specified)" << std::endl;
		} else {
			for (const auto& balance : genesisInfo.initialBalance) {
				std::cout << "  " << balance.first << ": " << balance.second << std::endl;
			}
		}
		
		// Display block data 
		std::cout << "\nBlock Data:" << std::endl;
		if (genesisInfo.blockData.empty()) {
			std::cout << "  (None specified)" << std::endl;
		} else {
			for (const auto& data : genesisInfo.blockData) {
				std::cout << "  " << data.first << ": " << data.second << std::endl;
			}
		}
		
		std::cout << "\n=== Validation Results ===" << std::endl;
		bool allValid = true;
		
		if (!configManager.ValidateConfig()) {
			std::cout << "✗ Configuration validation: FAILED" << std::endl;
			allValid = false;
		} else {
			std::cout << "✓ Configuration validation: PASSED" << std::endl;
		}
		
		// Verify the generation of the genesis block
		GenesisConfig::GenesisBlockGenerator& generator = GenesisConfig::GenesisBlockGenerator::GetInstance();
		if (!generator.ValidateConfigGenesisHash()) {
			std::cout << "✗ Genesis block hash validation: FAILED" << std::endl;
			allValid = false;
		} else {
			std::cout << "✓ Genesis block hash validation: PASSED" << std::endl;
		}
		
		// Attempt to generate the genesis block for integrity verification
		std::string genesisBlockRaw = generator.GenerateGenesisBlockRaw();
		if (genesisBlockRaw.empty()) {
			std::cout << "✗ Genesis block generation: FAILED" << std::endl;
			allValid = false;
		} else {
			std::cout << "✓ Genesis block generation: PASSED" << std::endl;
			std::string genesisHash = generator.GetGenesisBlockHash();
			std::cout << "  Generated genesis hash: " << genesisHash << std::endl;
			
			if (configManager.GetNetworkInfo().type == "primary") {
				if (genesisHash == global::ca::kGenesisBlockHash) {
					std::cout << "✓ Primary network hash match: PASSED" << std::endl;
				} else {
					std::cout << "✗ Primary network hash match: FAILED" << std::endl;
					std::cout << "  Expected: " << global::ca::kGenesisBlockHash << std::endl;
					std::cout << "  Generated: " << genesisHash << std::endl;
					allValid = false;
				}
			}
		}
		
		std::cout << "\n========================================" << std::endl;
		if (allValid) {
			std::cout << "✓ OVERALL VALIDATION: PASSED" << std::endl;
			std::cout << "Configuration is valid and ready to use." << std::endl;
			return 0;
		} else {
			std::cout << "✗ OVERALL VALIDATION: FAILED" << std::endl;
			std::cout << "Please fix the configuration errors above." << std::endl;
			return 1;
		}
	}
	
	if (generateGenesisOnly)
	{
		std::cout << "Generating genesis block for network: " << configManager.GetNetworkInfo().type << std::endl;
		GenesisConfig::GenesisBlockGenerator &generator = GenesisConfig::GenesisBlockGenerator::GetInstance();

		// Generate the genesis block
		std::string genesisBlockRaw = generator.GenerateGenesisBlockRaw();
		if (genesisBlockRaw.empty())
		{
			std::cout << "Failed to generate genesis block!" << std::endl;
			return 2;
		}
		
		std::string genesisHash = generator.GetGenesisBlockHash();
		
		std::string configJson = configManager.GenerateConfigJson();
		
		std::string networkType = configManager.GetNetworkInfo().type;
		std::string filename = networkType + "_genesis.json";
		
		try {
			std::ofstream file(filename);
			if (!file.is_open()) {
				std::cout << "Failed to create file: " << filename << std::endl;
				return 2;
			}
			
			nlohmann::json formattedConfig = nlohmann::json::parse(configJson);
			file << formattedConfig.dump(4); // 4 spaces indentation
			file.close();
			
			std::cout << "Genesis configuration saved to: " << filename << std::endl;
			std::cout << "\nGenesis Block Info:" << std::endl;
			std::cout << "  Hash: " << genesisHash << std::endl;
			std::cout << "  Time: " << configManager.GetGenesisInfo().genesisTime << std::endl;
			std::cout << "  Account: " << configManager.GetGenesisInfo().initAccountAddr << std::endl;
			
			// Display the initial balance
			std::cout << "  Initial Balances:" << std::endl;
			for (const auto& balance : configManager.GetGenesisInfo().initialBalance) {
				std::cout << "    " << balance.first << ": " << balance.second << std::endl;
			}
			
			std::cout << "\nValidation Info:" << std::endl;
			std::string currentNetworkType = configManager.GetNetworkInfo().type;
			
			if (currentNetworkType == "primary") {
				std::cout << "  Expected Hash (hardcoded): " << global::ca::kGenesisBlockHash << std::endl;
				std::cout << "  Generated Hash (from config): " << genesisHash << std::endl;
				if (genesisHash == global::ca::kGenesisBlockHash) {
					std::cout << "  ✓ Hash validation: PASSED" << std::endl;
				} else {
					std::cout << "  ✗ Hash validation: FAILED" << std::endl;
				}
			} else {
				std::cout << "  Network Type: " << currentNetworkType << " (Flexible Configuration)" << std::endl;
				std::cout << "  Generated Hash: " << genesisHash << std::endl;
				std::cout << "  ✓ Configuration: VALID (Hash validation skipped for non-primary networks)" << std::endl;
			}
			
			std::cout << "\nTo use this configuration:" << std::endl;
			std::cout << "  " << argv[0] << " --network " << filename << std::endl;
			
			if (currentNetworkType == "primary") {
				std::cout << "\nNote: Primary network will validate that your config generates" << std::endl;
				std::cout << "      the expected genesis hash: " << global::ca::kGenesisBlockHash << std::endl;
			} else {
				std::cout << "\nNote: " << currentNetworkType << " network allows flexible genesis parameters." << std::endl;
				std::cout << "      You can customize time, address, balances, and other parameters." << std::endl;
			}
			
		} catch (const std::exception& e) {
			std::cout << "Failed to write configuration file: " << e.what() << std::endl;
			return 2;
		}
		
		return 0;
	}

	// Get the current number of CPU cores and physical memory size
	global::cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
	uint64_t memory = (uint64_t)sysconf(_SC_PAGESIZE) * (uint64_t)sysconf(_SC_PHYS_PAGES) / 1024 / 1024;
	INFOLOG("Number of current CPU cores:{}", global::cpu_count);
	INFOLOG("Number of current memory size:{}", memory);
	if (global::GetBuildType() == GenesisConfig::BuildType::BUILD_TYPE_DEV)
	{
		if (global::cpu_count < 1 || memory < 0.5 * 1024)
		{
			std::cout << "Current machine configuration:\n"
					  << "The number of processors currently online (available): " << global::cpu_count << "\n"
					  << "The memory size: " << memory << " MB" << std::endl;
			std::cout << RED << "MM basic configuration:\n"
					  << "The number of processors currently online (available): 1\n"
					  << "The memory size: 512 MB" << RESET << std::endl;
			return 1;
		}
	}
	else if (global::GetBuildType() == GenesisConfig::BuildType::K_BUILD_TYPE_TEST)
	{
		if (global::cpu_count < 4 || memory < 6656)
		{
			std::cout << "Current machine configuration:\n"
					  << "The number of processors currently online (available): " << global::cpu_count << "\n"
					  << "The memory size: " << memory << " MB" << std::endl;
			std::cout << RED << "MM basic configuration:\n"
					  << "The number of processors currently online (available): 4\n"
					  << "The memory size: 8192 MB" << RESET << std::endl;
			return 1;
		}
	}
	else
	{
		if (global::cpu_count < 8 || memory < 12800)
		{
			std::cout << "Current machine configuration:\n"
					  << "The number of processors currently available: " << global::cpu_count << "\n"
					  << "The memory size: " << memory << " MB" << std::endl;
			std::cout << RED << "MM basic configuration:\n"
					  << "The number of processors currently available: 8\n"
					  << "The memory size: 16,384 MB" << RESET << std::endl;
			return 1;
		}
	}

	if (isDaemonMode) {
        const char* programName = argv[0];
        char resolved_path[1024];  
        getcwd(resolved_path, sizeof(resolved_path));
        const char* programPath = resolved_path;
        startDaemon(programName, programPath);
    }



	bool initFail = false;
	if (!Init())
	{
		initFail = true;
	}

	if (!Check())
	{
		initFail = true;
	}

	if (initFail)
	{
		CaCleanup();
		exit(0);
	}

	DEBUGLOG("*** Program startup *** ");

	if (showMenu)
	{
		Menu();
	}

	while (true)
	{
		sleep(9999);
	}

	return 0;
}