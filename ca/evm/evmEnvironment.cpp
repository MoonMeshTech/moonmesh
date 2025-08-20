//
// Created by root on 2024/4/24.
//

#include <utils/string_util.h>
#include "evmEnvironment.h"
#include "db/db_api.h"
#include "include/logging.h"
#include "utils/contract_utils.h"
#include "utils/util.h"
#include "transaction.h"
#include "evm_manager.h"
#include <string>

namespace
{
    int64_t GetChainId()
    {
        static int64_t chainId = 0;
        if (chainId != 0)
        {
            return chainId;
        }

        std::string blockHash;
        DBReader dbReader;
        if (DBStatus::DB_SUCCESS != dbReader.getBlockHashByBlockHeight(0, blockHash))
        {
            ERRORLOG("fail to read genesis block hash")
            return -1;
        }

        std::string genesisBlockPrefix = blockHash.substr(0, 8);
        chainId = StringUtil::StringToNumber(genesisBlockPrefix);
        return chainId;
    }

    int MakeHost(global::ca::TxType contractType, const std::string &sender, const std::string &recipient,
                 uint64_t transferAmount, const evmc::bytes &code, EvmHost &host, int64_t blockTimestamp,
                 int64_t blockPrevRandao, int64_t blockNumber)
    {

        int result = evmEnvironment::createTxContext(sender, host.tx_context, blockTimestamp, blockPrevRandao, blockNumber);
        if (result < 0)
        {
            return -1;
        }

        int64_t nonce = evmEnvironment::GetNonce(sender);
        if (nonce < 0)
        {
            return -2;
        }
        host.nonceCache = nonce;

        host.coinTransfersInProgress.emplace_back(sender, recipient, transferAmount);


        if (contractType != global::ca::TxType::kTransactionTypeDeploy)
        {
            host.accounts[evm_utils::convertStringToEvmAddress(recipient)].set_code(code);
        }
        std::string contractRootHashValue;
        if (contractType != global::ca::TxType::kTransactionTypeDeploy)
        {
            result = fetchContractRootHash(recipient, contractRootHashValue, host.contractDataStorage);
            if (result != 0)
            {
                return -3;
            }
        }

        host.accounts[evm_utils::convertStringToEvmAddress(recipient)].CreateTrie(contractRootHashValue, recipient,
                                                                        host.contractDataStorage);
        return 0;
    }

    int MakeMessage(global::ca::TxType contractType, const evmc_address &sender, const evmc_address &recipient,
                    const evmc::bytes &input, const uint64_t &contractTransfer, const std::string &gasType, evmc_message &message)
    {
        message.sender = sender;
        message.recipient = recipient;

        uint64_t balance = 0;
        std::string inputStr = "";
        if(contractType != global::ca::TxType::kTransactionTypeDeploy){
            inputStr = evm_utils::BytesToString(input);
            inputStr = inputStr.substr(0, 8);
        }
        
        if(inputStr != "6e27d889"){
            int ret = get_balance_by_utxo(evm_utils::evm_addr_to_string(sender), gasType, balance);
            if (ret != 0)
            {
                ERRORLOG("can't get balance of {} ret{}", evm_utils::evm_addr_to_string(sender),ret);
                return -1;
            }
        }

        if(global::ca::GetInitAccountAddr()== evm_utils::evm_addr_to_string(sender) || inputStr == "6e27d889"){
            balance = 111657576591;
        }

        int64_t signedBalance = 0;
        try
        {
            signedBalance = Util::convertUnsigned64ToSigned64(balance);
        }
        catch (const std::exception &e)
        {
            ERRORLOG("balance {} convert fail {}", balance, e.what());
            return -2;
        }

        message.gas = signedBalance;

        if (contractType == global::ca::TxType::kTransactionTypeDeploy)
        {
            message.kind = EVMC_CREATE;
        }
        else if (contractType == global::ca::TxType::TX_TYPE_INVOKE_CONTRACT)
        {
            message.kind = EVMC_CALL;
            message.input_data = input.data();
            message.input_size = input.size();
            dev::u256 value = contractTransfer;
            if (value > 0)
            {
                dev::bytes by = dev::fromHex(dev::toCompactHex(value, 32));
                memcpy(message.value.bytes, &by[0], by.size() * sizeof(uint8_t));
            }
        }
        return 0;
    }

    int createRpcMessage(global::ca::TxType contractType, const evmc_address &sender, const evmc_address &recipient,
                    const evmc::bytes &input, const uint64_t &contractTransfer, evmc_message &message)
    {
        message.sender = sender;
        message.recipient = recipient;

        message.gas = 111657576591;

        if (contractType == global::ca::TxType::kTransactionTypeDeploy)
        {
            message.kind = EVMC_CREATE;
        }
        else if (contractType == global::ca::TxType::TX_TYPE_INVOKE_CONTRACT)
        {
            message.kind = EVMC_CALL;
            message.input_data = input.data();
            message.input_size = input.size();
            dev::u256 value = contractTransfer;
            if (value > 0)
            {
                dev::bytes by = dev::fromHex(dev::toCompactHex(value, 32));
                memcpy(message.value.bytes, &by[0], by.size() * sizeof(uint8_t));
            }
        }
        return 0;
    }
}

int64_t evmEnvironment::GetBlockNumber()
{
    DBReader dbReader;
    uint64_t top;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("getBlockTop for evm block number failed !")
        return -1;
    }

    return Util::convertUnsigned64ToSigned64(top + 1);
}

int64_t evmEnvironment::calculateBlockTimestamp(int64_t predictOffset)
{
    int64_t nowTime = 0;
    try
    {
        nowTime = Util::convertUnsigned64ToSigned64(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());
    }
    catch (const std::exception &e)
    {
        ERRORLOG("{}", e.what())
        return -1;
    }
    int64_t evm_precision_time = nowTime / 1000000;
    return Util::integerRound(evm_precision_time + predictOffset);
}

int64_t evmEnvironment::GetNonce(const std::string &address)
{
    std::vector<std::string> contractAddresses;
    DBReader dbReader;
    const auto &result = dbReader.getContractAddrByDeployerAddr(address, contractAddresses);
    if (result == DBStatus::DB_NOT_FOUND)
    {
        return 0;
    }
    else if (result == DBStatus::DB_SUCCESS)
    {
        return Util::convertUnsigned64ToSigned64(contractAddresses.size());
    }
    else
    {
        ERRORLOG("fail to read genesis block hash")
        return -1;
    }
}

int64_t evmEnvironment::GetNextNonce(const std::string &address)
{
    int64_t nonce = GetNonce(address);
    if (nonce < 0)
    {
        return -1;
    }
    return nonce + 1;

}

int evmEnvironment::createDeploymentMessageRequest(const evmc_address &sender, const evmc_address &recipient, const std::string &gasType, evmc_message &message)
{
    return MakeMessage(global::ca::TxType::kTransactionTypeDeploy, sender, recipient, *std::unique_ptr<evmc::bytes>(), 0,
                       gasType, message);
}

int evmEnvironment::MakeCallMessage(const evmc_address &sender, const evmc_address &recipient, const evmc::bytes &input,
                                     const uint64_t &contractTransfer, const std::string &gasType,evmc_message &message)
{
    evmc_message msg{};
    int result = MakeMessage(global::ca::TxType::TX_TYPE_INVOKE_CONTRACT, sender, recipient, input, contractTransfer, gasType,msg);
    if (result == 0)
    {
        message = msg;
    }
    return result;
}

int
evmEnvironment::rpcCallMessage(const evmc_address &sender, const evmc_address &recipient, const evmc::bytes &input,
                                 const uint64_t &contractTransfer, evmc_message &message)
{
    evmc_message msg{};
    int result = createRpcMessage(global::ca::TxType::TX_TYPE_INVOKE_CONTRACT, sender, recipient, input, contractTransfer, msg);
    if (result == 0)
    {
        message = msg;
    }
    return result;
}

int evmEnvironment::make_deploy_host(const std::string &sender, const std::string &recipient, EvmHost &host,
                                    int64_t blockTimestamp, int64_t blockPrevRandao, int64_t blockNumber,
                                    uint64_t transferAmount)
{
    return MakeHost(global::ca::TxType::kTransactionTypeDeploy, sender, recipient, transferAmount,
                    *std::unique_ptr<evmc::bytes>(),
                    host, blockTimestamp, blockPrevRandao, blockNumber);
}

int evmEnvironment::makeCallHostRequest(const std::string &sender, const std::string &recipient, uint64_t transferAmount,
                                  const evmc::bytes &code,
                                  EvmHost &host, int64_t blockTimestamp, int64_t blockPrevRandao, int64_t blockNumber)
{
    return MakeHost(global::ca::TxType::TX_TYPE_INVOKE_CONTRACT, sender, recipient, transferAmount, code,
                    host, blockTimestamp, blockPrevRandao, blockNumber);
}

int evmEnvironment::createTxContext(const std::string &from, evmc_tx_context &txContext, int64_t blockTimestamp,
                                   int64_t blockPrevRandao, int64_t blockNumber)
{
    txContext.tx_gas_price = evm_utils::convertUint32ToEvmcUint256be(1);
    txContext.tx_origin = evm_utils::convertStringToEvmAddress(from);
    txContext.block_coinbase = evm_utils::convertStringToEvmAddress(
        MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr());
    txContext.block_timestamp = blockTimestamp;
    txContext.block_gas_limit = std::numeric_limits<int64_t>::max();
    txContext.block_prev_randao = evm_utils::convertUint32ToEvmcUint256be(blockPrevRandao);
    txContext.block_base_fee = evm_utils::convertUint32ToEvmcUint256be(1);

    txContext.block_number = blockNumber;
    int64_t chainId = GetChainId();
    if (chainId < 0)
    {
        return -2;
    }
    txContext.chain_id = evm_utils::convertUint32ToEvmcUint256be(chainId);
    return 0;
}

int64_t evmEnvironment::blockPrevRandaoCalculator(const std::string &from, const std::string &gasType, bool isFindUtxo)
{
    std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
    uint64_t total;
    std::multimap<std::string, std::string> fromAddr_assetType;
    fromAddr_assetType.insert(std::make_pair(from, gasType));
    int result = TxHelper::FindUtxo(fromAddr_assetType, TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
    if (result != 0)
    {
        ERRORLOG("can't found utxo from {}, gasType: {}", from, gasType);
        return -1;
    }

    std::string utxoString;
    for (const auto &utxo : outputUtxosSet)
    {
        DEBUGLOG("PrevRandao hash: {}", utxo.hash)
        utxoString += utxo.hash;
    }
    return StringUtil::StringToNumber(utxoString);
}

int64_t evmEnvironment::GetBlockTimestamp(const CTransaction &transaction)
{
    try
    {
        nlohmann::json dataJson = nlohmann::json::parse(transaction.data());
        nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();

        return txInfo[Evmone::contract_block_timestamp_key_name].get<int64_t>();
    }
    catch (...)
    {
        ERRORLOG("fail to get block timestamp from tx {}", transaction.hash())
        return -1;
    }
}

int64_t evmEnvironment::get_block_prev_randao(const CTransaction &transaction)
{
    try
    {
        nlohmann::json dataJson = nlohmann::json::parse(transaction.data());
        nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();

        return txInfo[Evmone::contractBlockPrevRandaoKey].get<int64_t>();
    }
    catch (...)
    {
        ERRORLOG("fail to get block timestamp from tx {}", transaction.hash())
        return -1;
    }
}

bool evmEnvironment::VerifyCoinbase(const CBlock &block, const std::string &coinbase)
{
    std::string transaction_packager = GetPackager(block);
    DEBUGLOG("transaction_packager: {}, coinbase: {}", transaction_packager, coinbase);
    return transaction_packager == coinbase;
}

int64_t evmEnvironment::blockPrevRandaoCalculator(const CTransaction &transaction)
{
    std::string from;
    try
    {
        nlohmann::json dataJson = nlohmann::json::parse(transaction.data());
        nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();
        from = txInfo[Evmone::contractSenderKeyName_].get<std::string>();
    }
    catch (const std::exception &e)
    {
        ERRORLOG("parse transaction data fail {}", e.what())
        return -1;
    }

    std::string utxoString;
    for (auto &utxo : transaction.utxos())
    {
        for (const auto &vin : utxo.vin())
        {
            for (const auto &utxo : vin.prevout())
            {
                DEBUGLOG("PrevRandao hash: {}", utxo.hash())
                utxoString += utxo.hash();
            }
        }
    }
    return StringUtil::StringToNumber(utxoString);
}
