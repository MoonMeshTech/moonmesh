//
// Created by root on 2024/4/30.
//

#ifndef EVM_MANAGER_HEADER
#define EVM_MANAGER_HEADER


#include <optional>
#include <db_api.h>
#include "evm_host.h"

namespace Evmone
{
    const std::string contractVersionId = "version";
    const std::string contractVmKeyName = "virtualMachine";
    const std::string contractSenderKeyName_ = "sender";
    const std::string contractRecipientKeyNameStr = "recipient";
    const std::string contract_input_key_name = "input";
    const std::string contractDonationKey = "donation";
    const std::string contractTransferKeyLabel = "transfer";

    const std::string contractOutputKey = "output";
    const std::string contractCreationKeyLabel = "creation";
    const std::string contract_log_key_name = "log";
    const std::string storageKeyForContract = "storage";
    const std::string contractPreHashKey = "preHash";
    const std::string contractDestructionKey = "destruction";

    const std::string contract_block_timestamp_key_name = "blockTimestamp";
    const std::string contractBlockPrevRandaoKey = "blockPrevRandao";
    const std::string contractBlockCoinbaseKeyName = "blockCoinbase";

    const std::string contractDeployerKeyAlias = "contractDeployer";

    std::optional<evmc::VM> GetEvmInstance();

    int
    executeSynchronouslyWithEvmone(const evmc_message &msg, const evmc::bytes &code, EvmHost &host, std::string &output);

    bool VerifyContractAddress(const std::string &from, const std::string &contractAddress);

    std::optional<std::string> latestContractAddress(const std::string &from);

    int SaveContract(const std::string &deployerAddress, const std::string &deployHash,
                     const std::string &contractAddress, const std::string &contractCode,
                     DBReadWriter &dbReadWriter, global::ca::VmType vmType);

    int DeleteContract(const std::string &deployerAddress, const std::string &contractAddress,
                       DBReadWriter &dbReadWriter, global::ca::VmType vmType);
}


#endif 
