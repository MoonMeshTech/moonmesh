/**
 * *****************************************************************************
 * @file        test.h
 * @brief       
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */

#ifndef CA_TEST_HEADER_GUARD
#define CA_TEST_HEADER_GUARD
#include <string>

#include <nlohmann/json.hpp>
#include "proto/block.pb.h"
#include "proto/transaction.pb.h"

/**
 * @brief       
 * 
 * @param       start: 
 * @param       end: 
 * @param       isConsoleOutput: 
 * @param       stream: 
 * @return      int 
 */
int printRocksdbInfo(uint64_t start, uint64_t end, bool isConsoleOutput, std::ostream & stream);

/**
 * @brief       
 * 
 * @param       block: 
 * @param       isConsoleOutput: 
 * @param       stream: 
 * @return      int 
 */
int PrintBlock(const CBlock & block, bool isConsoleOutput, std::ostream & stream);

/**
 * @brief       
 * 
 * @param       tx: 
 * @param       isConsoleOutput: 
 * @param       stream: 
 * @return      int 
 */
int PrintTx(const CTransaction & tx, bool isConsoleOutput, std::ostream & stream);

/**
 * @brief       
 * 
 * @param       num: 
 * @param       preHashEnabled: 
 * @return      std::string 
 */
std::string PrintBlocks(int num = 0, bool preHashEnabled = false);

/**
 * @brief       
 * 
 * @param       num: 
 * @param       preHashEnabled: 
 * @return      std::string 
 */
std::string print_blocks_hash(int num = 0, bool preHashEnabled = false);

/**
 * @brief       
 * 
 * @param       startNum: 
 * @param       num: 
 * @param       preHashEnabled: 
 * @return      std::string 
 */
std::string printRangeBlocks(int startNum = 0,int num = 0, bool preHashEnabled = false);

/**
 * @brief       
 * 
 * @param       strHeader: 
 * @param       blocks: 
 */

std::string transactionInvestment(const CTransaction& tx);
void BlockInvert(const std::string& strHeader, nlohmann::json& blocks);
nlohmann::json fixedTxField(const nlohmann::json& inTx);

/**
 * @brief       
 * 
 * @param       where: 
 * @return      std::string 
 */

int print_contract_block(const CBlock & block, bool isConsoleOutput, std::ostream & stream);
std::string DisplayContractSections(int num, bool preHashEnabled);
std::string print_range_contract_blocks(int startNum,int num, bool preHashEnabled);
std::string PrintCache(int where);

/**
 @brief * Fixes the data field in a given JSON object representing block data.
 * 
 * This function modifies the provided JSON object to correct or enhance the data field.
 * It is intended to ensure the integrity and correctness of the block data as represented
 * in JSON format.
 * 
 * @param data A reference to a JSON object containing block data to be fixed.
 * @return int Returns 0 on success, non-zero on failure.
 */
int FixBlockDataField(nlohmann::json &data);

int FixTxDataField(nlohmann::json &data, uint32_t type);
#endif
