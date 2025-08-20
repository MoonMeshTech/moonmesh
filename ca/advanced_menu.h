/**
 * *****************************************************************************
 * @file        advanced_menu.h
 * @brief       So the implementation of the menu function
 * @date        2023-09-25
 * @copyright   mm
 * *****************************************************************************
 */

#ifndef ADVANCED_MENU_GUARD
#define ADVANCED_MENU_GUARD

#include <string>
#include <cstdint>
#include "db/db_api.h"


/**
 * @brief       Rollback block from Height or Rollback block from Hash 
 */
void RollBack();

/**
 * @brief       Get the stake list
 */
void GetStakeList();

/**
 * @brief    Get the Exchequer
 */
void fetchExchequer();
/**
 * @brief      Get a list of addresses that can claim bonuses
 */
int fetchBonusAddressInfo();

/**
 * @brief       
 */
void SendMessageToUser();

/**
 * @brief       
 */
void displayKBuckets();

/**
 * @brief       
 */
void kickOutNode();

/**
 * @brief       
 */
void TestEcho();

/**
 * @brief       
 */
void printRequestAndAcknowledgment();

#pragma region threeLevelMenu
/**
 * @brief       blockinfoMenu
 */
void menuBlockInfo();

/**
 * @brief       Get the tx block info object
 * 
 * @param       top: block height
 */
void retrieveTransactionBlockInfo(uint64_t& top);



/**
 * @brief       
 */
void get_balance_by_utxo();

/**
 * @brief       
 */
int MockTransactionStruct();

/**
 * @brief       
 */
void TestsHandleDelegating();

/**
 * @brief       
 */
void MultiAccountTransactionEntity();

/**
 * @brief       
 */
void get_all_pledge_addresses();

/**
 * @brief       
 */
void AutoTx();

/**
 * @brief       
 */
void getBlockInfoByTxHash();

/**
 * @brief       
 */
void CreateNodeAutomaticTransferTransaction();

/**
 * @brief       
 */
void createMultiThreadAutoStakeTransaction();

/**
 * @brief       
 */
void AutoDelegation();

/**
 * @brief       
 */
void printAndVerifyNode();


/**
 * @brief       
 */
void TpsCount();

/**
 * @brief       
 */
void Get_DelegatingedNodeBlance();

/**
 * @brief       
 */
void printDatabaseBlock();

/**
 * @brief       
 */
void PrintTxData();

/**
 * @brief       
 */
void MultiTx();

/**
 * @brief       
 */
void getContractAddr();

/**
 * @brief       
 */
void print_benchmark_to_file();

/**
 * @brief       
 */
void GetRewardAmount();

/**
 * @brief  
 */
void testManToOneDelegate();

/**
 * @brief  open log
 */
void OpenLog();

/**
 * @brief  close log
 */
void CloseLog();

void TestSign();

void createMultiThreadAutoTransaction();
void MultiTransaction();
namespace ThreadTest
{
    /**
     * @brief       
     * 
     * @param       from: 
     * @param       to: 
     */
    void testCreateTransactionMessage(const std::string& from,const std::string& to,bool isAmount,const double amount, std::string asset_type);

    /**
     * @brief       
     * 
     * @param       tranNum: 
     * @param       addrs: 
     * @param       sleepTime: 
     */
    void TestCreateTx(uint32_t tranNum, std::vector<std::string> addrs, int sleepTime, std::string asset_type);
    /**
     * @brief       Set the Stop Tx Flag object
     * 
     * @param       flag: 
     */
    void setStopTransmissionFlag(const bool &flag);

    /**
     * @brief       Get the Stop Tx Flag object
     * 
     * @param       flag: 
     */
    void stopTxFlag(bool &flag);

    

}

#pragma endregion
#endif
