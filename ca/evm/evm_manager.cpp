//
// Created by root on 2024/4/30.
//

#include <utils/account_manager.h>
#include "evm_manager.h"
#include "include/logging.h"
#include "utils/contract_utils.h"
#include "evmEnvironment.h"

std::optional<evmc::VM> Evmone::GetEvmInstance()
{
    struct evmc_vm *pvm = evmc_create_evmone();
    if (!pvm)
    {
        ERRORLOG("can't create evmone instance")
        return {};
    }
    if (!evmc_is_abi_compatible(pvm))
    {
        ERRORLOG("created evmone instance abi not compatible")
        return {};
    }
    evmc::VM vm{pvm};
    return vm;
}

int
Evmone::executeSynchronouslyWithEvmone(const evmc_message &msg, const evmc::bytes &code, EvmHost &host, std::string &output)
{
    auto createResult = Evmone::GetEvmInstance();
    if (!createResult.has_value())
    {
        return -1;
    }

    evmc::VM &vm = createResult.value();

    auto result = vm.execute(host, EVMC_MAX_REVISION, msg, code.data(), code.size());
    DEBUGLOG("ContractAddress: {} , Result: {}", evm_utils::evm_addr_to_string(msg.recipient), result.status_code);
    if (result.status_code != EVMC_SUCCESS)
    {
        ERRORLOG("Evmone execution failed!");
        auto strOutput = std::string_view(reinterpret_cast<const char *>(result.output_data), result.output_size);
        DEBUGLOG("Output:   {}\n", strOutput);
        return -2;
    }
    output = std::move(evmc::hex({result.output_data, result.output_size}));
    DEBUGLOG("Output:   {}\n", output);
    return 0;
}

bool Evmone::VerifyContractAddress(const std::string &from, const std::string &contractAddress)
{
    auto result = latestContractAddress(from);
    if (!result.has_value())
    {
        return false;
    }
    return result.value() == contractAddress;
}

std::optional<std::string> Evmone::latestContractAddress(const std::string &from)
{
    int64_t nonce = evmEnvironment::GetNextNonce(from);
    if (nonce < 0)
    {
        ERRORLOG("can't read nonce of {}", from);
        return {};
    }
    return GenerateAddr(from + std::to_string(nonce));
}

int Evmone::SaveContract(const std::string &deployerAddress, const std::string &deployHash,
                         const std::string &contractAddress, const std::string &contractCode,
                         DBReadWriter &dbReadWriter, global::ca::VmType vmType)
{

    if (DBStatus::DB_SUCCESS != dbReadWriter.setContractDeployUtxoByContractAddr(contractAddress, deployHash))
    {
        return -84;
    }

    if (DBStatus::DB_SUCCESS != dbReadWriter.setContractCodeByContractAddr(contractAddress, contractCode))
    {
        return -84;
    }

    if (DBStatus::DB_SUCCESS != dbReadWriter.setLatestUtxoByContractAddr(contractAddress, deployHash))
    {
        return -90;
    }

    if (DBStatus::DB_SUCCESS != dbReadWriter.setContractAddrByDeployerAddr(deployerAddress, contractAddress))
    {
        return -91;
    }
    std::vector<std::string> deployerAddresses;
    DBStatus ret;
    if (vmType == global::ca::VmType::EVM)
    {
        ret = dbReadWriter.getAllEvmDeployerAddr(deployerAddresses);
    }

    if (DBStatus::DB_SUCCESS != ret && DBStatus::DB_NOT_FOUND != ret)
    {
        return -92;
    }
    auto iter = std::find(deployerAddresses.begin(), deployerAddresses.end(), deployerAddress);
    if (iter == deployerAddresses.end())
    {
        if (vmType == global::ca::VmType::EVM)
        {
            if (DBStatus::DB_SUCCESS != dbReadWriter.setEvmDeployerAddr(deployerAddress))
            {
                return -93;
            }
        }
    }
    return 0;
}

int Evmone::DeleteContract(const std::string &deployerAddress, const std::string &contractAddress,
                           DBReadWriter &dbReadWriter, global::ca::VmType vmType)
{
    if (DBStatus::DB_SUCCESS != dbReadWriter.removeContractDeployUtxoByContractAddr(contractAddress))
    {
        return -88;
    }

    if (DBStatus::DB_SUCCESS != dbReadWriter.removeContractCodeByContractAddr(contractAddress))
    {
        return -89;
    }
    if (DBStatus::DB_SUCCESS != dbReadWriter.removeLatestUtxoByContractAddr(contractAddress))
    {
        return -92;
    }

    if (DBStatus::DB_SUCCESS !=
        dbReadWriter.removeContractAddrByDeployerAddr(deployerAddress, contractAddress))
    {
        return -93;
    }

    std::vector<std::string> deployedUtxos;
    auto ret = dbReadWriter.getContractAddrByDeployerAddr(deployerAddress, deployedUtxos);
    if (DBStatus::DB_NOT_FOUND == ret || deployedUtxos.empty())
    {
        if (vmType == global::ca::VmType::EVM)
        {
            if (DBStatus::DB_SUCCESS != dbReadWriter.removeEvmDeployerAddr(deployerAddress))
            {
                return -94;
            }
        }
    }

    return 0;
}