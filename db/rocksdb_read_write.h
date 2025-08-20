#ifndef ROCKSDB_READ_WRITE_H_INCLUDED
#define ROCKSDB_READ_WRITE_H_INCLUDED

#include "db/rocksdb.h"
#include <memory>
#include <string>
#include <vector>

class RocksDBReadWriter
{
public:
    RocksDBReadWriter(std::shared_ptr<RocksDB> db, const std::string &txn_name);
    ~RocksDBReadWriter();
    RocksDBReadWriter(RocksDBReadWriter &&) = delete;
    RocksDBReadWriter(const RocksDBReadWriter &) = delete;
    RocksDBReadWriter &operator=(RocksDBReadWriter &&) = delete;
    RocksDBReadWriter &operator=(const RocksDBReadWriter &) = delete;

    /**
     * @brief Initialize the transaction
     * 
     * @return Whether initialization is successful
     */
    bool transactionInit();


    /**
     * @brief Commit the transaction
     * 
     * @param retStatus Used to store the status of the commit operation
     * @return Whether the commit was successful
     */
    bool transactionCommit(rocksdb::Status &retStatus);

    /**
     * @brief Roll back the transaction
     * 
     * @param retStatus Used to store the status of the rollback operation
     * @return Whether the rollback was successful
     */
    bool transactionRollBack(rocksdb::Status &retStatus);


    /**
     * @brief Batch read multiple key-value pairs
     * 
     * @param keys The collection of keys to read
     * @param values The collection to store the read values
     * @param retStatus The collection to store the status of each key read
     * @return Whether the read was successful
     */
    bool multiReadData(const std::vector<rocksdb::Slice> &keys, std::vector<std::string> &values, std::vector<rocksdb::Status> &retStatus);

    /**
     * @brief Merge value into the key
     * 
     * @param key The key to merge
     * @param value The value to merge
     * @param retStatus Used to store the status of the merge operation
     * @param firstOrLast Whether to merge at the beginning or end, default is false
     * @return Whether the merge was successful
     */
    bool mergeValue(const std::string &key, const std::string &value, rocksdb::Status &retStatus, bool firstOrLast = false);

    /**
     * @brief Remove merged value from the key
     * 
     * @param key The key to remove
     * @param value The value to remove
     * @param retStatus Used to store the status of the remove operation
     * @return Whether the removal was successful
     */
    bool removeMergeValue(const std::string &key, const std::string &value, rocksdb::Status &retStatus);

    /**
     * @brief Read a single key-value pair
     * 
     * @param key The key to read
     * @param value The variable to store the read value
     * @param retStatus Used to store the status of the read operation
     * @return Whether the read was successful
     */
    bool readData(const std::string &key, std::string &value, rocksdb::Status &retStatus);

    /**
     * @brief Write a single key-value pair
     * 
     * @param key The key to write
     * @param value The value to write
     * @param retStatus Used to store the status of the write operation
     * @return Whether the write was successful
     */
    bool writeData(const std::string &key, const std::string &value, rocksdb::Status &retStatus);

    /**
     * @brief Delete a single key-value pair
     * 
     * @param key The key to delete
     * @param retStatus Used to store the status of the delete operation
     * @return Whether the deletion was successful
     */
    bool deleteData(const std::string &key, rocksdb::Status &retStatus);

private:

    /**
     * @brief Read a single key-value pair and lock it for update
     * 
     * @param key The key to read and lock for update
     * @param value The variable to store the read value
     * @param retStatus Used to store the status of the read operation
     * @return Whether the read and lock for update was successful
     */
    bool readForUpdate(const std::string &key, std::string &value, rocksdb::Status &retStatus);

    std::string txn_name_;
    std::shared_ptr<RocksDB> rocksdb_;
    rocksdb::Transaction *txn_;
    rocksdb::WriteOptions write_options_;
    rocksdb::ReadOptions read_options_;
};
#endif
