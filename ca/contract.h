/**
 * *****************************************************************************
 * @file        contarct.h
 * @brief       
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef CA_CONTRACT_HEADER_GUARD
#define CA_CONTRACT_HEADER_GUARD

#include <string>
#include <unordered_map>

#include <evmc/evmc.hpp>
#include <ca/evm/evm_host.h>
#include <transaction.pb.h>
#include "ca/evm/evmEnvironment.h"
#include "txhelper.h"
#include "global.h"

namespace Evmone
{
    /**
     * @brief       
     * 
     * @param       host: 
     * @param       txHash:
     * @param       TxType:
     * @param       transactionVersion:
     * @param       txInfoJson: 
     * @param       contractPreHashCache_:   
     * @return      int 
     */
    int
    ContractInfoAdd(const EvmHost &host, const std::string &txHash, global::ca::TxType TxType,
                    uint32_t transactionVersion,
                    nlohmann::json &txInfoJson, std::map<std::string, std::string> &contractPreHashCache_, bool checkContractPreHashEnabled = false);
    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       transferrings: 
     * @param       txInfoJson: 
     * @param       height: 
     * @param       gasCost: 
     * @param       outTx: 
     * @param       type: 
     * @param       info_: 
     * @param       contractTip: 
     * @return      int 
     */
    int fillCallOutTransaction(const std::string &fromAddr, const std::string &toAddr, const std::vector<TransferInfo> &transferrings,
                      const std::pair<std::string, std::string> &gasTrade,
                      const nlohmann::json &txInfoJson, int64_t gasCost, CTransaction &outTx, TxHelper::vrfAgentType &type,
                      const std::string &encodedInfo, bool isFindUtxo);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       txType:
     * @param       transferrings: 
     * @param       gasCost: 
     * @param       outTx: 
     * @param       contractTip: 
     * @param       utxoHashesList: 
     * @param       shouldGenerateSignature: 
     * @return      int 
     */
    int generateCallOutMessage(const std::string &fromAddr, const std::string &toAddr, global::ca::TxType txType, const std::vector<TransferInfo> &transferrings, const std::pair<std::string, std::string> &gasTrade,
                     int64_t gasCost, CTransaction &outTx, std::vector<std::string> &utxoHashesList, bool isFindUtxo,bool shouldGenerateSignature = true, bool isFlowInTx = false,bool isFlowOutTx = false);
    /**
     * @brief       
     * 
     * @param       tx: 
     * @param       callOutRequest: 
     * @return      int 
     */
    int VerifyUtxo(const std::string& sender, const CTransaction& tx, const CTransaction& callOutRequest, int mapping);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       transferrings: 
     * @param       txInfoJson: 
     * @param       gasCost: 
     * @param       height: 
     * @param       outTx: 
     * @param       type: 
     * @param       info_: 
     * @return      int 
     */
    int fillDeployOutTransaction(const std::string &fromAddr, const std::string &toAddr, const std::vector<TransferInfo> &transferrings,
                        const std::pair<std::string, std::string> &gasTrade,
                        const nlohmann::json &txInfoJson, int64_t gasCost, CTransaction &outTx, TxHelper::vrfAgentType &type,
                        const std::string &encodedInfo, bool isFindUtxo);

    /**
     * @brief       
     * 
     * @param       from:
     * @param       host:
     * @return      int
     */
    int DeployContract(const std::string &from, EvmHost &host, evmc_message &message, const std::string &to,
                       int64_t blockTimestamp, int64_t blockPrevRandao, int64_t blockNumber, const std::string &gasType);

    /**
     * @brief       
     * 
     * @param       from:
     * @param       strInput:
     * @param       host:
     * @param       contractTransfer: 
     * @return      int 
     */
    int
    CallContract(const std::string &from, const std::string &strInput, EvmHost &host, const uint64_t &contractTransfer,
                 const std::string &to, evmc_message &message, int64_t blockTimestamp, int64_t blockPrevRandao,
                 int64_t blockNumber, const std::string &gasType);

    int callContractRpcRequest(const std::string &from, const std::string &strInput, EvmHost &host, const uint64_t &contractTransfer,
                 const std::string &to, evmc_message &message, int64_t blockTimestamp, int64_t blockPrevRandao,
                 int64_t blockNumber,const std::string &gasType);

    int
    CallContractWithResult(const std::string &from, const std::string &strInput, EvmHost &host, const uint64_t &contractTransfer,
                 const std::string &to, evmc_message &message, int64_t blockTimestamp, int64_t blockPrevRandao,
                 int64_t blockNumber, const std::string &gasType,std::string &output);

    /**
     * @brief       Get the Storage object
     * 
     * @param       host: 
     * @param       jStorage: 
     * @param       dirtyContract: 
     */
    void GetStorage(const EvmHost &host, nlohmann::json &jStorage, std::set<evmc::address> &dirtyContract);
    /**
     * @brief       
     * 
     * @param       host: 
     * @param       calledContract: 
     */
    void invokedContract(const EvmHost& host, std::vector<std::string>& calledContract);
    void
    fillStorageMap(const std::pair<std::string, std::string> &item, std::map<std::string, std::string> &storages,
                   const std::string &contractAddr);
    int mapHandleFunction(const std::pair<std::string, std::string> &tradeMapping, const std::string &gasAssetTypeEnum, const std::string &hexStr, const std::string &contractAddr, const std::string &contractOwnerEvmAddress,
                              CTransaction &outTx, nlohmann::json &txInfoJson, const std::string &encodedInfo, const EvmHost &host,
                              const evmc_message &message,bool isFindUtxo, bool &isMappingTransaction, bool isFlowOutGasTrade);

    int ValidateMappingTransaction(const std::string &hexStr, const std::string &contractAddr, const std::string &contractOwnerEvmAddress, const CTransaction &outTx, const EvmHost &host,
                                   const evmc_message &message,std::string &output);

    int FillFlowOutVin(const std::pair<std::string, std::string> &gasTrade, CTxUtxos *txUtxo, uint64_t &total, bool isFindUtxo);
}

namespace ContractCommonInterfaceHandler
{
    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       expend: 
     * @param       tempGas: 
     * @param       txUtxo: 
     * @param       utxoHashesList: 
     * @param       total: 
     * @param       isFindUtxo: 
     * @param       isSign:
     * @param       isSign:
     * @return      int 
     */
    int GenVin(const std::pair<std::string, std::string>& addressCurrencyMapping, const uint64_t& expend, const uint64_t& tempGas, CTxUtxos * txUtxo, 
            std::vector<std::string>& utxoHashesList, uint64_t& total, bool isFindUtxo, bool isSign,const std::string& sender = "");
    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       txType: 
     * @param       transfersMap: 
     * @param       gasCost: 
     * @param       outTx: 
     * @param       contractTip: 
     * @param       utxoHashesList: 
     * @param       shouldGenerateSignature: 
     * @return      int 
     */
    int transactionFillings(const std::string &fromAddr, const std::string &toAddr, global::ca::TxType txType,
                            const std::map<std::string, std::map<std::string, uint64_t>> &transfersMap, const std::pair<std::string, std::string> &gasTrade, const int64_t gasCost,
                            CTransaction &outTx, std::vector<std::string> &utxoHashesList, bool isFindUtxo, bool shouldGenerateSignature = true, bool isFlowInTx = false, bool isFlowOutTx = false);
    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       txType: 
     * @param       height: 
     * @param       outTx: 
     * @param       type: 
     * @param       info_: 
     * @return      int 
     */
    int transactionFillings(const std::string &fromAddr, global::ca::TxType txType, 
                            CTransaction &outTx, TxHelper::vrfAgentType &type);
}


/**
 * @brief       
 * 
 * @param       msg: 
 * @param       host:
 * @return      int
 */
int execute_by_evmone(const evmc_message &msg, EvmHost &host,std::string &output);
static std::string soutput = "";
/**
 * @brief
 *
 */
void TestAddressMapping();

bool checkUtxoOwnership(const CTransaction& tx);

/**
 * @brief VerifyContractBaseFee verify contract base fee
 * 
 * @param fromAddr 
 * @param gasCost 
 * @param new_baseFee 
 * @return int 
 */
int VerifyContractBaseFee(const std::string& fromAddr, int64_t& gasCost, double &new_baseFee);

#endif
