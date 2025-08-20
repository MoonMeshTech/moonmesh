/**
 * *****************************************************************************
 * @file        algorithm.h
 * @brief       
 * @date        2023-09-25
 * @copyright   mm
 * *****************************************************************************
 */

#ifndef CRYPTO_ALGORITHM_HEADER_GUARD
#define CRYPTO_ALGORITHM_HEADER_GUARD

#include <nlohmann/json.hpp>
#include "global.h"
#include "db/db_api.h"
#include "txhelper.h"
namespace ca_algorithm
{
/**
 * @brief       Get the Abnormal Sign Addr List By Period object
 * 
 * @param       curTime: 
 * @param       abnormal_addr_list: 
 * @param       addr_sign_cnt: 
 * @return      int 
 */
int fetchAbnormalSignAddrListByPeriod(uint64_t &curTime, std::map<std::string, double> &addr_percent, std::unordered_map<std::string, uint64_t> & addrSignCount);

/**
 * @brief       Obtain the time (nanosecond) of pledge transaction with pledge limit of more than 500 according to the addressGet the Pledge Time By Addr object
                When the return value is less than 0, the function execution fails
                Equal to 0 means no pledge
                Greater than 0 means pledge time

 * 
 * @param       addr: 
 * @param       stakeType: 
 * @param       db_reader_ptr: 
 * @return      int64_t 
 */
int64_t GetPledgeTimeByAddr(const std::string &addr, global::ca::StakeType stakeType, DBReader *dbReaderInstance = nullptr);
/**
 * @brief       
 * 
 * @param       tx: 
 * @return      std::string 
 */
std::string calculateTransactionHash(CTransaction tx);
/**
 * @brief       
 * 
 * @param       block: 
 * @return      std::string 
 */
std::string CalcBlockHash(CBlock block);

/**
 * @brief       
 * 
 * @param       cblock: 
 * @return      std::string 
 */
std::string calculateBlockMerkle(CBlock cblock);

/**
 * @brief       
 * 
 * @param       tx: 
 * @param       missingBlockProtocolEnabled: 
 * @param       missing_utxo: 
 * @return      int 
 */
int doubleSpendCheck(const CTransaction &tx, bool missingBlockProtocolEnabled, std::string* missing_utxo = nullptr);

/**
 * @brief       Verification transaction
 * 
 * @param       tx: 
 * @return      int 
 */
int memoryVerificationTransactionRequest(const CTransaction &tx, global::ca::SaveType saveType = global::ca::SaveType::Unknow);

/**
 * @brief       Verification transaction
 * 
 * @param       tx: 
 * @param       tx_height: 
 * @param       missingBlockProtocolEnabled: 
 * @param       verify_abnormal: 
 * @return      int 
 */
int verifyTransactionRequest(const CTransaction &tx, uint64_t txHeight, bool enableMissingBlockRequest = false, bool checkAnomaly = true);

/**
 * @brief       
 * 
 * @param       block: 
 * @return      int 
 */
int verifyPreSaveBlock_(const CBlock &block);
/**
 * @brief       
 * 
 * @param       sign: 
 * @param       serHash: 
 * @return      int 
 */
int VerifySign(const CSign & sign, const std::string & serHash);
/**
 * @brief       Check block
 * 
 * @param       block: 
 * @param       isVerify: 
 * @param       blockStatus: 
 * @return      int 
 */
int memVerifyBlock(const CBlock& block, bool isVerify = true, BlockStatus* blockStat = nullptr, global::ca::SaveType saveType = global::ca::SaveType::Unknow);

/**
 * @brief
 * 
 * @param       isContractVerified: 
 * @param       calledContract: 
 * @return      bool 
 */
bool verifyDirtyContract(const std::vector<std::string> &isContractVerified, const std::vector<std::string> &calledContract);

/**
 * @brief       Verify the contract storage data
 * 
 * @param       txInfo: 
 * @param       expected_tx_info: 
 * @return      int 
 */
int verify_contract_storage(const nlohmann::json& txInfo, const nlohmann::json& expected_tx_info);
/**
 * @brief
 * 
 * @param       ContractTxs: 
 * @param       dependencyTransactionRequest: 
 * @param       block:
 * @param       blockData:
 * @return      int 
 */
int verifyContractDepsTx(const std::map<std::string, CTransaction>& ContractTxs, std::map<std::string,std::vector<std::string>>& dependencyTransactionRequest, const CBlock &block, nlohmann::json& blockData, global::ca::SaveType saveType = global::ca::SaveType::Unknow);

/**
 * @brief       Verify that the contract block is correct
 * 
 * @param       block: 
 * @return      int 
 */
int verifyContractBlock(const CBlock &block, global::ca::SaveType saveType = global::ca::SaveType::Unknow);
/**
 * @brief       Check block
 * 
 * @param       block: 
 * @param       enableMissingBlockRequest: 
 * @param       checkAnomaly: 
 * @param       isVerify: 
 * @param       blockStatus: 
 * @param       msg: 
 * @return      int 
 */
int VerifyBlock(const CBlock &block, bool enableMissingBlockRequest = false, bool checkAnomaly = true, bool isVerify = true, BlockStatus* blockStatus = nullptr,BlockMsg* msg = nullptr, global::ca::SaveType saveType = global::ca::SaveType::Unknow);
/**
 * @brief       
 * 
 * @param       dbWriter: 
 * @param       block: 
 * @param       saveType: 
 * @param       computeMeanValue: 
 * @return      int 
 */
int SaveBlock(DBReadWriter &dbWriter, const CBlock &block, global::ca::SaveType saveType, global::ca::blockMean computeMeanValue);

/**
 * @brief       
 * 
 * @param       dbWriter: 
 * @param       blockHash: 
 * @return      int 
 */
int DeleteBlock(DBReadWriter &dbWriter, const std::string &blockHash);

/**
 * @brief       When calling, pay attention not to have too much difference between the height and the maximum height. The memory occupation is too large, and the process is easy to be killed
                Rollback to specified height
 * 
 * @param       height: 
 * @return      int 
 */
int RollBackToHeight(uint64_t height);

/**
 * @brief       Rollback specified hash
 * 
 * @param       blockHash: 
 * @return      int 
 */
int rollback_by_hash(const std::string &blockHash);
/**
 * @brief       
 * 
 * @param       tx: 
 */
void PrintTx(const CTransaction &tx);
/**
 * @brief       
 * 
 * @param       block: 
 */
void PrintBlock(const CBlock &block);
/**
 * @brief       Calculate the pledge rate and obtain the rate of return
 * 
 * @param       curTime: 
 * @param       bonusAddr: 
 * @param       vlaues: 
 * @param       isDisplay: 
 * @return      int 
 */
int CalcBonusValue(uint64_t &curTime, const std::string &bonusAddr,std::map<std::string, uint64_t> & vlaues,bool isDisplay = false);
/**
 * @brief       Get the Inflation Rate object
 *
 * @param       curTime:
 * @param       annualizedRate:
 * @return      int
 */
int GetAnnualizedRate(const uint64_t &curTime,double &annualizedRate);
/**
 * @brief       Get the Sum Hash Ceiling Height object
 * 
 * @param       height: 
 * @return      uint64_t Ceiling Height
 */
uint64_t calculateSumHashCeilingHeight(uint64_t height);
/**
 * @brief       Get the Sum Hash Floor Height object
 * 
 * @param       height: 
 * @return      uint64_t Floor Height
 */
uint64_t get_sum_hash_floor_height(uint64_t height);
/**
 * @brief       
 * 
 * @param       blockHeight: 
 * @param       dbWriter: 
 * @return      int 
 */
int calculateHeightsSumHash(uint64_t blockHeight, DBReadWriter &dbWriter);

/**
 * @brief       
 * 
 * @param       blockHeight: 
 * @param       dbWriter: 
 * @return      int 
 */
int recalculate_heights_sum_hash(const uint64_t newTop, const uint64_t hashHeightSum, std::string& sumHash, uint64_t blockHeight, DBReadWriter &dbWriter);


/**
 * @brief       
 * 
 * @param       blockHeight: 
 * @param       dbWriter: 
 * @param       backHash: 
 * @return      int 
 */
int calculateSumHashOf1000Heights(uint64_t blockHeight, DBReadWriter &dbWriter, std::string& backHash);

/**
 * @brief       
 * 
 * @param       startHeight: 
 * @param       endHeight: 
 * @param       dbWriter: 
 * @param       sumHash: 
 * @return      true 
 * @return      false 
 */
bool calculate_height_sum_hash(uint64_t startHeight, uint64_t endHeight, DBReadWriter &dbWriter, std::string &sumHash);
/**
 * @brief       Get the Commission Percentage object
 * 
 * @param       addr: 
 * @param       commissionRateRet: Commission Percentage
 * @return      int 0 success
 */
int GetCommissionPercentage(const std::string& addr, double& commissionRateRet);
/**
 * @brief       Get the starting address for executing the contract
 * 
 * @param       transaction:
 * @param       fromAddr:
 * @return      int 0 success
 */
int get_call_contract_from_addr(const CTransaction& transaction, std::string& fromAddr);

void GetAddrType(std::string& addr, std::vector<std::pair<std::string, bool>> &vecAddrType);

int GetAsseTypeByContractAddr(const std::string& contractAddr, std::string& assetType);

int GetVoteAssetType(std::map<std::string, TxHelper::VoteInfo>& voteAsset);

int AssetTypeIsMM(const std::string& assetType);

int GetAssetTypeIsValid(const std::string& assetType, TxHelper::ProposalInfo* proposalInfo = nullptr);

void GetAllMappedAssets();
int GetAssetRate(const std::string& contractAddr, std::string& ExchangeRate);

int fetchAvailableAssetType(std::map<std::string, TxHelper::ProposalInfo>& assetMap);

int GetUnavailableAssetType(std::vector<std::string>& assetList);

int canBeRevoked(const std::string& assetType);

int CheckProposaInfo(std::string& rate, std::string& contractAddr, uint64_t minVote, std::string& assetName);

int GetVoteNum(DBReadWriter& db, const std::string &asserType, uint64_t &approveNum, uint64_t &againstNum);

int CheckForDuplicateVotes(const std::string &asserType, const std::string addr);

int addVoteCount(DBReadWriter& db, const std::string &asserType, const int voteType, const std::string& addr, const uint64_t& voteNumber);

int subVoteNum(DBReadWriter& db, const std::string &asserType, const int voteType, const std::string& addr, const uint64_t& voteNumber);

int CheckVoteTxInfo(const std::string& voteHash, uint32_t voteTxType, int pollType, uint64_t currentTime, global::ca::SaveType saveType = global::ca::SaveType::Unknow);

std::string ConversionTime(const uint64_t time);

int GetProposalInfo(const std::string& assetType, TxHelper::ProposalInfo& proposalInfo);

int AssetTypeListIsEmpty();

int GetCanBeRevokeAssetType(std::string &assetType);

int GetLockValue(const std::string& addr, uint64_t &lockValue);

void Trim(std::string& str);

/**
 * @brief       Get the Bonus Addr Info object
 * 
 * @param       bonusAddrDelegatingMap 
 * @return      int 
 */
int fetchBonusAddressInfo(std::map<std::string, uint64_t> &bonusAddrDelegatingMap);

/**
 * @brief       CaculateGuoKu
 *
 * @param       dbReader
 * @param       bonusExchequer
 * @param       addr
 * @param       currentTime
 * @return      int
 */


template <typename T>
std::vector<T> bidirectionalDifference(const std::vector<T>& a, const std::vector<T>& b) {
    std::vector<T> a_sorted(a);
    std::vector<T> b_sorted(b);
    
    std::sort(a_sorted.begin(), a_sorted.end());
    std::sort(b_sorted.begin(), b_sorted.end());

    std::vector<T> diff;

    std::set_difference(
        a_sorted.begin(), a_sorted.end(),
        b_sorted.begin(), b_sorted.end(),
        std::back_inserter(diff)
    );

    std::set_difference(
        b_sorted.begin(), b_sorted.end(),
        a_sorted.begin(), a_sorted.end(),
        std::back_inserter(diff)
    );

    return diff;
}

}; // namespace ca_algorithm

#endif
