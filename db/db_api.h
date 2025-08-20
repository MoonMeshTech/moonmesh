#ifndef DATABASE_DB_API_HEADER
#define DATABASE_DB_API_HEADER

#include "db/rocksdb_read.h"
#include "db/rocksdb_read_write.h"
#include "proto/block.pb.h"
#include <string>
#include <vector>

bool DBInit(const std::string &path);
void destroyDatabase();

enum DBStatus
{
    DB_SUCCESS = 0,                  // Operation successful
    DB_ERROR = 1,                    // General error
    DB_PARAM_NULL = 2,               // Parameter is null
    DB_NOT_FOUND = 3,                // Data not found
    DB_IS_EXIST = 4,                 // Data already exists
    DB_DESERIALIZATION_FAILED = 5    // Deserialization failed
};

class DBReader
{
public:
    DBReader();
    ~DBReader() = default;
    DBReader(DBReader &&) = delete;
    DBReader(const DBReader &) = delete;
    DBReader &operator=(DBReader &&) = delete;
    DBReader &operator=(const DBReader &) = delete;
    
    /**
     * @brief Get the list of multi-signature addresses
     * 
     * @param addresses String vector to store multi-signature addresses
     * @return DBStatus Operation result status code
     */
    DBStatus getMutliSignAddr(std::vector<std::string> &addresses);

    /**
     * @brief Get the UTXO list for a specific multi-signature address
     * 
     * @param address Multi-signature address
     * @param utxos String vector to store UTXOs
     * @return DBStatus Operation result status code
     */
    DBStatus getMultiSignAddrUtxo(const std::string &address, std::vector<std::string> &utxos);

    /**
     * @brief Get block hash list by block height range
     * 
     * @param startHeight Start block height
     * @param endHeight End block height
     * @param blockHashes String vector to store block hashes
     * @return DBStatus Operation result status code
     */
    DBStatus getBlockHashesByBlockHeight(uint64_t startHeight, uint64_t endHeight, std::vector<std::string> &blockHashes);

    /**
     * @brief Get block contents by block hash list
     * 
     * @param blockHashes Vector of block hashes
     * @param blocks String vector to store block contents
     * @return DBStatus Operation result status code
     */
    DBStatus getBlocksByBlockHash(const std::vector<std::string> &blockHashes, std::vector<std::string> &blocks);

    /**
     * @brief Get block height by block hash
     * 
     * @param blockHash Block hash
     * @param blockHeight Variable to store block height
     * @return DBStatus Operation result status code
     */
    DBStatus getBlockHeightByBlockHash(const std::string &blockHash, unsigned int &blockHeight);

    /**
     * @brief Get block hash by block height
     * 
     * @param blockHeight Block height
     * @param hash Variable to store block hash
     * @return DBStatus Operation result status code
     */
    DBStatus getBlockHashByBlockHeight(uint64_t blockHeight, std::string &hash);

    /**
     * @brief Get all block hashes by block height (may have forks)
     * 
     * @param blockHeight Block height
     * @param hashes String vector to store block hashes
     * @return DBStatus Operation result status code
     */
    DBStatus getBlockHashsByBlockHeight(uint64_t blockHeight, std::vector<std::string> &hashes);

    /**
     * @brief Get block content by block hash
     * 
     * @param blockHash Block hash
     * @param block Variable to store block content
     * @return DBStatus Operation result status code
     */
    DBStatus getBlockByBlockHash(const std::string &blockHash, std::string &block);

    /**
     * @brief Get summary hash by block height
     * 
     * @param height Block height
     * @param sumHash Variable to store summary hash
     * @return DBStatus Operation result status code
     */
    DBStatus getSumHashByHeight(uint64_t height, std::string& sumHash);

    /**
     * @brief Get check block hash by block height
     * 
     * @param blockHeight Block height
     * @param sumHash Variable to store check hash
     * @return DBStatus Operation result status code
     */
    DBStatus getCheckBlockHashsByBlockHeight(const uint64_t &blockHeight, std::string &sumHash);

    /**
     * @brief Get the summary hash of the top thousand blocks
     * 
     * @param thousandNum Variable to store the number of blocks
     * @return DBStatus Operation result status code
     */
    DBStatus getTopThousandSumHash(uint64_t &thousandNum);

    /**
     * @brief Get the top block height of the current blockchain
     * 
     * @param blockHeight Variable to store the top block height
     * @return DBStatus Operation result status code
     */
    DBStatus getBlockTop(uint64_t &blockHeight);

    /**
     * @brief Get all UTXO hashes for a given address
     * 
     * @param address Address
     * @param utxoHashesList String vector to store UTXO hashes
     * @return DBStatus Operation result status code
     */
    DBStatus getUtxoHashsByAddress(const std::string &address, std::vector<std::string> &utxoHashesList);

    /**
     * @brief Get all UTXO hashes for a given address and asset type
     * 
     * @param address Address
     * @param assetType Asset type
     * @param utxoHashesList String vector to store UTXO hashes
     * @return DBStatus Operation result status code
     */
    DBStatus getUtxoHashsByAddress(const std::string &address, const std::string& assetType, std::vector<std::string> &utxoHashesList);

    /**
     * @brief Get balance by UTXO hash and address
     * 
     * @param utxoHash UTXO hash
     * @param address Address
     * @param balance Variable to store balance
     * @return DBStatus Operation result status code
     */
    DBStatus getUtxoValueByUtxoHashes(const std::string &utxoHash, const std::string &address, std::string &balance);

    /**
     * @brief Get balance by UTXO hash, address and asset type
     * 
     * @param utxoHash UTXO hash
     * @param address Address
     * @param assetType Asset type
     * @param balance Variable to store balance
     * @return DBStatus Operation result status code
     */
    DBStatus getUtxoValueByUtxoHashes(const std::string &utxoHash, const std::string &address, const std::string &assetType, std::string &balance);

    /**
     * @brief Get transaction raw data by transaction hash
     * 
     * @param txHash Transaction hash
     * @param txRaw Variable to store transaction raw data
     * @return DBStatus Operation result status code
     */
    DBStatus getTransactionByHash(const std::string &txHash, std::string &txRaw);

    /**
     * @brief Get block hash by transaction hash
     * 
     * @param txHash Transaction hash
     * @param blockHash Variable to store block hash
     * @return DBStatus Operation result status code
     */
    DBStatus getBlockHashByTransactionHash(const std::string &txHash, std::string &blockHash);
    
    /**
     * @brief       Get block transaction by transaction address
     * 
     * @param       address:
     * @param       txNum:
     * @param       txRaw:
     * @return      DBStatus
     */
    [[deprecated("Not used")]]
    DBStatus getTransactionByAddress(const std::string &address, const uint32_t txNum, std::string &txRaw);
    /**
     * @brief       Get block hash from transaction address
     * 
     * @param       address:
     * @param       txNum:
     * @param       blockHash:
     * @return      DBStatus
     */
    [[deprecated("Not used")]]
    DBStatus getBlockHashByAddress(const std::string &address, const uint32_t txNum, std::string &blockHash);
    /**
     * @brief       Obtain the highest transaction height through the transaction address
     * 
     * @param       address:
     * @param       txIndex:
     * @return      DBStatus
     */
    [[deprecated("Not used")]]
    DBStatus getTransactionTopByAddress(const std::string &address, unsigned int &txIndex);

    /**
     * @brief Get the balance for a given address and asset type
     * 
     * @param address Address
     * @param assetType Asset type
     * @param balance Variable to store the balance
     * @return DBStatus Operation result status code
     */
    DBStatus getBalanceByAddr(const std::string &address, const std::string& assetType, int64_t &balance);

    /**
     * @brief Get the list of all stake addresses
     * 
     * @param addresses String vector to store stake addresses
     * @return DBStatus Operation result status code
     */
    DBStatus getStakeAddr(std::vector<std::string> &addresses);

    /**
     * @brief Get the list of stake UTXOs for a given address and asset type
     * 
     * @param address Stake address
     * @param assetType Asset type
     * @param utxos String vector to store UTXOs
     * @return DBStatus Operation result status code
     */
    DBStatus getStakeAddrUtxo(const std::string &address, const std::string& assetType, std::vector<std::string> &utxos);

    /**
     * @brief Get the list of all bonus addresses
     * 
     * @param bonus_addresses_list String vector to store bonus addresses
     * @return DBStatus Operation result status code
     */
    DBStatus getBonusAddr(std::vector<std::string> &bonus_addresses_list);

    /**
     * @brief Get the list of delegating addresses by bonus address
     * 
     * @param bonusAddr Bonus address
     * @param delegatingAddr String vector to store delegating addresses
     * @return DBStatus Operation result status code
     */
    DBStatus getDelegatingAddrByBonusAddr(const std::string &bonusAddr, std::vector<std::string> &delegatingAddr);

    /**
     * @brief Get the delegating addresses and related info by bonus address (multimap)
     * 
     * @param bonusAddr Bonus address
     * @param delegatingAddr Multimap to store delegating addresses and related info
     * @return DBStatus Operation result status code
     */
    DBStatus getDelegatingAddrByBonusAddr(const std::string &bonusAddr, std::multimap<std::string, std::string> &delegatingAddr);

    /**
     * @brief Get the list of bonus node addresses by delegating address
     * 
     * @param address Delegating address
     * @param nodes String vector to store bonus node addresses
     * @return DBStatus Operation result status code
     */
    DBStatus getBonusAddrByDelegatingAddr(const std::string &address, std::vector<std::string> &nodes);

    /**
     * @brief Get the bonus node addresses and asset types by delegating address
     * 
     * @param address Delegating address
     * @param nodes String vector to store bonus node addresses and asset types
     * @return DBStatus Operation result status code
     */
    DBStatus getBonusAddrAndAssetTypeByDelegatingAddr(const std::string &address, std::vector<std::string> &nodes);

    /**
     * @brief Get the UTXO list by bonus address and delegating address
     * 
     * @param addr Bonus address
     * @param address Delegating address
     * @param utxos String vector to store UTXOs
     * @return DBStatus Operation result status code
     */
    DBStatus getBonusAddrDelegatingAddrUtxoByBonusAddr(const std::string &addr,const std::string &address, std::vector<std::string> &utxos);

    /**
     * @brief Get the UTXO list by bonus address, delegating address and asset type
     * 
     * @param bonusAddr Bonus address
     * @param delegatingAddr Delegating address
     * @param assetType Asset type
     * @param utxos String vector to store UTXOs
     * @return DBStatus Operation result status code
     */
    DBStatus getBonusAddrDelegatingAddrUtxoByBonusAddr(const std::string &bonusAddr, const std::string &delegatingAddr,  const std::string assetType, std::vector<std::string> &utxos);

    /**
     * @brief Get the bonus UTXO list by period
     * 
     * @param period Period
     * @param utxos String vector to store UTXOs
     * @return DBStatus Operation result status code
     */
    DBStatus getBonusUtxoByPeriod(const uint64_t &period, std::vector<std::string> &utxos);

    /**
     * @brief Get the fund UTXO list by period
     * 
     * @param period Period
     * @param utxos String vector to store UTXOs
     * @return DBStatus Operation result status code
     */
    DBStatus getFundUtxoByPeriod(const uint64_t &period, std::vector<std::string> &utxos);

    /**
     * @brief Get the delegating UTXO list by period
     * 
     * @param period Period
     * @param utxos String vector to store UTXOs
     * @return DBStatus Operation result status code
     */
    DBStatus getDelegatingUtxoByPeriod(const uint64_t &period, std::vector<std::string> &utxos);

    /**
     * @brief Get the number of signatures by period and address
     * 
     * @param period Period
     * @param address Address
     * @param signNumber Variable to store the number of signatures
     * @return DBStatus Operation result status code
     */
    DBStatus getSignNumberByPeriod(const uint64_t &period, const std::string &address, uint64_t &signNumber);

    /**
     * @brief Get the number of blocks by period
     * 
     * @param period Period
     * @param blockNumber Variable to store the number of blocks
     * @return DBStatus Operation result status code
     */
    DBStatus getBlockNumberByPeriod(const uint64_t &period, uint64_t &blockNumber);

    /**
     * @brief Get the list of sign addresses by period
     * 
     * @param period Period
     * @param signAddresses String vector to store sign addresses
     * @return DBStatus Operation result status code
     */
    DBStatus getSignAddrByPeriod(const uint64_t &period, std::vector<std::string> &signAddresses);

    /**
     * @brief Get the burn amount by period
     * 
     * @param period Period
     * @param burnAmount Variable to store the burn amount
     * @return DBStatus Operation result status code
     */
    DBStatus getBurnAmountByPeriod(const uint64_t &period, uint64_t &burnAmount);


    /**
     * @brief Get the total delegate amount
     * 
     * @param Total Variable to store the total delegate amount
     * @return DBStatus Operation result status code
     */
    DBStatus getTotalDelegatingAmount(uint64_t &Total);

    /**
     * @brief Get the total burn amount
     * 
     * @param totalBurn Variable to store the total burn amount
     * @return DBStatus Operation result status code
     */
    DBStatus getTotalBurnAmount(uint64_t &totalBurn);

    /**
     * @brief Get all EVM contract deployer addresses
     * 
     * @param deployerAddr String vector to store deployer addresses
     * @return DBStatus Operation result status code
     */
    DBStatus getAllEvmDeployerAddr(std::vector<std::string> &deployerAddr);

    /**
     * @brief Get contract addresses by deployer address
     * 
     * @param deployerAddr Deployer address
     * @param contractAddr String vector to store contract addresses
     * @return DBStatus Operation result status code
     */
    DBStatus getContractAddrByDeployerAddr(const std::string &deployerAddr, std::vector<std::string> &contractAddr);

    /**
     * @brief Get contract code by contract address
     * 
     * @param contractAddr Contract address
     * @param contractCode Variable to store contract code
     * @return DBStatus Operation result status code
     */
    DBStatus getContractCodeByContractAddr(const std::string &contractAddr, std::string &contractCode);

    /**
     * @brief Get contract deployment UTXO by contract address
     * 
     * @param contractAddr Contract address
     * @param contractDeploymentUtxo Variable to store contract deployment UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus getContractDeployUtxoByContractAddr(const std::string &contractAddr, std::string &contractDeploymentUtxo);

    /**
     * @brief Get the latest UTXO by contract address
     * 
     * @param contractAddr Contract address
     * @param Utxo Variable to store the latest UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus getLatestUtxoByContractAddr(const std::string &contractAddr, std::string &Utxo);

    /**
     * @brief Get the MPT value by MPT key
     * 
     * @param mptKey MPT key
     * @param mptValue Variable to store MPT value
     * @return DBStatus Operation result status code
     */
    DBStatus getMptValueByMptKey(const std::string &mptKey, std::string &mptValue);

    /**
     * @brief Get all asset types
     * 
     * @param assetTypes String vector to store all asset types
     * @return DBStatus Operation result status code
     */
    DBStatus getAllAssetType(std::vector<std::string> &assetTypes);

    /**
     * @brief Get revoke transaction hashes by asset type
     * 
     * @param asserType Asset type
     * @param revokeTxHashs String vector to store revoke transaction hashes
     * @return DBStatus Operation result status code
     */
    DBStatus getRevokeTxHashByAssetType(const std::string &asserType, std::vector<std::string> &revokeTxHashs);

    /**
     * @brief Get approve addresses by asset hash
     * 
     * @param asserType Asset hash
     * @param addrs Set to store approve addresses
     * @return DBStatus Operation result status code
     */
    DBStatus getApproveAddrsByAssetHash(const std::string &asserType, std::set<std::string> &addrs);

    /**
     * @brief Get against addresses by asset hash
     * 
     * @param asserType Asset hash
     * @param addrs Set to store against addresses
     * @return DBStatus Operation result status code
     */
    DBStatus getAgainstAddrsByAssetHash(const std::string &asserType, std::set<std::string> &addrs);

    /**
     * @brief Get vote transaction hashes by asset hash
     * 
     * @param asserType Asset hash
     * @param txHashs String vector to store vote transaction hashes
     * @return DBStatus Operation result status code
     */
    DBStatus GetVoteTxHashByAssetHash(const std::string &asserType, std::vector<std::string> &txHashs);

    /**
     * @brief Get asset information by asset type
     * 
     * @param asserType Asset type
     * @param info String to store asset information
     * @return DBStatus Operation result status code
     */
    DBStatus getAssetInfobyAssetType(const std::string &asserType, std::string& info);

    /**
     * @brief Get revoke proposal information by transaction hash
     * 
     * @param TxHash Transaction hash
     * @param info String to store revoke proposal information
     * @return DBStatus Operation result status code
     */
    DBStatus getRevokeProposalInfobyTxHash(const std::string &TxHash, std::string& info);

    /**
     * @brief Get vote number information by asset hash
     * 
     * @param asserType Asset hash
     * @param info String to store vote number information
     * @return DBStatus Operation result status code
     */
    DBStatus getVoteNumByAssetHash(const std::string &asserType, std::string &info);

    /**
     * @brief Get vote number by address and asset type
     * 
     * @param addr Address
     * @param assetType Asset type
     * @param num Variable to store vote number
     * @return DBStatus Operation result status code
     */
    DBStatus getVoteNumByAddr(const std::string &addr, const std::string &assetType, uint64_t& num);

    /**
     * @brief Get total number of voters by asset hash
     * 
     * @param assetType Asset hash
     * @param num Variable to store total number of voters
     * @return DBStatus Operation result status code
     */
    DBStatus getTotalNumberOfVotersByAssetHash(const std::string &assetType, uint64_t& num);

    /**
     * @brief Get all lock addresses
     * 
     * @param addresses String vector to store lock addresses
     * @return DBStatus Operation result status code
     */
    DBStatus getLockAddr(std::vector<std::string> &addresses);

    /**
     * @brief Get UTXO list by lock address and asset type
     * 
     * @param address Lock address
     * @param assetType Asset type
     * @param utxos String vector to store UTXOs
     * @return DBStatus Operation result status code
     */
    DBStatus getLockAddrUtxo(const std::string &address, const std::string &assetType, std::vector<std::string> &utxos);

    /**
     * @brief Get asset types by address
     * 
     * @param addr Address
     * @param asserType String vector to store asset types
     * @return DBStatus Operation result status code
     */
    DBStatus getAssetTypeByAddr(const std::string& addr, std::vector<std::string> &asserType);

    /**
     * @brief Get asset type by contract address
     * 
     * @param contractAddr Contract address
     * @param asserType String to store asset type
     * @return DBStatus Operation result status code
     */
    DBStatus getAssetTypeByContractAddr(const std::string& contractAddr, std::string &asserType);

    /**
     * @brief Get gas amount by period and type
     * 
     * @param period Period
     * @param type Type
     * @param gasAmount Variable to store gas amount
     * @return DBStatus Operation result status code
     */
    DBStatus getGasAmountByPeriod(const uint64_t &period, const std::string &type,uint64_t &gasAmount);

    /**
     * @brief Get package count by period
     * 
     * @param period Period
     * @param count Variable to store package count
     * @return DBStatus Operation result status code
     */
    DBStatus getPackageCountByPeriod(const uint64_t& period, uint64_t& count);

    /**
     * @brief Get packager times by period and address
     * 
     * @param period Period
     * @param address Address
     * @param times Variable to store packager times
     * @return DBStatus Operation result status code
     */
    DBStatus getPackagerTimesByPeriod(const uint64_t& period, const std::string& address, uint64_t& times);

    /**
     * @brief Get total locked amount
     * 
     * @param TotalLockedAmount Variable to store total locked amount
     * @return DBStatus Operation result status code
     */
    DBStatus getTotalLockedAmonut(uint64_t& TotalLockedAmount);


    /**
     * @brief Get the database initialization version
     * 
     * @param version Variable to store the version
     * @return DBStatus Operation result status code
     */
    DBStatus getInitVer(std::string &version);

    /**
     * @brief Batch read multiple key-value pairs
     * 
     * @param keys The collection of keys to read
     * @param values The collection to store the read values
     * @return DBStatus Operation result status code
     */
    virtual DBStatus multiReadData(const std::vector<std::string> &keys, std::vector<std::string> &values);

    /**
     * @brief Read a single key-value pair
     * 
     * @param key The key to read
     * @param value The variable to store the read value
     * @return DBStatus Operation result status code
     */
    virtual DBStatus readData(const std::string &key, std::string &value);


private:
    RocksDBDataReader db_reader_;
};

class DBReadWriter : public DBReader
{
public:
    DBReadWriter(const std::string &txn_name = std::string());
    virtual ~DBReadWriter();
    DBReadWriter(DBReadWriter &&) = delete;
    DBReadWriter(const DBReadWriter &) = delete;
    DBReadWriter &operator=(DBReadWriter &&) = delete;
    DBReadWriter &operator=(const DBReadWriter &) = delete;

    /**
     * @brief Re-initialize the transaction
     * 
     * @return DBStatus Operation result status code
     */
    DBStatus reInitTransaction();

    /**
     * @brief Commit the current transaction
     * 
     * @return DBStatus Operation result status code
     */
    DBStatus transactionCommit();

    /**
     * @brief Set block height by block hash
     * 
     * @param blockHash Block hash
     * @param blockHeight Block height
     * @return DBStatus Operation result status code
     */
    DBStatus setBlockHeightByBlockHash(const std::string &blockHash, const unsigned int blockHeight);

    /**
     * @brief Delete block height by block hash
     * 
     * @param blockHash Block hash
     * @return DBStatus Operation result status code
     */
    DBStatus deleteBlockHeightByBlockHash(const std::string &blockHash);

    /**
     * @brief Set block hash by block height
     * 
     * @param blockHeight Block height
     * @param blockHash Block hash
     * @param isMainBlock Whether it is the main block, default is false
     * @return DBStatus Operation result status code
     */
    DBStatus setBlockHashByBlockHeight(const unsigned int blockHeight, const std::string &blockHash, bool isMainBlock = false);

    /**
     * @brief Remove block hash by block height
     * 
     * @param blockHeight Block height
     * @param blockHash Block hash
     * @return DBStatus Operation result status code
     */
    DBStatus removeBlockHashByBlockHeight(const unsigned int blockHeight, const std::string &blockHash);

    /**
     * @brief Set block content by block hash
     * 
     * @param blockHash Block hash
     * @param block Block content
     * @return DBStatus Operation result status code
     */
    DBStatus setBlockByBlockHash(const std::string &blockHash, const std::string &block);

    /**
     * @brief Delete block content by block hash
     * 
     * @param blockHash Block hash
     * @return DBStatus Operation result status code
     */
    DBStatus deleteBlockByBlockHash(const std::string &blockHash);

    /**
     * @brief Set sum hash by height
     * 
     * @param height Block height
     * @param sumHash Sum hash
     * @return DBStatus Operation result status code
     */
    DBStatus setSumHashByHeight(uint64_t height, const std::string& sumHash);

    /**
     * @brief Remove sum hash by height
     * 
     * @param height Block height
     * @return DBStatus Operation result status code
     */
    DBStatus removeSumHashByHeight(uint64_t height);   

    /**
     * @brief Set check block hashes by block height
     * 
     * @param blockHeight Block height
     * @param sumHash Check hash
     * @return DBStatus Operation result status code
     */
    DBStatus setCheckBlockHashsByBlockHeight(const uint64_t &blockHeight ,const std::string &sumHash);

    /**
     * @brief Remove check block hashes by block height
     * 
     * @param blockHeight Block height
     * @return DBStatus Operation result status code
     */
    DBStatus removeCheckBlockHashsByBlockHeight(const uint64_t &blockHeight);

    /**
     * @brief Set top thousand sum hash
     * 
     * @param thousandNum Thousand block number
     * @return DBStatus Operation result status code
     */
    DBStatus setTopThousandSumHash(const uint64_t &thousandNum);

    /**
     * @brief Remove top thousand sum hash
     * 
     * @param thousandNum Thousand block number
     * @return DBStatus Operation result status code
     */
    DBStatus removeTopThousandSumhash(const uint64_t &thousandNum);   

    /**
     * @brief Set blockchain top height
     * 
     * @param blockHeight Block height
     * @return DBStatus Operation result status code
     */
    DBStatus setBlockTop(const unsigned int blockHeight);

    /**
     * @brief Add multi-signature address
     * 
     * @param address Multi-signature address
     * @return DBStatus Operation result status code
     */
    DBStatus setMutliSignAddr(const std::string &address);

    /**
     * @brief Remove multi-signature address
     * 
     * @param address Multi-signature address
     * @return DBStatus Operation result status code
     */
    DBStatus removeMutliSignAddr(const std::string &address);

    /**
     * @brief Set UTXO for multi-signature address
     * 
     * @param address Multi-signature address
     * @param utxo UTXO information
     * @return DBStatus Operation result status code
     */
    DBStatus setMultiSignAddrUtxo(const std::string &address, const std::string &utxo);

    /**
     * @brief Remove UTXO for multi-signature address
     * 
     * @param address Multi-signature address
     * @param utxos UTXO information
     * @return DBStatus Operation result status code
     */
    DBStatus removeMultiSignAddrUtxo(const std::string &address, const std::string &utxos);

    /**
     * @brief Set UTXO hash for address and asset type
     * 
     * @param address Address
     * @param assetType Asset type
     * @param utxoHash UTXO hash
     * @return DBStatus Operation result status code
     */
    DBStatus setUtxoHashesByAddr(const std::string &address, const std::string &assetType, const std::string &utxoHash);

    /**
     * @brief Remove UTXO hash for address and asset type
     * 
     * @param address Address
     * @param assetType Asset type
     * @param utxoHash UTXO hash
     * @return DBStatus Operation result status code
     */
    DBStatus removeUtxoHashesByAddr(const std::string &address, const std::string &assetType, const std::string &utxoHash);

    /**
     * @brief Set balance information for UTXO hash
     * 
     * @param utxoHash UTXO hash
     * @param address Address
     * @param assetType Asset type
     * @param balance Balance
     * @return DBStatus Operation result status code
     */
    DBStatus setUtxoValueByUtxoHashes(const std::string &utxoHash, const std::string &address, const std::string &assetType, const std::string &balance);

    /**
     * @brief Remove balance information for UTXO hash
     * 
     * @param utxoHash UTXO hash
     * @param address Address
     * @param assetType Asset type
     * @param balance Balance
     * @return DBStatus Operation result status code
     */
    DBStatus removeUtxoValueByUtxoHashes(const std::string &utxoHash, const std::string &address, const std::string &assetType, const std::string &balance);

    /**
     * @brief Set transaction content by transaction hash
     * 
     * @param txHash Transaction hash
     * @param txRaw Transaction raw content
     * @return DBStatus Operation result status code
     */
    DBStatus setTransactionByHash(const std::string &txHash, const std::string &txRaw);

    /**
     * @brief Delete transaction content by transaction hash
     * 
     * @param txHash Transaction hash
     * @return DBStatus Operation result status code
     */
    DBStatus seleteTransactionByHash(const std::string &txHash);

    /**
     * @brief Set block hash by transaction hash
     * 
     * @param txHash Transaction hash
     * @param blockHash Block hash
     * @return DBStatus Operation result status code
     */
    DBStatus setBlockHashByTransactionHash(const std::string &txHash, const std::string &blockHash);

    /**
     * @brief Delete block hash by transaction hash
     * 
     * @param txHash Transaction hash
     * @return DBStatus Operation result status code
     */
    DBStatus seleteBlockHashByTransactionHash(const std::string &txHash);

    /**
     * @brief       Set block transaction by transaction address
     * 
     * @param       address:
     * @param       txNum:
     * @param       txRaw:
     * @return      DBStatus
     */
    [[deprecated("Not used")]]
    DBStatus setTransactionByAddress(const std::string &address, const uint32_t txNum, const std::string &txRaw);
    /**
     * @brief       Remove the transaction data in the database through the transaction address
     * 
     * @param       address:
     * @param       txNum:
     * @return      DBStatus
     */

    [[deprecated("Not used")]]
    DBStatus deleteTransactionByAddress(const std::string &address, const uint32_t txNum);
    /**
     * @brief       Set block hash by transaction address
     * 
     * @param       address:
     * @param       txNum:
     * @param       blockHash:
     * @return      DBStatus
     */
    // TODO: Not used
    DBStatus setBlockHashByAddress(const std::string &address, const uint32_t txNum, const std::string &blockHash);
    /**
     * @brief       Remove the block hash in the database through the transaction address
     * 
     * @param       address:
     * @param       txNum:
     * @return      DBStatus
     */
    // todo: Not used
    DBStatus deleteBlockHashByAddress(const std::string &address, const uint32_t txNum);
    /**
     * @brief       Set the maximum transaction height through the transaction address
     * 
     * @param       address:
     * @param       txIndex:
     * @return      DBStatus
     */
    // todo: Not used
    DBStatus setTransactionTopByAddress(const std::string &address, const unsigned int txIndex);

    /**
     * @brief Set the balance for a specific address and asset type
     * 
     * @param address Address
     * @param assetType Asset type
     * @param balance Balance
     * @return DBStatus Operation result status code
     */
    DBStatus setBalanceByAddr(const std::string &address, const std::string& assetType, int64_t balance);

    /**
     * @brief Delete the balance for a specific address and asset type
     * 
     * @param address Address
     * @param assetType Asset type
     * @return DBStatus Operation result status code
     */
    DBStatus deleteBalanceByAddr(const std::string &address, const std::string &assetType);

    /**
     * @brief Set a stake address
     * 
     * @param address Stake address
     * @return DBStatus Operation result status code
     */
    DBStatus setStakeAddr(const std::string &address);

    /**
     * @brief Remove a stake address
     * 
     * @param address Stake address
     * @return DBStatus Operation result status code
     */
    DBStatus removeStakeAddr(const std::string &address);

    /**
     * @brief Set a bonus address
     * 
     * @param bonusAddr Bonus address
     * @return DBStatus Operation result status code
     */
    DBStatus setBonusAddr(const std::string &bonusAddr);

    /**
     * @brief Remove a bonus address
     * 
     * @param bonusAddr Bonus address
     * @return DBStatus Operation result status code
     */
    DBStatus removeBonusAddr(const std::string &bonusAddr);

    /**
     * @brief Set delegating address and asset type by bonus address
     * 
     * @param bonusAddr Bonus address
     * @param delegatingAddr Delegating address
     * @param assetType Asset type
     * @return DBStatus Operation result status code
     */
    DBStatus setDelegatingAddrByBonusAddr(const std::string &bonusAddr, const std::string& delegatingAddr, const std::string assetType);

    /**
     * @brief Remove delegating address and asset type by bonus address
     * 
     * @param bonusAddr Bonus address
     * @param delegatingAddr Delegating address
     * @param assetType Asset type
     * @return DBStatus Operation result status code
     */
    DBStatus removeDelegatingAddrByBonusAddr(const std::string &bonusAddr, const std::string& delegatingAddr, const std::string assetType);

    /**
     * @brief Set bonus address by delegating address
     * 
     * @param delegatingAddr Delegating address
     * @param bonusAddr Bonus address
     * @return DBStatus Operation result status code
     */
    DBStatus setBonusAddrByDelegatingAddr(const std::string &delegatingAddr, const std::string& bonusAddr);

    /**
     * @brief Set bonus address by delegating address and asset type
     * 
     * @param delegatingAddr Delegating address
     * @param assetType Asset type
     * @param bonusAddr Bonus address
     * @return DBStatus Operation result status code
     */
    DBStatus setBonusAddrAndAssetTypeByDelegatingAddr(const std::string &delegatingAddr, const std::string &assetType, const std::string& bonusAddr);

    /**
     * @brief Remove bonus address by delegating address and asset type
     * 
     * @param delegatingAddr Delegating address
     * @param assetType Asset type
     * @param bonusAddr Bonus address
     * @return DBStatus Operation result status code
     */
    DBStatus removeBonusAddrAndAssetTypeByDelegatingAddr(const std::string &delegatingAddr, const std::string &assetType, const std::string& bonusAddr);

    /**
     * @brief Set bonus address, delegating address, asset type and UTXO by bonus address
     * 
     * @param bonusAddr Bonus address
     * @param delegatingAddr Delegating address
     * @param assetType Asset type
     * @param utxo UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus setBonusAddrDelegatingAddrUtxoByBonusAddr(const std::string &bonusAddr, const std::string &delegatingAddr, const std::string assetType, const std::string &utxo);

    /**
     * @brief Remove bonus address, delegating address, asset type and UTXO by bonus address
     * 
     * @param bonusAddr Bonus address
     * @param delegatingAddr Delegating address
     * @param assetType Asset type
     * @param utxo UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus removeBonusAddrDelegatingAddrUtxoByBonusAddr(const std::string &bonusAddr, const std::string &delegatingAddr, const std::string assetType, const std::string &utxo);

    /**
     * @brief Set the total burn amount
     * 
     * @param totalBurn Total burn amount
     * @return DBStatus Operation result status code
     */
    DBStatus setTotalBurnAmount(uint64_t &totalBurn);

    /**
     * @brief Set the total delegating amount
     * 
     * @param delegatingCount Total delegating amount
     * @return DBStatus Operation result status code
     */
    DBStatus setTotalDelegatingAmount(uint64_t &delegatingCount);

    /**
     * @brief Set UTXO for a stake address
     * 
     * @param stakeAddr Stake address
     * @param assetType Asset type
     * @param utxo UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus setStakeAddrUtxo(const std::string &stakeAddr, const std::string& assetType, const std::string &utxo);

    /**
     * @brief Remove UTXO for a stake address
     * 
     * @param stakeAddr Stake address
     * @param assetType Asset type
     * @param utxo UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus removeStakeAddrUtxo(const std::string &stakeAddr, const std::string& assetType, const std::string &utxo);

    /**
     * @brief Set bonus UTXO by period
     * 
     * @param period Period
     * @param utxo UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus setBonusUtxoByPeriod(const uint64_t &period, const std::string &utxo);

    /**
     * @brief Set fund UTXO by period
     * 
     * @param period Period
     * @param utxo UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus setFundUtxoByPeriod(const uint64_t &period, const std::string &utxo);

    /**
     * @brief Remove bonus UTXO by period
     * 
     * @param period Period
     * @param utxo UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus removeBonusUtxoByPeriod(const uint64_t &period, const std::string &utxo);

    /**
     * @brief Remove fund UTXO by period
     * 
     * @param period Period
     * @param utxo UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus removeFundUtxoByPeriod(const uint64_t &period, const std::string &utxo);

    /**
     * @brief Set delegating UTXO by period
     * 
     * @param period Period
     * @param utxo UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus setDelegatingUtxoByPeriod(const uint64_t &period, const std::string &utxo);

    /**
     * @brief Remove delegating UTXO by period
     * 
     * @param period Period
     * @param utxo UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus removeDelegatingUtxoByPeriod(const uint64_t &period, const std::string &utxo);

    /**
     * @brief Set sign number by period
     * 
     * @param period Period
     * @param address Address
     * @param signNumber Sign number
     * @return DBStatus Operation result status code
     */
    DBStatus setSignNumberByPeriod(const uint64_t &period, const std::string &address, const uint64_t &signNumber);

    /**
     * @brief Remove sign number by period
     * 
     * @param period Period
     * @param address Address
     * @return DBStatus Operation result status code
     */
    DBStatus removeSignNumberByPeriod(const uint64_t &period, const std::string &address);

    /**
     * @brief Set block number by period
     * 
     * @param period Period
     * @param blockNumber Block number
     * @return DBStatus Operation result status code
     */
    DBStatus setBlockNumberByPeriod(const uint64_t &period, const uint64_t &blockNumber);

    /**
     * @brief Remove block number by period
     * 
     * @param period Period
     * @return DBStatus Operation result status code
     */
    DBStatus removeBlockNumberByPeriod(const uint64_t &period);

    /**
     * @brief Set sign address by period
     * 
     * @param period Period
     * @param addr Sign address
     * @return DBStatus Operation result status code
     */
    DBStatus setSignAddrByPeriod(const uint64_t &period, const std::string &addr);

    /**
     * @brief Remove sign address by period
     * 
     * @param period Period
     * @param addr Sign address
     * @return DBStatus Operation result status code
     */
    DBStatus removeSignAddrByPeriod(const uint64_t &period, const std::string &addr);

    /**
     * @brief Set burn amount by period
     * 
     * @param period Period
     * @param burnAmount Burn amount
     * @return DBStatus Operation result status code
     */
    DBStatus setBurnAmountByPeriod(const uint64_t &period, const uint64_t &burnAmount);

    /**
     * @brief Remove burn amount by period
     * 
     * @param period Period
     * @param burnAmount Burn amount
     * @return DBStatus Operation result status code
     */
    DBStatus removeBurnAmountByPeriod(const uint64_t &period, const uint64_t &burnAmount);

    /**
     * @brief Set EVM contract deployer address
     * 
     * @param deployerAddr Deployer address
     * @return DBStatus Operation result status code
     */
    DBStatus setEvmDeployerAddr(const std::string &deployerAddr);

    /**
     * @brief Remove EVM contract deployer address
     * 
     * @param deployerAddr Deployer address
     * @return DBStatus Operation result status code
     */
    DBStatus removeEvmDeployerAddr(const std::string &deployerAddr);

    /**
     * @brief Set contract address by deployer address
     * 
     * @param deployerAddr Deployer address
     * @param contractAddr Contract address
     * @return DBStatus Operation result status code
     */
    DBStatus setContractAddrByDeployerAddr(const std::string &deployerAddr, const std::string &contractAddr);

    /**
     * @brief Remove contract address by deployer address
     * 
     * @param deployerAddr Deployer address
     * @param contractAddr Contract address
     * @return DBStatus Operation result status code
     */
    DBStatus removeContractAddrByDeployerAddr(const std::string &deployerAddr, const std::string &contractAddr);

    /**
     * @brief Set contract code by contract address
     * 
     * @param contractAddr Contract address
     * @param contractCode Contract code
     * @return DBStatus Operation result status code
     */
    DBStatus setContractCodeByContractAddr(const std::string &contractAddr, const std::string &contractCode);

    /**
     * @brief Remove contract code by contract address
     * 
     * @param contractAddr Contract address
     * @return DBStatus Operation result status code
     */
    DBStatus removeContractCodeByContractAddr(const std::string &contractAddr);

    /**
     * @brief Set contract deployment UTXO by contract address
     * 
     * @param contractAddr Contract address
     * @param contractDeploymentUtxo Contract deployment UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus setContractDeployUtxoByContractAddr(const std::string &contractAddr, const std::string &contractDeploymentUtxo);

    /**
     * @brief Remove contract deployment UTXO by contract address
     * 
     * @param contractAddr Contract address
     * @return DBStatus Operation result status code
     */
    DBStatus removeContractDeployUtxoByContractAddr(const std::string &contractAddr);

    /**
     * @brief Set latest UTXO by contract address
     * 
     * @param contractAddr Contract address
     * @param utxo Latest UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus setLatestUtxoByContractAddr(const std::string &contractAddr, const std::string &utxo);

    /**
     * @brief Remove latest UTXO by contract address
     * 
     * @param contractAddr Contract address
     * @return DBStatus Operation result status code
     */
    DBStatus removeLatestUtxoByContractAddr(const std::string &contractAddr);

    /**
     * @brief Set MPT value by MPT key
     * 
     * @param mptKey MPT key
     * @param mptValue MPT value
     * @return DBStatus Operation result status code
     */
    DBStatus setMptValueByMptKey(const std::string &mptKey, const std::string &mptValue);

    /**
     * @brief Remove MPT value by MPT key
     * 
     * @param mptKey MPT key
     * @return DBStatus Operation result status code
     */
    DBStatus removeMptValueByMptKey(const std::string &mptKey);


    /**
     * @brief Set asset type
     * 
     * @param asserType Asset type
     * @return DBStatus Operation result status code
     */
    DBStatus setAssetType(const std::string &asserType);

    /**
     * @brief Remove asset type
     * 
     * @param asserType Asset type
     * @return DBStatus Operation result status code
     */
    DBStatus removeAssetType(const std::string &asserType);

    /**
     * @brief Set revoke transaction hash by asset type
     * 
     * @param asserType Asset type
     * @param revokeTransactionHashValue Revoke transaction hash value
     * @return DBStatus Operation result status code
     */
    DBStatus setRevokeTxHashByAssetType(const std::string &asserType, const std::string revokeTransactionHashValue);

    /**
     * @brief Remove revoke transaction hash by asset type
     * 
     * @param asserType Asset type
     * @param revokeTransactionHashValue Revoke transaction hash value
     * @return DBStatus Operation result status code
     */
    DBStatus removeRevokeTxHashByAssetType(const std::string &asserType, const std::string revokeTransactionHashValue);

    /**
     * @brief Set approve vote address by asset hash
     * 
     * @param asserType Asset type
     * @param addr Address
     * @return DBStatus Operation result status code
     */
    DBStatus setApproveVoteByAssetHash(const std::string &asserType, const std::string &addr);

    /**
     * @brief Remove approve vote address by asset hash
     * 
     * @param asserType Asset type
     * @param addr Address
     * @return DBStatus Operation result status code
     */
    DBStatus removeApproveVoteByAssetHash(const std::string &asserType, const std::string &addr);

    /**
     * @brief Set against vote address by asset hash
     * 
     * @param asserType Asset type
     * @param addr Address
     * @return DBStatus Operation result status code
     */
    DBStatus setAgainstVoteByAssetHash(const std::string &asserType, const std::string &addr);

    /**
     * @brief Remove against vote address by asset hash
     * 
     * @param asserType Asset type
     * @param addr Address
     * @return DBStatus Operation result status code
     */
    DBStatus removeAgainstVoteByAssetHash(const std::string &asserType, const std::string &addr);

    /**
     * @brief Set vote transaction hash by asset hash
     * 
     * @param asserType Asset type
     * @param voteTxHash Vote transaction hash
     * @return DBStatus Operation result status code
     */
    DBStatus setVoteTxHashByAssetHash(const std::string &asserType, const std::string &voteTxHash);

    /**
     * @brief Remove vote transaction hash by asset hash
     * 
     * @param asserType Asset type
     * @param voteTxHash Vote transaction hash
     * @return DBStatus Operation result status code
     */
    DBStatus removeVoteTxHashByAssetHash(const std::string &asserType, const std::string &voteTxHash);

    /**
     * @brief Set asset info by asset type
     * 
     * @param asserType Asset type
     * @param info Asset info
     * @return DBStatus Operation result status code
     */
    DBStatus setAssetInfobyAssetType(const std::string &asserType, const std::string info);

    /**
     * @brief Remove asset info by asset type
     * 
     * @param asserType Asset type
     * @return DBStatus Operation result status code
     */
    DBStatus removeAssetInfobyAssetType(const std::string &asserType);

    /**
     * @brief Set revoke proposal info by transaction hash
     * 
     * @param TxHash Transaction hash
     * @param info Revoke proposal info
     * @return DBStatus Operation result status code
     */
    DBStatus setRevokeProposalInfobyTxHash(const std::string &TxHash, const std::string info);

    /**
     * @brief Remove revoke proposal info by transaction hash
     * 
     * @param TxHash Transaction hash
     * @return DBStatus Operation result status code
     */
    DBStatus removeRevokeProposalInfobyTxHash(const std::string &TxHash);

    /**
     * @brief Set vote number by asset hash
     * 
     * @param asserType Asset type
     * @param info Vote number info
     * @return DBStatus Operation result status code
     */
    DBStatus setVoteNumByAssetHash(const std::string &asserType, const std::string &info);

    /**
     * @brief Delete vote number by asset hash
     * 
     * @param asserType Asset type
     * @return DBStatus Operation result status code
     */
    DBStatus deleteVoteNumByAssetHash(const std::string &asserType);

    /**
     * @brief Set vote number by address and asset type
     * 
     * @param addr Address
     * @param asserType Asset type
     * @param voteNum Vote number
     * @return DBStatus Operation result status code
     */
    DBStatus setVoteNumByAddr(const std::string &addr, const std::string& asserType, const uint64_t& voteNum);

    /**
     * @brief Select vote number by address and asset type
     * 
     * @param addr Address
     * @param asserType Asset type
     * @return DBStatus Operation result status code
     */
    DBStatus seleteVoteNumByAddr(const std::string &addr, const std::string& asserType);

    /**
     * @brief Set total number of voters by asset hash
     * 
     * @param asserType Asset type
     * @param voteNum Number of voters
     * @return DBStatus Operation result status code
     */
    DBStatus setTotalNumberOfVotersByAssetHash(const std::string& asserType, const uint64_t& voteNum);

    /**
     * @brief Delete total number of voters by asset hash
     * 
     * @param asserType Asset type
     * @return DBStatus Operation result status code
     */
    DBStatus deleteTotalNumberOfVotersByAssetHash(const std::string& asserType);

    /**
     * @brief Set lock address
     * 
     * @param address Lock address
     * @return DBStatus Operation result status code
     */
    DBStatus setLockAddr(const std::string &address);

    /**
     * @brief Remove lock address
     * 
     * @param address Lock address
     * @return DBStatus Operation result status code
     */
    DBStatus removeLockAddr(const std::string &address);

    /**
     * @brief Set UTXO for lock address
     * 
     * @param LockAddr Lock address
     * @param assetType Asset type
     * @param utxo UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus setLockAddrUtxo(const std::string &LockAddr, const std::string& assetType ,const std::string &utxo);

    /**
     * @brief Remove UTXO for lock address
     * 
     * @param LockAddr Lock address
     * @param assetType Asset type
     * @param utxo UTXO
     * @return DBStatus Operation result status code
     */
    DBStatus removeLockAddrUtxo(const std::string &LockAddr, const std::string& assetType , const std::string &utxo);

    /**
     * @brief Set total locked amount
     * 
     * @param TotalLockedAmount Total locked amount
     * @return DBStatus Operation result status code
     */
    DBStatus setTotalLockedAmonut(const uint64_t& TotalLockedAmount);

    /**
     * @brief Set asset type by address
     * 
     * @param addr Address
     * @param asserType Asset type
     * @return DBStatus Operation result status code
     */
    DBStatus setAssetTypeByAddr(const std::string& addr, const std::string &asserType);

    /**
     * @brief Remove asset type by address
     * 
     * @param addr Address
     * @param asserType Asset type
     * @return DBStatus Operation result status code
     */
    DBStatus removeAssetTypeByAddr(const std::string& addr, const std::string &asserType);

    /**
     * @brief Set asset type by contract address
     * 
     * @param contractAddr Contract address
     * @param asserType Asset type
     * @return DBStatus Operation result status code
     */
    DBStatus setAssetTypeByContractAddr(const std::string& contractAddr, const std::string &asserType);

    /**
     * @brief Remove asset type by contract address
     * 
     * @param contractAddr Contract address
     * @return DBStatus Operation result status code
     */
    DBStatus removeAssetTypeByContractAddr(const std::string& contractAddr);

    /**
     * @brief Set gas amount by period
     * 
     * @param period Period
     * @param type Type
     * @param gasAmount Gas amount
     * @return DBStatus Operation result status code
     */
    DBStatus setGasAmountByPeriod(const uint64_t &period, const std::string &type,const uint64_t &gasAmount);

    /**
     * @brief Set package count by period and asset type
     * 
     * @param period Period
     * @param assetType Asset type
     * @param count Package count
     * @return DBStatus Operation result status code
     */
    DBStatus setPackageCountByPeriod(const uint64_t& period, const std::string& assetType, const uint64_t& count);

    /**
     * @brief Set bonus exchequer by transaction hash and period
     * 
     * @param txHash Transaction hash
     * @param time Period
     * @param bonusExchequer Bonus exchequer amount
     * @return DBStatus Operation result status code
     */
    DBStatus setBonusExchequerByTxHashAndPeriod(const std::string &txHash, const uint64_t &time, const uint64_t &bonusExchequer);

    /**
     * @brief Remove bonus exchequer by transaction hash and period
     * 
     * @param txHash Transaction hash
     * @param time Period
     * @param bonusExchequer Bonus exchequer amount
     * @return DBStatus Operation result status code
     */
    DBStatus removeBonusExchequerByTxHashAndPeriod(const std::string &txHash, const uint64_t &time, const uint64_t &bonusExchequer);

    /**
     * @brief Set packager times by period and address
     * 
     * @param period Period
     * @param address Address
     * @param times Packager times
     * @return DBStatus Operation result status code
     */
    DBStatus setPackagerTimesByPeriod(const uint64_t& period, const std::string& address, const uint64_t& times);

    /**
     * @brief Set initial version
     * 
     * @param version Version
     * @return DBStatus Operation result status code
     */
    DBStatus setInitVer(const std::string &version);


private:

    /**
     * @brief Transaction rollback
     * 
     * @return DBStatus Operation result status code
     */
    DBStatus transactionRollBack();

    /**
     * @brief Batch read data
     * 
     * @param keys List of keys to read
     * @param values List to store the read values
     * @return DBStatus Operation result status code
     */
    virtual DBStatus multiReadData(const std::vector<std::string> &keys, std::vector<std::string> &values);

    /**
     * @brief Read single data
     * 
     * @param key Key to read
     * @param value Value to store the read result
     * @return DBStatus Operation result status code
     */
    virtual DBStatus readData(const std::string &key, std::string &value);

    /**
     * @brief Merge and write data
     * 
     * @param key Key to merge
     * @param value Value to merge
     * @param firstOrLast Whether to merge at the beginning or end, default is false
     * @return DBStatus Operation result status code
     */
    DBStatus mergeValue(const std::string &key, const std::string &value, bool firstOrLast = false);

    /**
     * @brief Remove merged value
     * 
     * @param key Key to remove
     * @param value Value to remove
     * @return DBStatus Operation result status code
     */
    DBStatus removeMergeValue(const std::string &key, const std::string &value);

    /**
     * @brief Write data
     * 
     * @param key Key to write
     * @param value Value to write
     * @return DBStatus Operation result status code
     */
    DBStatus writeData(const std::string &key, const std::string &value);

    /**
     * @brief Delete data
     * 
     * @param key Key to delete
     * @return DBStatus Operation result status code
     */
    DBStatus deleteData(const std::string &key);

    
    std::set<std::string> delete_keys_;
    RocksDBReadWriter dbReaderWriter;
    bool autoOperationTrans;
};

#endif