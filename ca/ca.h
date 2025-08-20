/**
 * *****************************************************************************
 * @file        ca.h
 * @brief       
 * @date        2023-09-26
 * @copyright   mm
 * *****************************************************************************
 */

#ifndef CA_H
#define _CA_H

#include <iostream>
#include <thread>
#include <shared_mutex>

#include "proto/transaction.pb.h"

extern bool bStopTx;
extern bool isCreateTx;




void RegisterCallback();
void TestCreateTx(const std::vector<std::string> & addrs, const int & sleepTime);

/**
 * @brief       CA initialization
 * 
 * @return      true success
 * @return      false failure
 */
bool CaInit();

/**
 * @brief       CA cleanup function
 */
void CaCleanup();

/**
 * @brief       Related implementation functions used in the main menu
 */
void PrintBasicInfo();

/**
 * @brief       
 */
void HandleTransaction();

/**
 * @brief       
 */
void HandleStake();

/**
 * @brief       
 */
void HandleUnstake();

/**
 * @brief       
 */
void handleDelegating();

/**
 * @brief       
 */
void HandleUndelegating();

/**
 * @brief       
 */
void HandleBonus();

/**
 * @brief       
 */
void HandleChangeAccountPassword();

/**
 * @brief       
 */
void handleAccountManager();

/**
 * @brief       
 */
void handle_set_default_account();

/**
 * @brief       
 */
void DeployContract();

/**
 * @brief       
 */
void CallContract();

/**
 * @brief Deletes the private key from the system.
 */
void DeletePrivateKey();


void GenKey();
/**
 * @brief Imports a private key into the system.
 */
void ImportPrivateKey();

/**
 * @brief       
 */
void handleExportPrivateKey();

/**
 * @brief       NTPcheckout
 * 
 * @return      int 
 */
int CheckNtpTime();

/**
 * @brief       Get the Chain Height object
 * 
 * @param       chainHeight: 
 * @return      int 
 */
int GetChainHeight(unsigned int & chainHeight);


/**
 * @brief       
 * 
 * @param       arg: 
 * @param       ack: 
 * @return      std::string 
 */
std::string rpcCallRequest(void * arg,void *ack);

/**
 * @brief       
 * 
 * @param       arg: 
 * @param       ack: 
 * @return      std::string 
 */
std::string deployContractRequest(void * arg,void *ack);

/**
 * @brief       
 * 
 * @param       tx: 
 * @param       addr: 
 * @return      int 
 */
int SigTx(CTransaction &tx,const std::string & addr);
/**
 * @brief   Deploy multiple contracts with one click   
 * 
 */
void createAutomaticDeployContract();
/**
 * @brief   Processing contract transactions   
 * 
 */
void handleMultiDeployContract(const std::string &strFromAddr);
/**
 * @brief   The contract data is exported to json
 * 
 */
void printJson();

std::string remove0xPrefix(std::string str);
std::string addHexPrefix(std::string hexStr);

void HandleLock();            
void HandleUnLock();          
void HandleProposal();        
void revokeProposalRequest();            
void HandleVote();    
void HandleTresury();

int transactionDiscoveryHeight(uint64_t &height);

#endif
