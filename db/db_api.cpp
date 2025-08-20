#include "db/db_api.h"
#include "utils/magic_singleton.h"
#include "include/logging.h"
#include "utils/string_util.h"
#include "ca/global.h"

const std::string KAssetType = "assetType_";    //asset type
const std::string KRevokeTxHash = "revokeTxHash_";
const std::string approveVote = "approveVote_";//approve vote
const std::string againstVoteFlag = "againstVote_";//against vote
const std::string KVoteTxHash = "voteTxHash_";
const std::string kLockAddrKey = "Lockaddr_";
const std::string KAssetInfo = "assetInfo_";
const std::string KRevokeProposalInfo = "revokeProposalInfo_";
const std::string PROPOSAL_VOTES = "proposalVotes_";
const std::string KRevokeProposalVotes = "revokeProposalVotes_";
const std::string KAddrAssetType = "addrAssetType_";
const std::string PROPOSAL_CONTRACT_ADDRESS = "proposalContractAddr_";
const std::string KVoteName = "voteName_";
const std::string KTurnout = "turnout_";

// Block-related interfaces
const std::string BLOCK_HASH_TO_BLOCK_HEIGHT_KEY = "blkhs2blkht_";
const std::string kBlockHeightToBlockHashKey = "blkht2blkhs_";
const std::string BLOCK_HEIGHT_TO_SUM_HASH = "blkht2sumhs_";
const std::string K_TOP_THOUSAND_SUM_HASH_KEY = "topthousandsumhs_";
const std::string kBlockHeight_2000_Sum_Hash = "thousandsblkht2sumhs_";
const std::string K_BLOCK_HASH_TO_BLOCK_RAW_KEY = "blkhs2blkraw_";
const std::string BLOCK_TOP_KEY_VALUE = "blktop_";
const std::string ADDRESS_TO_UTXO_KEY = "addr2utxo_";
const std::string TRANSACTION_HASH_TO_TRANSACTION_RAW_KEY = "txhs2txraw_";
const std::string TRANSACTION_HASH_TO_BLOCK_HASH_KEY = "txhs2blkhs_";

// Transaction inquiry related
const std::string ADDRESS_TO_TRANSACTION_RAW_KEY = "addr2txraw_";
const std::string ADDRESS_TO_BLOCK_HASH_KEY = "addr2blkhs_";
const std::string ADDRESS_TO_TRANSACTION_TOP_KEY = "addr2txtop_";
const std::string kTimeTypeGasamountKey = "timetypegasamount_";
const std::string kTimeTypePackageCountKey = "timetypepackagecount_";
const std::string kTimeTypePackagerKey = "timetypepackager_";
const std::string kTimeTxHashExchequerKey = "timetxhashexchequer_";

// Block-related interfaces
const std::string ADDRESS_TO_BALANCE_KEY = "addr2bal_";
const std::string kStakeAddressKey = "stakeaddr_";
const std::string kMultiSignKey = "mutlisign_";
const std::string BONUS_UTXO_KEY = "bonusutxo_";
const std::string kFundUtxoKey = "fundutxo_";
const std::string BONUS_ADDR_KEY = "bonusaddr_";
const std::string kBonusAddr2DelegatingAddrKey = "bonusaddr2delegatingaddr_";
const std::string kDelegatingAddr2BonusAddrKey = "delegatingaddr2bonusaddr_";
const std::string kBonusAddrDelegatingAddr2DelegatingAddrUtxo = "delegating_nodeaddr2delegatingaddrutxo_";
const std::string kDelegatingUtxoKey = "delegatingutxo_";
const std::string KDelegatingAddr2AssetTypeBalance = "delegatingaddr2assettypebalance_";



const std::string kDM = "DM_"; //Deflationary Mechanism
const std::string kTotaldelegateAmount = "totaldelegateAmount_";
const std::string kTotalLockedAmount = "totallockedamount_";
const std::string kInitializationVersionKey = "initver_";
const std::string kSignatureNumberKey = "signnumber_";
const std::string BLOCK_NUMBER_KEY = "blocknumber_";
const std::string SIGN_ADDR_KEY = "signaddr_";
const std::string BURN_AMOUNT_KEY = "burnamount_";


const std::string kEvmAllDeployerAddress = "allevmdeployeraddr_";
const std::string DEPLOYER_ADDR_TO_CONTRACT_ADDR = "deployeraddr2contractaddr_";
const std::string kContractAddrToContractCode = "contractaddr2contractcode_";
const std::string K_CONTRACT_ADDR_TO_DEPLOY_UTXO = "contractaddr2deployutxo_";
const std::string kContractAddrToLatestUtxo = "contractaddr2latestutxo_";
const std::string LATEST_CONTRACT_BLOCK_HASH = "latestcontractblockhash_";
const std::string kContractMptKey = "contractmpt_";
bool DBInit(const std::string &db_path)
{
    MagicSingleton<RocksDB>::GetInstance()->setDBPath(db_path);
    rocksdb::Status ret_status;
    if (!MagicSingleton<RocksDB>::GetInstance()->initDB(ret_status))
    {
        ERRORLOG("rocksdb init fail {}", ret_status.ToString());
        return false;
    }
    return true;
}
void destroyDatabase()
{
    MagicSingleton<RocksDB>::DesInstance();
}

DBReader::DBReader() : db_reader_(MagicSingleton<RocksDB>::GetInstance())
{
}

DBStatus DBReader::getBlockHashesByBlockHeight(uint64_t startHeight, uint64_t endHeight, std::vector<std::string> &blockHashes)
{
    std::vector<std::string> keys;
    std::vector<std::string> values;
    std::vector<std::string> hashes;
    for (uint64_t index_height = startHeight; index_height <= endHeight; ++index_height)
    {
        keys.push_back(kBlockHeightToBlockHashKey + std::to_string(index_height));
    }
    auto ret = multiReadData(keys, values);
    if (values.empty() && DBStatus::DB_SUCCESS != ret)
    {
        return ret;
    }
    blockHashes.clear();
    for (auto &value : values)
    {
        hashes.clear();
        StringUtil::SplitString(value, "_", hashes);
        blockHashes.insert(blockHashes.end(), hashes.begin(), hashes.end());
    }
    return ret;
}
DBStatus DBReader::getBlocksByBlockHash(const std::vector<std::string> &blockHashes, std::vector<std::string> &blocks)
{
    std::vector<std::string> keys;
    for (auto &hash : blockHashes)
    {
        keys.push_back(K_BLOCK_HASH_TO_BLOCK_RAW_KEY + hash);
    }
    return multiReadData(keys, blocks);
}

// Gets the height of the data block by the block hash
DBStatus DBReader::getBlockHeightByBlockHash(const std::string &blockHash, unsigned int &blockHeight)
{
    if (blockHash.empty())
    {
        return DBStatus::DB_PARAM_NULL;
    }
    std::string db_key = BLOCK_HASH_TO_BLOCK_HEIGHT_KEY + blockHash;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        blockHeight = std::stoul(value);
    }
    return ret;
}


DBStatus DBReader::getBlockHashByBlockHeight(uint64_t blockHeight, std::string &hash)
{
    std::vector<std::string> hashes;
    auto ret = getBlockHashsByBlockHeight(blockHeight, hashes);
    if (DBStatus::DB_SUCCESS == ret && (!hashes.empty()))
    {
        hash = hashes.at(0);
    }
    return ret;
}


// Multiple block hashes are obtained by the height of the data block
DBStatus DBReader::getBlockHashsByBlockHeight(uint64_t blockHeight, std::vector<std::string> &hashes)
{
    std::string db_key = kBlockHeightToBlockHashKey + std::to_string(blockHeight);
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", hashes);
    }
    return ret;
}


// The block information is obtained through the block hash
DBStatus DBReader::getBlockByBlockHash(const std::string &blockHash, std::string &block)
{
    if (blockHash.empty())
    {
        return DBStatus::DB_PARAM_NULL;
    }
    std::string db_key = K_BLOCK_HASH_TO_BLOCK_RAW_KEY + blockHash;
    return readData(db_key, block);
}

// Get Sum hash per 100 heights
DBStatus DBReader::getSumHashByHeight(uint64_t height, std::string& sumHash)
{
    if (height % 100 != 0 || height == 0)
    {
        return DBStatus::DB_PARAM_NULL;
    }

    std::string db_key = BLOCK_HEIGHT_TO_SUM_HASH + std::to_string(height);
    return readData(db_key, sumHash); 
}


DBStatus DBReader::getCheckBlockHashsByBlockHeight(const uint64_t &blockHeight, std::string &sumHash)
{
    if (blockHeight % 1000 != 0 || blockHeight == 0)
    {
        return DBStatus::DB_PARAM_NULL;
    }

    std::string db_key = kBlockHeight_2000_Sum_Hash + std::to_string(blockHeight);
    return readData(db_key, sumHash); 
}


DBStatus DBReader::getTopThousandSumHash(uint64_t &thousandNum)
{
    std::string value;
    auto ret = readData(K_TOP_THOUSAND_SUM_HASH_KEY, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        thousandNum = std::stoul(value);
    }
    return ret;
}



DBStatus DBReader::getBlockTop(uint64_t &blockHeight)
{
    std::string value;
    auto ret = readData(BLOCK_TOP_KEY_VALUE, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        blockHeight = std::stoul(value);
    }
    return ret;
}


// Get the Uxo hash by address (there are multiple utxohashes)
DBStatus DBReader::getUtxoHashsByAddress(const std::string &address, std::vector<std::string> &utxoHashesList)
{
    std::string db_key = ADDRESS_TO_UTXO_KEY + address;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxoHashesList);
    }
    return ret;
}
DBStatus DBReader::getUtxoHashsByAddress(const std::string &address, const std::string& assetType, std::vector<std::string> &utxoHashesList)
{
    std::string db_key = ADDRESS_TO_UTXO_KEY + address + assetType;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxoHashesList);
    }
    return ret;
}

DBStatus DBReader::getUtxoValueByUtxoHashes(const std::string &utxoHash, const std::string &address, std::string &balance)
{
    std::string db_key = address + "_" + utxoHash;
    return readData(db_key, balance);
}

DBStatus DBReader::getUtxoValueByUtxoHashes(const std::string &utxoHash, const std::string &address, const std::string &assetType, std::string &balance)
{
    std::string db_key = address + "_" + utxoHash + "_" + assetType;
    return readData(db_key, balance);
}

// Obtain the transaction raw data by the transaction hash
DBStatus DBReader::getTransactionByHash(const std::string &txHash, std::string &txRaw)
{
    std::string db_key = TRANSACTION_HASH_TO_TRANSACTION_RAW_KEY + txHash;
    return readData(db_key, txRaw);
}

// Get the block hash by transaction hash
DBStatus DBReader::getBlockHashByTransactionHash(const std::string &txHash, std::string &blockHash)
{
    std::string db_key = TRANSACTION_HASH_TO_BLOCK_HASH_KEY + txHash;
    return readData(db_key, blockHash);
}

// Get block transactions by transaction address
DBStatus DBReader::getTransactionByAddress(const std::string &address, const uint32_t txNum, std::string &txRaw)
{
    std::string db_key = ADDRESS_TO_TRANSACTION_RAW_KEY + address + "_" + std::to_string(txNum);
    return readData(db_key, txRaw);
}


// Gets the block hash by the transaction address
DBStatus DBReader::getBlockHashByAddress(const std::string &address, const uint32_t txNum, std::string &blockHash)
{
    std::string db_key = ADDRESS_TO_BLOCK_HASH_KEY + address + "_" + std::to_string(txNum);
    return readData(db_key, blockHash);
}


// Get the maximum height of the transaction by the transaction address
DBStatus DBReader::getTransactionTopByAddress(const std::string &address, unsigned int &txIndex)
{
    std::string db_key = ADDRESS_TO_TRANSACTION_TOP_KEY + address;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        txIndex = std::stoul(value);
    }
    return ret;
}


// Get the account balance by the transaction address
DBStatus DBReader::getBalanceByAddr(const std::string &address, const std::string & assetType, int64_t &balance)
{
    std::string db_key = ADDRESS_TO_BALANCE_KEY + address + assetType;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        balance = std::stol(value);
    }
    return ret;
}

// Get the staking address
DBStatus DBReader::getStakeAddr(std::vector<std::string> &addresses)
{
    std::string value;
    auto ret = readData(kStakeAddressKey, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", addresses);
    }
    return ret;
}

// Get the UTXO of the staking address
DBStatus DBReader::getStakeAddrUtxo(const std::string &address, const std::string& assetType, std::vector<std::string> &utxos)
{
    std::string db_key = kStakeAddressKey + address + assetType;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxos);
    }
    return ret;
}

// Get the multi-Sig address
DBStatus DBReader::getMutliSignAddr(std::vector<std::string> &addresses)
{
    std::string value;
    auto ret = readData(kMultiSignKey, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", addresses);
    }
    return ret;
}


// Gets the UTXO of the multi-signature address
DBStatus DBReader::getMultiSignAddrUtxo(const std::string &address,std::vector<std::string> &utxos)
{
    std::string db_key = kMultiSignKey + address;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxos);
    }
    return ret;
}


// Get the nodes that are delegatinged
DBStatus DBReader::getBonusAddr(std::vector<std::string> &bonus_addresses_list)
{
    std::string db_key = BONUS_ADDR_KEY;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", bonus_addresses_list);
    }
    return ret;
}

// Get the Delegating pledge address Delegating_A:X_Y_Z (where A is the delegatingee address, X, Y, Z is the delegatingor's address)
DBStatus DBReader::getDelegatingAddrByBonusAddr(const std::string &bonusAddr, std::vector<std::string> &addresses)
{
    std::string db_key = kBonusAddr2DelegatingAddrKey + bonusAddr;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", addresses);
    }
    return ret;
}
DBStatus DBReader::getDelegatingAddrByBonusAddr(const std::string &bonusAddr, std::multimap<std::string, std::string> &addresses_assetType)
{
    std::string db_key = kBonusAddr2DelegatingAddrKey + bonusAddr;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", addresses_assetType);
    }

    return ret;
}
// Get the Delegating pledge address Delegating_X:A_B_C (where X is the delegatingor's account and A, B, and C are the delegatingee addresses)
DBStatus DBReader::getBonusAddrByDelegatingAddr(const std::string &address, std::vector<std::string> &nodes)
{
    std::string db_key = kDelegatingAddr2BonusAddrKey + address;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", nodes);
    }
    return ret;
}

DBStatus DBReader::getBonusAddrAndAssetTypeByDelegatingAddr(const std::string &address, std::vector<std::string> &nodes)
{
    std::string db_key = KDelegatingAddr2AssetTypeBalance + address;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", nodes);
    }
    return ret;
}

// Obtain the UTXO Delegating_A_X:u1_u2_u3 of the account that delegatings in pledged assets
DBStatus DBReader::getBonusAddrDelegatingAddrUtxoByBonusAddr(const std::string & addr,const std::string & address, std::vector<std::string> &utxos)
{
    std::string db_key = kBonusAddrDelegatingAddr2DelegatingAddrUtxo + addr + "_" + address;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxos);
    }
    return ret;
}

DBStatus DBReader::getBonusAddrDelegatingAddrUtxoByBonusAddr(const std::string &bonusAddr, const std::string &delegatingAddr,  const std::string assetType, std::vector<std::string> &utxos)
{
    std::string db_key = kBonusAddrDelegatingAddr2DelegatingAddrUtxo + bonusAddr + "_" + delegatingAddr + "_" + assetType;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxos);
    }
    return ret;
}

DBStatus DBReader::getBonusUtxoByPeriod(const uint64_t &period, std::vector<std::string> &utxos)
{
    std::string value;
    auto ret = readData(BONUS_UTXO_KEY + std::to_string(period), value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxos);
    }
    return ret;
}


DBStatus DBReader::getFundUtxoByPeriod(const uint64_t &period, std::vector<std::string> &utxos)
{
    std::string value;
    auto ret = readData(kFundUtxoKey + std::to_string(period), value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxos);
    }
    return ret;
}

DBStatus DBReader::getDelegatingUtxoByPeriod(const uint64_t &period, std::vector<std::string> &utxos)
{
    std::string value;
    auto ret = readData(kDelegatingUtxoKey + std::to_string(period), value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", utxos);
    }
    return ret;
}

//  Get Number of signatures By period
DBStatus DBReader::getSignNumberByPeriod(const uint64_t &period, const std::string &address, uint64_t &SignNumber)
{
    std::string value;
    auto ret = readData(kSignatureNumberKey + std::to_string(period) + address, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        SignNumber = std::stoull(value);
    }
    return ret;
}

//  Get Number of blocks By period
DBStatus DBReader::getBlockNumberByPeriod(const uint64_t &period, uint64_t &BlockNumber)
{
    std::string value;
    auto ret = readData(BLOCK_NUMBER_KEY + std::to_string(period), value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        BlockNumber = std::stoull(value);
    }
    return ret;
}

//  Set Addr of signatures By period
DBStatus DBReader::getSignAddrByPeriod(const uint64_t &period, std::vector<std::string> &signAddresses)
{
    std::string value;
    auto ret = readData(SIGN_ADDR_KEY + std::to_string(period), value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", signAddresses);
    }
    return ret;
}

DBStatus DBReader::getBurnAmountByPeriod(const uint64_t &period, uint64_t &burnAmount)
{
    std::string value;
    auto ret = readData(BURN_AMOUNT_KEY + std::to_string(period), value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        burnAmount = std::stoull(value);
    }
    return ret;
}

DBStatus DBReader::getTotalBurnAmount(uint64_t &totalBurn)
{
    std::string value;
    auto ret = readData(kDM, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        totalBurn = std::stoull(value);
    }
    return ret;
}

// Get the total amount of stake
DBStatus DBReader::getTotalDelegatingAmount(uint64_t &Total)
{
    std::string value;
    auto ret = readData(kTotaldelegateAmount, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        Total = std::stoull(value);
    }
    return ret;
}

DBStatus DBReader::getAllEvmDeployerAddr(std::vector<std::string> &deployerAddr)
{
    std::string value;
    auto ret = readData(kEvmAllDeployerAddress, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value,  "_", deployerAddr);
    }
    return ret;
}


DBStatus DBReader::getContractAddrByDeployerAddr(const std::string &deployerAddr, std::vector<std::string> &contractAddr)
{
    std::string db_key = DEPLOYER_ADDR_TO_CONTRACT_ADDR + deployerAddr;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", contractAddr);
    }
    return ret;
}

DBStatus DBReader::getContractCodeByContractAddr(const std::string &contractAddr, std::string &contractCode)
{
    std::string db_key = kContractAddrToContractCode + contractAddr;
    return readData(db_key, contractCode);
}
DBStatus DBReader::getContractDeployUtxoByContractAddr(const std::string &contractAddr, std::string &contractDeploymentUtxo)
{
    std::string db_key = K_CONTRACT_ADDR_TO_DEPLOY_UTXO + contractAddr;
    return readData(db_key, contractDeploymentUtxo);
}

DBStatus DBReader::getLatestUtxoByContractAddr(const std::string &contractAddr, std::string &Utxo)
{
    std::string db_key = kContractAddrToLatestUtxo + contractAddr;
    return readData(db_key, Utxo);
}

DBStatus DBReader::getMptValueByMptKey(const std::string &mptKey, std::string &MptValue)
{
    std::string db_key = kContractMptKey + mptKey;
    return readData(db_key, MptValue);
}
DBStatus DBReader::getAllAssetType(std::vector<std::string> &assetTypes)
{
    std::string value;
    auto ret = readData(KAssetType, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", assetTypes);
    }
    return ret;
}

DBStatus DBReader::getRevokeTxHashByAssetType(const std::string &asserType, std::vector<std::string> &revokeTxHashs)
{
    std::string db_key = KRevokeTxHash + asserType;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", revokeTxHashs);
    }
    return ret;
}

DBStatus DBReader::getApproveAddrsByAssetHash(const std::string &asserType, std::set<std::string> &addrs)
{
    std::string db_key = approveVote + asserType;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitStringToSet(value, "_", addrs);
    }
    return ret;
}

DBStatus DBReader::getAgainstAddrsByAssetHash(const std::string &asserType, std::set<std::string> &addrs)
{
    std::string db_key = againstVoteFlag + asserType;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitStringToSet(value, "_", addrs);
    }
    return ret;
}

DBStatus DBReader::GetVoteTxHashByAssetHash(const std::string &asserType, std::vector<std::string> &txHashs)
{
    std::string db_key = KVoteTxHash + asserType;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", txHashs);
    }
    return ret;
}


DBStatus DBReader::getAssetInfobyAssetType(const std::string &asserType, std::string& info)
{
    return readData(KAssetInfo + asserType, info);
}

DBStatus DBReader::getRevokeProposalInfobyTxHash(const std::string &TxHash, std::string& info)
{
    return readData(KRevokeProposalInfo + TxHash, info);
}

DBStatus DBReader::getVoteNumByAssetHash(const std::string &asserType, std::string &info)
{
    return readData(PROPOSAL_VOTES + asserType, info);
}

DBStatus DBReader::getVoteNumByAddr(const std::string &addr, const std::string &assetType, uint64_t& num)
{
    std::string value;
    auto ret = readData(KVoteName + assetType + addr, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        num = std::stoull(value);
    }
    return ret;
}

DBStatus DBReader::getTotalNumberOfVotersByAssetHash(const std::string &assetType, uint64_t& num)
{
    std::string value;
    auto ret = readData(KTurnout + assetType, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        num = std::stoull(value);
    }
    return ret;
}

DBStatus DBReader::getLockAddr(std::vector<std::string> &addresses)
{
    std::string value;
    auto ret = readData(kLockAddrKey, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", addresses);
    }
    return ret;
}

DBStatus DBReader::getLockAddrUtxo(const std::string &address, const std::string &assetType, std::vector<std::string> &asserType)
{
    std::string db_key = kLockAddrKey + address + assetType;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", asserType);
    }
    return ret;
}

DBStatus DBReader::getAssetTypeByAddr(const std::string& addr, std::vector<std::string> &asserType)
{
    std::string db_key = KAddrAssetType + addr;
    std::string value;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        StringUtil::SplitString(value, "_", asserType);
    }
    return ret;
}

DBStatus DBReader::getAssetTypeByContractAddr(const std::string& contractAddr, std::string &asserType)
{
    return readData(PROPOSAL_CONTRACT_ADDRESS + contractAddr, asserType);
}

DBStatus DBReader::getGasAmountByPeriod(const uint64_t &period, const std::string &type,uint64_t &gasAmount){
    std::string value;
    std::string db_key = kTimeTypeGasamountKey + std::to_string(period) + "_" + type;
    auto ret = readData(db_key, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        gasAmount = std::stoull(value);
    }
    return ret;
}

DBStatus DBReader::getPackageCountByPeriod(const uint64_t& period,  uint64_t& count){
    std::string value;
    std::string db_key = kTimeTypePackageCountKey + "_" + std::to_string(period);
    auto ret = readData(db_key, value);
    if(DBStatus::DB_SUCCESS == ret){
        count = std::stoull(value);
    }
    return ret;
}


DBStatus DBReader::getPackagerTimesByPeriod(const uint64_t& period, const std::string& address, uint64_t& times){
    std::string value;
    std::string db_key = kTimeTypePackagerKey + std::to_string(period) + "_" + address;
    auto ret = readData(db_key,value);
    if(DBStatus::DB_SUCCESS ==  ret){
        times = std::stoull(value);
    }
    return ret;
}


DBStatus DBReader::getTotalLockedAmonut(uint64_t& TotalLockedAmount){
    std::string value;
    auto ret = readData(kTotalLockedAmount, value);
    if (DBStatus::DB_SUCCESS == ret)
    {
        TotalLockedAmount = std::stoull(value);
    }
    return ret;
}

DBStatus DBReader::getInitVer(std::string &version)
{
    std::string tmpversion;
    auto ret = readData(kInitializationVersionKey, tmpversion);
    if (DBStatus::DB_SUCCESS == ret)
    {
        version = tmpversion;
    }
    return ret;
}

DBStatus DBReader::multiReadData(const std::vector<std::string> &keys, std::vector<std::string> &values)
{
    if (keys.empty())
    {
        return DBStatus::DB_PARAM_NULL;
    }

    std::vector<std::string> cache_values;
    std::vector<std::string> db_keys_str;
    std::vector<rocksdb::Slice> db_keys;

    db_keys_str.reserve(keys.size());
    db_keys.reserve(keys.size());

    for (const auto &key : keys)
    {
        db_keys_str.push_back(key);
        db_keys.push_back(rocksdb::Slice(db_keys_str.back()));
    }

    std::string value;
    std::vector<rocksdb::Status> ret_status;
    if (db_reader_.multiReadData(db_keys, values, ret_status))
    {
        if (db_keys.size() != values.size())
        {
            return DBStatus::DB_ERROR;
        }
        values.insert(values.end(), cache_values.begin(), cache_values.end());
        return DBStatus::DB_SUCCESS;
    }
    else
    {
        for (auto status : ret_status)
        {
            if (status.IsNotFound())
            {
                return DBStatus::DB_NOT_FOUND;
            }
        }
    }
    return DBStatus::DB_ERROR;
}

DBStatus DBReader::readData(const std::string &key, std::string &value)
{
    if (key.empty())
    {
        return DBStatus::DB_PARAM_NULL;
    }

    rocksdb::Status ret_status;
    if (db_reader_.readData(key, value, ret_status))
    {
        return DBStatus::DB_SUCCESS;
    }
    else if (ret_status.IsNotFound())
    {
        value.clear();
        return DBStatus::DB_NOT_FOUND;
    }
    return DBStatus::DB_ERROR;
}

DBReadWriter::DBReadWriter(const std::string &txn_name) : dbReaderWriter(MagicSingleton<RocksDB>::GetInstance(), txn_name)
{
    autoOperationTrans = false;
    reInitTransaction();
}

DBReadWriter::~DBReadWriter()
{
    transactionRollBack();
}
DBStatus DBReadWriter::reInitTransaction()
{
    auto ret = transactionRollBack();
    if (DBStatus::DB_SUCCESS != ret)
    {
        return ret;
    }
    autoOperationTrans = true;
    if (!dbReaderWriter.transactionInit())
    {
        ERRORLOG("transction init error");
        return DBStatus::DB_ERROR;
    }
    return DBStatus::DB_SUCCESS;
}

DBStatus DBReadWriter::transactionCommit()
{
    rocksdb::Status ret_status;
    if (dbReaderWriter.transactionCommit(ret_status))
    {
        autoOperationTrans = false;
        return DBStatus::DB_SUCCESS;
    }
    ERRORLOG("transactionCommit faild:{}:{}", ret_status.code(), ret_status.ToString());
    return DBStatus::DB_ERROR;
}


// Sets the height of the data block by block hash
DBStatus DBReadWriter::setBlockHeightByBlockHash(const std::string &blockHash, const unsigned int blockHeight)
{
    std::string db_key = BLOCK_HASH_TO_BLOCK_HEIGHT_KEY + blockHash;
    return writeData(db_key, std::to_string(blockHeight));
}


// Removes the block height from the database by block hashing
DBStatus DBReadWriter::deleteBlockHeightByBlockHash(const std::string &blockHash)
{
    std::string db_key = BLOCK_HASH_TO_BLOCK_HEIGHT_KEY + blockHash;
    return deleteData(db_key);
}

// Hash data blocks by block height (multiple block hashes at the same height at the same time of concurrency)
DBStatus DBReadWriter::setBlockHashByBlockHeight(const unsigned int blockHeight, const std::string &blockHash, bool isMainBlock)
{
    std::string db_key = kBlockHeightToBlockHashKey + std::to_string(blockHeight);
    return mergeValue(db_key, blockHash, isMainBlock);
}


// Remove the hash of a block in the database by block height
DBStatus DBReadWriter::removeBlockHashByBlockHeight(const unsigned int blockHeight, const std::string &blockHash)
{
    std::string db_key = kBlockHeightToBlockHashKey + std::to_string(blockHeight);
    return removeMergeValue(db_key, blockHash);
}


// Set blocks by block hash
DBStatus DBReadWriter::setBlockByBlockHash(const std::string &blockHash, const std::string &block)
{
    std::string db_key = K_BLOCK_HASH_TO_BLOCK_RAW_KEY + blockHash;
    return writeData(db_key, block);
}


// Remove blocks inside a data block by block hashing
DBStatus DBReadWriter::deleteBlockByBlockHash(const std::string &blockHash)
{
    std::string db_key = K_BLOCK_HASH_TO_BLOCK_RAW_KEY + blockHash;
    return deleteData(db_key);
}


// Set Sum hash per 100 heights
DBStatus DBReadWriter::setSumHashByHeight(uint64_t height, const std::string& sumHash)
{
    std::string db_key = BLOCK_HEIGHT_TO_SUM_HASH + std::to_string(height);
    return writeData(db_key, sumHash);
}

// Remove Sum hash per 100 heights
DBStatus DBReadWriter::removeSumHashByHeight(uint64_t height)
{
    std::string db_key = BLOCK_HEIGHT_TO_SUM_HASH + std::to_string(height);
    return deleteData(db_key);
}


//Set  Sum hash per 1000 heights
DBStatus DBReadWriter::setCheckBlockHashsByBlockHeight(const uint64_t &blockHeight ,const std::string &sumHash)
{
    std::string db_key = kBlockHeight_2000_Sum_Hash + std::to_string(blockHeight);
    return writeData(db_key, sumHash);
}

//Set  Sum hash per 1000 heights
DBStatus DBReadWriter::removeCheckBlockHashsByBlockHeight(const uint64_t &blockHeight)
{
    std::string db_key = kBlockHeight_2000_Sum_Hash + std::to_string(blockHeight);
    return deleteData(db_key);
}


DBStatus DBReadWriter::setTopThousandSumHash(const uint64_t &thousandNum)
{
    return writeData(K_TOP_THOUSAND_SUM_HASH_KEY, std::to_string(thousandNum));
}

DBStatus DBReadWriter::removeTopThousandSumhash(const uint64_t &thousandNum)
{
    return deleteData(K_TOP_THOUSAND_SUM_HASH_KEY);
}

// Set the highest block
DBStatus DBReadWriter::setBlockTop(const unsigned int blockHeight)
{
    return writeData(BLOCK_TOP_KEY_VALUE, std::to_string(blockHeight));
}


DBStatus DBReadWriter::setUtxoHashesByAddr(const std::string &address, const std::string &assetType, const std::string &utxoHash)
{
    std::string db_key = ADDRESS_TO_UTXO_KEY + address + assetType;
    return mergeValue(db_key, utxoHash);
}

// Remove the Utoucho hash by address
DBStatus DBReadWriter::removeUtxoHashesByAddr(const std::string &address, const std::string &assetType, const std::string &utxoHash)
{
    std::string db_key = ADDRESS_TO_UTXO_KEY + address + assetType;
    return removeMergeValue(db_key, utxoHash);
}

DBStatus DBReadWriter::setUtxoValueByUtxoHashes(const std::string &utxoHash, const std::string &address, const std::string &assetType, const std::string &balance)
{
    std::string db_key = address + "_" + utxoHash + "_" + assetType;
    return writeData(db_key, balance);
}

DBStatus DBReadWriter::removeUtxoValueByUtxoHashes(const std::string &utxoHash, const std::string &address, const std::string &assetType, const std::string &balance)
{
    std::string db_key = address + "_" + utxoHash + "_" + assetType;
    return deleteData(db_key);
}

// Set the transaction raw data by transaction hash
DBStatus DBReadWriter::setTransactionByHash(const std::string &txHash, const std::string &txRaw)
{
    std::string db_key = TRANSACTION_HASH_TO_TRANSACTION_RAW_KEY + txHash;
    return writeData(db_key, txRaw);
}


// Remove the transaction raw data from the database by transaction hash
DBStatus DBReadWriter::seleteTransactionByHash(const std::string &txHash)
{
    std::string db_key = TRANSACTION_HASH_TO_TRANSACTION_RAW_KEY + txHash;
    return deleteData(db_key);
}


// Set the block hash by transaction hash
DBStatus DBReadWriter::setBlockHashByTransactionHash(const std::string &txHash, const std::string &blockHash)
{
    std::string db_key = TRANSACTION_HASH_TO_BLOCK_HASH_KEY + txHash;
    return writeData(db_key, blockHash);
}

// Remove the block hash from the database by transaction hashing
DBStatus DBReadWriter::seleteBlockHashByTransactionHash(const std::string &txHash)
{
    std::string db_key = TRANSACTION_HASH_TO_BLOCK_HASH_KEY + txHash;
    return deleteData(db_key);
}


// Set up block transactions by transaction address
DBStatus DBReadWriter::setTransactionByAddress(const std::string &address, const uint32_t txNum, const std::string &txRaw)
{
    std::string db_key = ADDRESS_TO_TRANSACTION_RAW_KEY + address + "_" + std::to_string(txNum);
    return writeData(db_key, txRaw);
}

// Remove transaction data from the database by transaction address
DBStatus DBReadWriter::deleteTransactionByAddress(const std::string &address, const uint32_t txNum)
{
    std::string db_key = ADDRESS_TO_TRANSACTION_RAW_KEY + address + "_" + std::to_string(txNum);
    return deleteData(db_key);
}


// Set the block hash by transaction address
DBStatus DBReadWriter::setBlockHashByAddress(const std::string &address, const uint32_t txNum, const std::string &blockHash)
{
    std::string db_key = ADDRESS_TO_BLOCK_HASH_KEY + address + "_" + std::to_string(txNum);
    return writeData(db_key, blockHash);
}


// Remove the block hash in the database by the transaction address
DBStatus DBReadWriter::deleteBlockHashByAddress(const std::string &address, const uint32_t txNum)
{
    std::string db_key = ADDRESS_TO_BLOCK_HASH_KEY + address + "_" + std::to_string(txNum);
    return deleteData(db_key);
}


// Set the maximum height of the transaction by the transaction address
DBStatus DBReadWriter::setTransactionTopByAddress(const std::string &address, const unsigned int txIndex)
{
    std::string db_key = ADDRESS_TO_TRANSACTION_TOP_KEY + address;
    return writeData(db_key, std::to_string(txIndex));
}


// Set the account balance by the transaction address
DBStatus DBReadWriter::setBalanceByAddr(const std::string &address, const std::string &assetType, int64_t balance)
{
    std::string db_key = ADDRESS_TO_BALANCE_KEY + address + assetType;
    return writeData(db_key, std::to_string(balance));
}

DBStatus DBReadWriter::deleteBalanceByAddr(const std::string &address, const std::string &assetType)
{
    std::string db_key = ADDRESS_TO_BALANCE_KEY + address + assetType;
    return deleteData(db_key);
}



// Set the staking address
DBStatus DBReadWriter::setStakeAddr(const std::string &address)
{
    return mergeValue(kStakeAddressKey, address);
}


// Remove the staking address from the database
DBStatus DBReadWriter::removeStakeAddr(const std::string &address)
{
    return removeMergeValue(kStakeAddressKey, address);
}


// Set up a multi-Sig address
DBStatus DBReadWriter::setMutliSignAddr(const std::string &address)
{
    return mergeValue(kMultiSignKey, address);
}


// Remove the multi-Sig address from the database
DBStatus DBReadWriter::removeMutliSignAddr(const std::string &address)
{
    return removeMergeValue(kMultiSignKey, address);
}


// Set up UTXO for the Holddown Asset Account
DBStatus DBReadWriter::setStakeAddrUtxo(const std::string &stakeAddr, const std::string& assetType, const std::string &utxo)
{
    std::string db_key = kStakeAddressKey + stakeAddr + assetType;
    return mergeValue(db_key, utxo);
}

// Remove the UTXO from the data
DBStatus DBReadWriter::removeStakeAddrUtxo(const std::string &stakeAddr, const std::string& assetType, const std::string &utxo)
{
    std::string db_key = kStakeAddressKey + stakeAddr + assetType;
    return removeMergeValue(db_key, utxo);
}


// Set up a UTXO for a multi-Sig asset account
DBStatus DBReadWriter::setMultiSignAddrUtxo(const std::string &address, const std::string &utxo)
{
    std::string db_key = kMultiSignKey + address;
    return mergeValue(db_key, utxo);
}


// Remove UTXO from multi-sig data
DBStatus DBReadWriter::removeMultiSignAddrUtxo(const std::string &address, const std::string &utxos)
{
    std::string db_key = kMultiSignKey + address;
    return removeMergeValue(db_key, utxos);
}


// Set up the node to be delegatinged
DBStatus DBReadWriter::setBonusAddr(const std::string &bonusAddr)
{
    std::string db_key = BONUS_ADDR_KEY;
    return mergeValue(db_key, bonusAddr);
}


// Remove the delegated staking address from the database
DBStatus DBReadWriter::removeBonusAddr(const std::string &bonusAddr)
{
    std::string db_key = BONUS_ADDR_KEY;
    return removeMergeValue(db_key, bonusAddr);
}


// Set the delegated staking address Delegating_A:X_Y_Z
DBStatus DBReadWriter::setDelegatingAddrByBonusAddr(const std::string &bonusAddr, const std::string& delegatingAddr, const std::string assetType)
{
    DEBUGLOG("setDelegatingAddrByBonusAddr : bonusAddr: {} , delegatingAddr : {} , assetType: {}", bonusAddr, delegatingAddr, assetType);
    std::string db_key = kBonusAddr2DelegatingAddrKey + bonusAddr;
    std::string value = delegatingAddr + "-" + assetType;
    return mergeValue(db_key, value);
}

// Remove the delegated staking address from the database
DBStatus DBReadWriter::removeDelegatingAddrByBonusAddr(const std::string &bonusAddr, const std::string& delegatingAddr, const std::string assetType)
{
    std::string db_key = kBonusAddr2DelegatingAddrKey + bonusAddr;
    std::string value = delegatingAddr + "-" + assetType;
    return removeMergeValue(db_key, value);
}



// Set which node the delegatingor delegatinged in
DBStatus DBReadWriter::setBonusAddrByDelegatingAddr(const std::string &delegatingAddr, const std::string& bonusAddr)
{
    std::string db_key = kDelegatingAddr2BonusAddrKey + delegatingAddr;
    return mergeValue(db_key, bonusAddr);
}
DBStatus DBReadWriter::setBonusAddrAndAssetTypeByDelegatingAddr(const std::string &delegatingAddr, const std::string &assetType, const std::string& bonusAddr)
{
    std::string db_key = KDelegatingAddr2AssetTypeBalance + delegatingAddr;
    std::string value = bonusAddr + "-" + assetType;
    return mergeValue(db_key, value);
}

DBStatus DBReadWriter::removeBonusAddrAndAssetTypeByDelegatingAddr(const std::string &delegatingAddr, const std::string &assetType, const std::string& bonusAddr)
{
    std::string db_key = KDelegatingAddr2AssetTypeBalance + delegatingAddr;
    std::string value = bonusAddr + "-" + assetType;
    return removeMergeValue(db_key, value);
}

// Set the UTXO Delegating_A_X:u1_u2_u3 corresponding to the node where you delegating in yourself
DBStatus DBReadWriter::setBonusAddrDelegatingAddrUtxoByBonusAddr(const std::string &bonusAddr, const std::string &delegatingAddr, const std::string assetType, const std::string &utxo)
{
    std::string db_key = kBonusAddrDelegatingAddr2DelegatingAddrUtxo + bonusAddr + "_" + delegatingAddr + "_" + assetType;
    return mergeValue(db_key, utxo);
}


// Remove the UTXO corresponding to the node where you delegatinged in it
DBStatus DBReadWriter::removeBonusAddrDelegatingAddrUtxoByBonusAddr(const std::string &bonusAddr, const std::string &delegatingAddr, const std::string assetType, const std::string &utxo)
{
    std::string db_key = kBonusAddrDelegatingAddr2DelegatingAddrUtxo + bonusAddr + "_" + delegatingAddr + "_" + assetType;
    return removeMergeValue(db_key, utxo);
}

// Set up a bonus transaction
DBStatus DBReadWriter::setBonusUtxoByPeriod(const uint64_t &period, const std::string &utxo)
{
    return mergeValue(BONUS_UTXO_KEY + std::to_string(period), utxo);
}
// Remove a bonus transaction
DBStatus DBReadWriter::removeBonusUtxoByPeriod(const uint64_t &period, const std::string &utxo)
{
    return removeMergeValue(BONUS_UTXO_KEY + std::to_string(period), utxo);
}


// Set up a bonus transaction
DBStatus DBReadWriter::setFundUtxoByPeriod(const uint64_t &period, const std::string &utxo)
{
    return mergeValue(kFundUtxoKey + std::to_string(period), utxo);
}
// Remove a bonus transaction
DBStatus DBReadWriter::removeFundUtxoByPeriod(const uint64_t &period, const std::string &utxo)
{
    return removeMergeValue(kFundUtxoKey + std::to_string(period), utxo);
}

// Set up an Delegating transaction
DBStatus DBReadWriter::setDelegatingUtxoByPeriod(const uint64_t &period, const std::string &utxo)
{
    return mergeValue(kDelegatingUtxoKey + std::to_string(period), utxo);
}
// Remove an Delegating transaction
DBStatus DBReadWriter::removeDelegatingUtxoByPeriod(const uint64_t &period, const std::string &utxo)
{
    return removeMergeValue(kDelegatingUtxoKey + std::to_string(period),utxo);
}

DBStatus DBReadWriter::setEvmDeployerAddr(const std::string &deployerAddr)
{
    return mergeValue(kEvmAllDeployerAddress, deployerAddr);
}
DBStatus DBReadWriter::removeEvmDeployerAddr(const std::string &deployerAddr)
{
    return removeMergeValue(kEvmAllDeployerAddress, deployerAddr);
}

DBStatus DBReadWriter::setContractAddrByDeployerAddr(const std::string &deployerAddr, const std::string &contractAddr)
{
    std::string db_key = DEPLOYER_ADDR_TO_CONTRACT_ADDR + deployerAddr;
    return mergeValue(db_key, contractAddr);
}

DBStatus DBReadWriter::removeContractAddrByDeployerAddr(const std::string &deployerAddr, const std::string &contractAddr)
{
    std::string db_key = DEPLOYER_ADDR_TO_CONTRACT_ADDR + deployerAddr;
    return removeMergeValue(db_key, contractAddr);
}

DBStatus DBReadWriter::setContractCodeByContractAddr(const std::string &contractAddr, const std::string &contractCode)
{
    std::string db_key = kContractAddrToContractCode + contractAddr;
    return writeData(db_key, contractCode);
}

DBStatus DBReadWriter::removeContractCodeByContractAddr(const std::string &contractAddr)
{
    std::string db_key = kContractAddrToContractCode + contractAddr;
    return deleteData(db_key);
}

DBStatus DBReadWriter::setContractDeployUtxoByContractAddr(const std::string &contractAddr, const std::string &contractDeploymentUtxo)
{
    std::string db_key = K_CONTRACT_ADDR_TO_DEPLOY_UTXO + contractAddr;
    return writeData(db_key, contractDeploymentUtxo);
}

DBStatus DBReadWriter::removeContractDeployUtxoByContractAddr(const std::string &contractAddr)
{
    std::string db_key = K_CONTRACT_ADDR_TO_DEPLOY_UTXO + contractAddr;
    return deleteData(db_key);
}

DBStatus DBReadWriter::setLatestUtxoByContractAddr(const std::string &contractAddr, const std::string &utxo)
{
    std::string db_key = kContractAddrToLatestUtxo + contractAddr;
    return writeData(db_key, utxo);
}

DBStatus DBReadWriter::removeLatestUtxoByContractAddr(const std::string &contractAddr)
{
    std::string db_key = kContractAddrToLatestUtxo + contractAddr;
    return deleteData(db_key);
}

DBStatus DBReadWriter::setMptValueByMptKey(const std::string &mptKey, const std::string &MptValue)
{
    std::string db_key = kContractMptKey + mptKey;
    return writeData(db_key, MptValue);
}
DBStatus DBReadWriter::removeMptValueByMptKey(const std::string &mptKey)
{
    std::string db_key = kContractMptKey + mptKey;
    return deleteData(db_key);
}
//  Set Number of signatures By period
DBStatus DBReadWriter::setSignNumberByPeriod(const uint64_t &period, const std::string &address, const uint64_t &SignNumber)
{
    std::string db_key = kSignatureNumberKey + std::to_string(period) + address;
    return writeData(db_key, std::to_string(SignNumber));
}

//  Remove Number of signatures By period
DBStatus DBReadWriter::removeSignNumberByPeriod(const uint64_t &period, const std::string &address)
{
    std::string db_key = kSignatureNumberKey + std::to_string(period) + address;
    return deleteData(db_key);
}

//  Set Number of blocks By period
DBStatus DBReadWriter::setBlockNumberByPeriod(const uint64_t &period, const uint64_t &BlockNumber)
{
    std::string db_key = BLOCK_NUMBER_KEY + std::to_string(period);
    return writeData(db_key, std::to_string(BlockNumber));
}


//  Remove Number of blocks By period
DBStatus DBReadWriter::removeBlockNumberByPeriod(const uint64_t &period)
{
    std::string db_key = BLOCK_NUMBER_KEY + std::to_string(period);
    return deleteData(db_key);
}

//  Set Addr of signatures By period
DBStatus DBReadWriter::setSignAddrByPeriod(const uint64_t &period, const std::string &addr)
{
    return mergeValue(SIGN_ADDR_KEY + std::to_string(period), addr);
}

//  Remove Addr of signatures By period
DBStatus DBReadWriter::removeSignAddrByPeriod(const uint64_t &period, const std::string &addr)
{
    return removeMergeValue(SIGN_ADDR_KEY + std::to_string(period), addr);
}

DBStatus DBReadWriter::setBurnAmountByPeriod(const uint64_t &period, const uint64_t &burnAmount)
{
    return writeData(BURN_AMOUNT_KEY + std::to_string(period), std::to_string(burnAmount));
}
DBStatus DBReadWriter::removeBurnAmountByPeriod(const uint64_t &period, const uint64_t &burnAmount)
{
    return deleteData(BURN_AMOUNT_KEY + std::to_string(period));
}

DBStatus DBReadWriter::setTotalBurnAmount(uint64_t &totalBurn)
{
    return writeData(kDM, std::to_string(totalBurn));
}


// Set the total amount of stake
DBStatus DBReadWriter::setTotalDelegatingAmount(uint64_t &delegatingCount)
{
    return writeData(kTotaldelegateAmount, std::to_string(delegatingCount));
}


// Record the version of the program that initialized the database
DBStatus DBReadWriter::setInitVer(const std::string &version)
{
    return writeData(kInitializationVersionKey, version);
}

DBStatus DBReadWriter::setAssetType(const std::string &asserType)
{
    return mergeValue(KAssetType, asserType);
}

DBStatus DBReadWriter::removeAssetType(const std::string &asserType)
{
    return removeMergeValue(KAssetType, asserType);
}

DBStatus DBReadWriter::setRevokeTxHashByAssetType(const std::string &asserType, const std::string revokeTransactionHashValue)
{
    std::string db_key = KRevokeTxHash + asserType;
    return mergeValue(db_key, revokeTransactionHashValue);
}

DBStatus DBReadWriter::removeRevokeTxHashByAssetType(const std::string &asserType, const std::string revokeTransactionHashValue)
{
    std::string db_key = KRevokeTxHash + asserType;
    return removeMergeValue(db_key, revokeTransactionHashValue);
}

DBStatus DBReadWriter::setApproveVoteByAssetHash(const std::string &asserType, const std::string &addr)
{
    std::string db_key = approveVote + asserType;
    return mergeValue(db_key, addr);
}

DBStatus DBReadWriter::removeApproveVoteByAssetHash(const std::string &asserType, const std::string &addr)
{
    std::string db_key = approveVote + asserType;
    return removeMergeValue(db_key, addr);
}

DBStatus DBReadWriter::setAgainstVoteByAssetHash(const std::string &asserType, const std::string &addr)
{
    std::string db_key = againstVoteFlag + asserType;
    return mergeValue(db_key, addr);
}

DBStatus DBReadWriter::removeAgainstVoteByAssetHash(const std::string &asserType, const std::string &addr)
{
    std::string db_key = againstVoteFlag + asserType;
    return removeMergeValue(db_key, addr);
}

DBStatus DBReadWriter::setVoteTxHashByAssetHash(const std::string &asserType, const std::string &voteTxHash)
{
    std::string db_key = KVoteTxHash + asserType;
    return mergeValue(db_key, voteTxHash);
}

DBStatus DBReadWriter::removeVoteTxHashByAssetHash(const std::string &asserType, const std::string &voteTxHash)
{
    std::string db_key = KVoteTxHash + asserType;
    return removeMergeValue(db_key, voteTxHash);
}

DBStatus DBReadWriter::setAssetInfobyAssetType(const std::string &asserType, const std::string info)
{
    return writeData(KAssetInfo + asserType, info);
}
DBStatus DBReadWriter::removeAssetInfobyAssetType(const std::string &asserType)
{
    return deleteData(KAssetInfo + asserType);
}

DBStatus DBReadWriter::setRevokeProposalInfobyTxHash(const std::string &TxHash, const std::string info)
{
    return writeData(KRevokeProposalInfo + TxHash, info);
}

DBStatus DBReadWriter::removeRevokeProposalInfobyTxHash(const std::string &TxHash)
{
    return deleteData(KRevokeProposalInfo + TxHash);
}

DBStatus DBReadWriter::setVoteNumByAssetHash(const std::string &asserType, const std::string &info)
{
    return writeData(PROPOSAL_VOTES + asserType, info);
}

DBStatus DBReadWriter::deleteVoteNumByAssetHash(const std::string &asserType)
{
    return deleteData(PROPOSAL_VOTES + asserType);
}

DBStatus DBReadWriter::setVoteNumByAddr(const std::string &addr, const std::string& asserType, const uint64_t& voteNum)
{
    return writeData(KVoteName + asserType + addr, std::to_string(voteNum));
}

DBStatus DBReadWriter::seleteVoteNumByAddr(const std::string &addr, const std::string& asserType)
{
    return deleteData(KVoteName + asserType + addr);
}

DBStatus DBReadWriter::setTotalNumberOfVotersByAssetHash(const std::string& asserType, const uint64_t& voteNum)
{
    return writeData(KTurnout + asserType, std::to_string(voteNum));
}

DBStatus DBReadWriter::deleteTotalNumberOfVotersByAssetHash(const std::string& asserType)
{
    return deleteData(KTurnout + asserType);
}

DBStatus DBReadWriter::setLockAddr(const std::string &address)
{
    return mergeValue(kLockAddrKey, address);
}

DBStatus DBReadWriter::removeLockAddr(const std::string &address)
{
    return removeMergeValue(kLockAddrKey, address);
}

DBStatus DBReadWriter::setLockAddrUtxo(const std::string &LockAddr,const std::string& assetType ,const std::string &utxo)
{
    std::string db_key = kLockAddrKey + LockAddr + assetType;
    return mergeValue(db_key, utxo);
}

DBStatus DBReadWriter::removeLockAddrUtxo(const std::string &LockAddr, const std::string& assetType ,const std::string &utxo)
{
    std::string db_key = kLockAddrKey + LockAddr + assetType;
    return removeMergeValue(db_key, utxo);
}

DBStatus DBReadWriter::setTotalLockedAmonut(const uint64_t& TotalLockedAmount){
    
    return writeData(kTotalLockedAmount, std::to_string(TotalLockedAmount));
}

DBStatus DBReadWriter::setAssetTypeByAddr(const std::string& addr, const std::string &asserType)
{
    std::string db_key = KAddrAssetType + addr;
    return mergeValue(db_key, asserType);
}

DBStatus DBReadWriter::removeAssetTypeByAddr(const std::string& addr, const std::string &asserType)
{
    std::string db_key = KAddrAssetType + addr;
    return removeMergeValue(db_key, asserType);
}


DBStatus DBReadWriter::setAssetTypeByContractAddr(const std::string& contractAddr, const std::string &asserType)
{
    return writeData(PROPOSAL_CONTRACT_ADDRESS + contractAddr, asserType);
}

DBStatus DBReadWriter::removeAssetTypeByContractAddr(const std::string& contractAddr)
{
    return deleteData(PROPOSAL_CONTRACT_ADDRESS + contractAddr);
}

DBStatus DBReadWriter::setGasAmountByPeriod(const uint64_t &period, const std::string &type,const uint64_t &gasAmount){
    std::string db_key = kTimeTypeGasamountKey + std::to_string(period) + "_" + type;
    return writeData(db_key,std::to_string(gasAmount));
}

DBStatus DBReadWriter::setPackageCountByPeriod(const uint64_t& period, const std::string& assetType, const uint64_t& count){
    std::string db_key = kTimeTypePackageCountKey + std::to_string(period) + "_" + assetType;
    return writeData(db_key,std::to_string(count));
}


DBStatus DBReadWriter::setBonusExchequerByTxHashAndPeriod(const std::string &txHash, const uint64_t &time, const uint64_t &bonusExchequer)
{
    std::string timeStr = std::to_string(time);
    std::string db_key = kTimeTxHashExchequerKey + txHash + "_" + timeStr;
    return mergeValue(db_key, std::to_string(bonusExchequer));
}

DBStatus DBReadWriter::removeBonusExchequerByTxHashAndPeriod(const std::string &txHash, const uint64_t &time, const uint64_t &bonusExchequer)
{
    std::string timeStr = std::to_string(time);
    std::string db_key = kTimeTxHashExchequerKey + txHash + "_" + timeStr;
    return removeMergeValue(db_key, std::to_string(bonusExchequer));
}

DBStatus DBReadWriter::setPackagerTimesByPeriod(const uint64_t& period, const std::string& address, const uint64_t& times){
    std::string db_key = kTimeTypePackagerKey + std::to_string(period)  + "_" + address;
    return writeData(db_key,std::to_string(times));
}

DBStatus DBReadWriter::transactionRollBack()
{
    if (autoOperationTrans)
    {
        rocksdb::Status ret_status;
        if (!dbReaderWriter.transactionRollBack(ret_status))
        {
            ERRORLOG("transction rollback code:{} info:{}", ret_status.code(), ret_status.ToString());
            return DBStatus::DB_ERROR;
        }
    }
    return DBStatus::DB_SUCCESS;
}

DBStatus DBReadWriter::multiReadData(const std::vector<std::string> &keys, std::vector<std::string> &values)
{
    if (keys.empty())
    {
        return DBStatus::DB_PARAM_NULL;
    }
    std::vector<rocksdb::Slice> db_keys;
    std::vector<std::string> str;
    for(auto t : keys)
    {
        str.push_back(t);
    }

    std::vector<std::string> db_keys_str;
    for (auto key : keys)
    {
        db_keys_str.push_back(key);
        db_keys.push_back(db_keys_str.back());
    }
    std::vector<rocksdb::Status> ret_status;
    if (dbReaderWriter.multiReadData(db_keys, values, ret_status))
    {
        if (db_keys.size() != values.size())
        {
            return DBStatus::DB_ERROR;
        }
        return DBStatus::DB_SUCCESS;
    }
    else
    {
        for (auto status : ret_status)
        {
            if (status.IsNotFound())
            {
                return DBStatus::DB_NOT_FOUND;
            }
        }
    }
    return DBStatus::DB_ERROR;
}

DBStatus DBReadWriter::readData(const std::string &key, std::string &value)
{
    if (key.empty())
    {
        return DBStatus::DB_PARAM_NULL;
    }
    rocksdb::Status ret_status;
    if (dbReaderWriter.readData(key, value, ret_status))
    {
        return DBStatus::DB_SUCCESS;
    }
    else if (ret_status.IsNotFound())
    {
        value.clear();
        return DBStatus::DB_NOT_FOUND;
    }
    return DBStatus::DB_ERROR;
}
DBStatus DBReadWriter::mergeValue(const std::string &key, const std::string &value, bool firstOrLast)
{
    rocksdb::Status ret_status;
    if (dbReaderWriter.mergeValue(key, value, ret_status, firstOrLast))
    {
        return DBStatus::DB_SUCCESS;
    }
    return DBStatus::DB_ERROR;
}
DBStatus DBReadWriter::removeMergeValue(const std::string &key, const std::string &value)
{
    rocksdb::Status ret_status;
    if (dbReaderWriter.removeMergeValue(key, value, ret_status))
    {
        return DBStatus::DB_SUCCESS;
    }
    return DBStatus::DB_ERROR;
}
DBStatus DBReadWriter::writeData(const std::string &key, const std::string &value)
{
    rocksdb::Status ret_status;
    if (dbReaderWriter.writeData(key, value, ret_status))
    {
        return DBStatus::DB_SUCCESS;
    }
    return DBStatus::DB_ERROR;
}
DBStatus DBReadWriter::deleteData(const std::string &key)
{
    rocksdb::Status ret_status;
    if (dbReaderWriter.deleteData(key, ret_status))
    {
        return DBStatus::DB_SUCCESS;
    }
    return DBStatus::DB_ERROR;
}

