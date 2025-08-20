#include "main.h"

#include <regex>
#include <iostream>

#include "include/logging.h"
#include "common/global.h"
#include "utils/time_util.h"
#include "net/api.h"
#include "ca/ca.h"
#include "ca/global.h"
#include "utils/hex_code.h"
#include "ca/advanced_menu.h"
#include "ca/algorithm.h"
#include "utils/magic_singleton.h"
#include "test.h"
#include "interface.pb.h"
#include "ca/interface.h"
#include "utils/account_manager.h"
#include "ca/block_http_callback.h"
#include "ca/block_stroage.h"
#include "net/httplib.h"
#include "ca/genesis_config.h"
#include "ca/genesis_block_generator.h"

void Menu()
{
	PrintBasicInfo();
	while (true)
	{
		std::cout << std::endl << std::endl;
		std::cout << "1.Transaction" << std::endl;
		std::cout << "2.Staking" << std::endl;
		std::cout << "3.Unstaking" << std::endl;
		std::cout << "4.Delegating" << std::endl;
		std::cout << "5.Undelegating" << std::endl;
		std::cout << "6.Get Bonus"  << std::endl;
        std::cout << "7.Print Account Info" << std::endl;
		std::cout << "8.Deploy contract"  << std::endl;
		std::cout << "9.Call contract"  << std::endl;
		std::cout << "10.Lock" << std::endl;
		std::cout << "11.UnLock" << std::endl;
		std::cout << "12.Proposal" << std::endl;
		std::cout << "13.Revoke proposal" << std::endl;
		std::cout << "14.Vote" << std::endl;
		std::cout << "15.Fund" << std::endl;
		std::cout << "16.Account Manager" << std::endl;
		std::cout << "0.Exit" << std::endl;

		std::string strKey;
		std::cout << "Please input your choice: "<< std::endl;
		std::cin >> strKey;	    
		std::regex pattern("^[0-9]|(1[0-6])|(99)|(100)$");
		if(!std::regex_match(strKey, pattern))
        {
            std::cout << "Invalid input." << std::endl;
            continue;
        }
        int key = std::stoi(strKey);
		switch (key)
		{			
			case 0:
				std::cout << "Exiting, bye!" << std::endl;
				CaCleanup();
				DEBUGLOG("*** Program exits normally *** ");
				quick_exit(0);
				return;		
			case 1:
				HandleTransaction();
				break;
			case 2:
				HandleStake();
				break;
			case 3:
				HandleUnstake();
				break;
			case 4:
				handleDelegating();
                break;
			case 5:
				HandleUndelegating();
                break;
			case 6:
				HandleBonus();
                break;
			case 7:
				PrintBasicInfo();
				ca_algorithm::GetAllMappedAssets();
				break;
			case 8:
				DeployContract();
				break;
			case 9:
				CallContract();
				break;
			case 10:
				HandleLock();
				break;
			case 11:
				HandleUnLock();
				break;
			case 12:
				HandleProposal();
				break;
			case 13:
				revokeProposalRequest();
				break;
			case 14:
				HandleVote();
				break;
			case 15:
				HandleTresury();
				break;
			case 16:
				handleAccountManager();
				break;
			default:
                std::cout << "Invalid input." << std::endl;
                continue;
		}
		sleep(1);
	}
}
	
bool Init()
{
	// Initialize the random number seed
	srand(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
	
	// Initialize configuration

	if(!InitConfig())
	{
		ERRORLOG("Failed to initialize log!");
		std::cout << "Failed to initialize log!" << std::endl;
		return false;
	}


	// Initialize log
	if (!InitLog())
	{
		ERRORLOG("Failed to initialize log!");
		std::cout << "Failed to initialize log!" << std::endl;
		return false;
	}

	// Initialize account
    if(!InitAccount())
	{
		ERRORLOG("Failed to initialize certification!");
		std::cout << "Failed to initialize certification!" << std::endl;
		return false;		
	}
	
	// Initialize database
    if(InitRocksDb()!=0)
	{
		ERRORLOG("Failed to initialize database!");
		std::cout << "Failed to initialize database!" << std::endl;
		return false;		
	}

	return  CaInit() && NetInit();
}

bool InitConfig()
{
	bool flag = false;
	MagicSingleton<Config>::GetInstance(flag);
	return flag;
}

bool InitLog()
{
	Config::Log log = {};
	MagicSingleton<Config>::GetInstance()->GetLog(log);
	if(!MagicSingleton<Log>::GetInstance()->LogInit(log.path, log.console, log.level))
	{
		return false;
	}
	return true;
}

bool InitAccount()
{
	try {
		auto accountManager = MagicSingleton<AccountManager>::GetInstance();
		auto result = accountManager->Initialize();
		
		if (result.status != accountInitResult::Status::SUCCESS) {
			std::cout << "Account initialization issue: " << result.message << std::endl;
			
			if (result.status == accountInitResult::Status::EMPTY_KEYSTORE) {
				std::cout << "No existing accounts found. Creating a new account..." << std::endl;
				result = accountManager->CreateInitialAccount();
				if (result.status != accountInitResult::Status::SUCCESS) {
					std::cout << "Failed to create account: " << result.message << std::endl;
					return false;
				}
				std::cout << "Account created successfully." << std::endl;
				return true;
			} 
			else if (result.status == accountInitResult::Status::kAccountsNotInitialized) {
				std::cout << "Would you like to create a new account? (y/n): ";
				std::string response;
				std::getline(std::cin, response);
				
				if (response == "y" || response == "Y") {
					result = accountManager->CreateInitialAccount();
					if (result.status != accountInitResult::Status::SUCCESS) {
						std::cout << "Failed to create account: " << result.message << std::endl;
						return false;
					}
					std::cout << "Account created successfully." << std::endl;
					return true;
				} else {
					std::cout << "Cannot run program without valid accounts." << std::endl;
					return false;
				}
			}
			else {
				// Other serious errors, simply return failure.
				return false;
			}
		}
		
		return true;
	}
	catch (const std::exception& e) {
		std::cout << "Exception during account initialization: " << e.what() << std::endl;
		return false;
	}
}

int InitRocksDb()
{
    if (!DBInit("./data.db"))
    {
        return -1;
    }

    DBReadWriter dbReadWriter;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReadWriter.getBlockTop(top))
    {
        // Initialize Block 0 using the dynamic genesis block generator
        GenesisConfig::GenesisBlockGenerator& generator = GenesisConfig::GenesisBlockGenerator::GetInstance();
        
        CBlock block;
        if (!generator.GenerateGenesisBlock(block))
        {
            ERRORLOG("Failed to generate genesis block");
            return -2;
        }
        
        if (block.txs_size() == 0)
        {
            return -3;
        }
        
        CTransaction tx = block.txs(0);
        if(tx.utxos_size() == 0)
        {
            return -4;
        }

        // Obtain the initial account address for dynamic configuration
        const GenesisConfig::GenesisConfigManager& configManager = GenesisConfig::GenesisConfigManager::GetInstance();
        const std::string& initAccountAddr = configManager.GetGenesisInfo().initAccountAddr;

        dbReadWriter.setBlockHeightByBlockHash(block.hash(), block.height());
        dbReadWriter.setBlockHashByBlockHeight(block.height(), block.hash(), true);
        dbReadWriter.setBlockByBlockHash(block.hash(), block.SerializeAsString());
        dbReadWriter.setBlockTop(0);
		
		for(auto& utxo : tx.utxos())
		{
			for(int i = 0; i < utxo.vout_size(); ++i)
			{
				if(!isValidAddress(utxo.vout(i).addr()))
				{
					continue;
				}
				dbReadWriter.setUtxoHashesByAddr(utxo.vout(i).addr(),utxo.assettype(),tx.hash());
				if(utxo.vout(i).addr() == initAccountAddr)
				{
					dbReadWriter.setUtxoValueByUtxoHashes(tx.hash(), utxo.vout(i).addr(), utxo.assettype(), std::to_string(utxo.vout(i).value()));
					dbReadWriter.setBalanceByAddr(utxo.vout(i).addr(), utxo.assettype(), utxo.vout(i).value());
				}
				else
				{
					dbReadWriter.setUtxoValueByUtxoHashes(tx.hash(), utxo.vout(i).addr(), utxo.assettype(), std::to_string(utxo.vout(i).value()));
					dbReadWriter.setBalanceByAddr(utxo.vout(i).addr(),  utxo.assettype(), utxo.vout(i).value());
				}
			}
			dbReadWriter.setBalanceByAddr(utxo.owner().at(0), utxo.assettype(), utxo.vout(0).value());
		}

        dbReadWriter.setTransactionByHash(tx.hash(), tx.SerializeAsString());
        dbReadWriter.setBlockHashByTransactionHash(tx.hash(), block.hash());
    }
	dbReadWriter.setInitVer(global::GetVersion());
	if (DBStatus::DB_SUCCESS != dbReadWriter.transactionCommit())
    {
        ERRORLOG("(rocksdb init) transactionCommit failed !");
        return -8;
    }

    return 0;
}


bool Check()
{
	DBReader dbReader;
    uint64_t top = 0;
	std::string hash;
	std::string blockRaw;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockHashByBlockHeight(top,hash))
    {
		return false;
	}

	if (DBStatus::DB_SUCCESS != dbReader.getBlockByBlockHash(hash,blockRaw))
    {
		return false;
	}

	CBlock dbBlock;
    dbBlock.ParseFromString(blockRaw);

	// Obtain the genesis time of the dynamic configuration
	const GenesisConfig::GenesisConfigManager& configManager = GenesisConfig::GenesisConfigManager::GetInstance();
	if (!configManager.IsConfigLoaded())
	{
		ERRORLOG("Network configuration not loaded during Check()");
		return false;
	}
	
	const uint64_t genesisTime = configManager.GetGenesisInfo().genesisTime;
	
	if(dbBlock.time() != genesisTime)
	{
		std::cout << "The current version data is inconsistent with the new version !" << std::endl;
		return false;
	}

	// Verify using the dynamic genesis block generator
	GenesisConfig::GenesisBlockGenerator &generator = GenesisConfig::GenesisBlockGenerator::GetInstance();
	CBlock expectedBlock;
	if (!generator.GenerateGenesisBlock(expectedBlock))
	{
		ERRORLOG("Failed to generate expected genesis block for verification");
		return false;
	}

	if(expectedBlock.hash() != hash)
	{
		std::cout << "The current version data is inconsistent with the new version !" << std::endl;
		return false;
	}

	return true;
}