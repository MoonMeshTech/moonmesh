/**
 * *****************************************************************************
 * @file        checker.h
 * @brief       
 * @date        2023-09-26
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef COMPLIANCE_CHECKER_ENABLED
#define COMPLIANCE_CHECKER_ENABLED

#include <vector>
#include <map>

#include "ca/transaction.h"
#include "ca/block_helper.h"
#include "ca/transaction_entity.h"

#include "proto/block.pb.h"
#include "proto/transaction.pb.h"


namespace Checker 
{
    /**
     * @brief
     *
     * @param       tx:
     * @param       cache:
     * @return      true
     * @return      false
     */
    bool CheckConflict(const CTransaction &tx, const std::vector<TransactionEntity>  &cache);
    /**
     * @brief       
     * 
     * @param       tx: 
     * @param       blocks: 
     * @return      true 
     * @return      false 
     */
    bool CheckConflict(const CTransaction &tx, const std::set<CBlock, compator::blockTimeAscending> &blocks);

    /**
     * @brief       
     * 
     * @param       block: 
     * @param       blocks: 
     * @param       txHashRef: 
     * @return      true 
     * @return      false 
     */
    bool CheckConflict(const CBlock &block, const std::set<CBlock, compator::blockTimeAscending> &blocks, std::string* txHashRef = nullptr);
    
    /**
     * @brief       
     * 
     * @param       block1: 
     * @param       block2: 
     * @param       txHashRef: 
     * @return      true 
     * @return      false 
     */
    bool CheckConflict(const CBlock &block1, const CBlock &block2, std::string* txHashRef = nullptr);

    /**
     * @brief       
     * 
     * @param       tx1: 
     * @param       tx2: 
     * @return      true 
     * @return      false 
     */
    bool CheckConflict(const CTransaction &tx1, const CTransaction &tx2);

    /**
     * @brief       
     * 
     * @param       block: 
     * @param       double_spent_transactions: 
     */
    void CheckConflict(const CBlock &block, std::vector<CTransaction>& double_spent_transactions);
    bool CheckConflict(const CTransaction &tx1, const CTransaction &tx2);
    void CheckConflict(const CBlock &block, std::vector<CTransaction>& double_spent_transactions);
};
#endif