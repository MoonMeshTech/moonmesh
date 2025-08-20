//
// Created by root on 2024/4/24.
//

#ifndef EVM_ENV_HEADER
#define EVM_ENV_HEADER

#include <cstdint>
#include <evmc/evmc.h>
#include <transaction.pb.h>
#include <block.pb.h>
#include "evm_host.h"

namespace evmEnvironment
{
    int64_t GetBlockNumber();

    int64_t GetNonce(const std::string &address);

    int64_t GetNextNonce(const std::string &address);

    int createDeploymentMessageRequest(const evmc_address &sender, const evmc_address &recipient, const std::string &gasType, evmc_message &message);

    int MakeCallMessage(const evmc_address &sender, const evmc_address &recipient, const evmc::bytes &input,
                        const uint64_t &contractTransfer, const std::string &gasType, evmc_message &message);
    
    int rpcCallMessage(const evmc_address &sender, const evmc_address &recipient, const evmc::bytes &input,
                                                                                                                                     const uint64_t &contractTransfer, evmc_message &message);

    int make_deploy_host(const std::string &sender, const std::string &recipient, EvmHost &host,
                       int64_t blockTimestamp, int64_t blockPrevRandao, int64_t blockNumber,
                       uint64_t transferAmount);

    int makeCallHostRequest(const std::string &sender, const std::string &recipient, uint64_t transferAmount,
                                  const evmc::bytes &code,
                                  EvmHost &host, int64_t blockTimestamp, int64_t blockPrevRandao, int64_t blockNumber);

    int createTxContext(const std::string &from, evmc_tx_context &txContext, int64_t blockTimestamp,
                      int64_t blockPrevRandao, int64_t blockNumber);

    int64_t GetBlockTimestamp(const CTransaction &transaction);

    int64_t get_block_prev_randao(const CTransaction &transaction);

    int64_t calculateBlockTimestamp(int64_t time);

    int64_t blockPrevRandaoCalculator(const std::string &from, const std::string &gasType, bool isFindUtxo);

    int64_t blockPrevRandaoCalculator(const CTransaction &transaction);

    bool VerifyCoinbase(const CBlock &block, const std::string &coinbase);
}


#endif 
