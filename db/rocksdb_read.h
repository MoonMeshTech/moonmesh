#ifndef ROCKSDB_READ_HEADER
#define ROCKSDB_READ_HEADER

#include "db/rocksdb.h"
#include <memory>
#include <string>
#include <vector>

class RocksDBDataReader
{
public:
    RocksDBDataReader(std::shared_ptr<RocksDB> rocksdb);
    ~RocksDBDataReader() = default;
    RocksDBDataReader(RocksDBDataReader &&) = delete;
    RocksDBDataReader(const RocksDBDataReader &) = delete;
    RocksDBDataReader &operator=(RocksDBDataReader &&) = delete;
    RocksDBDataReader &operator=(const RocksDBDataReader &) = delete;

    /**
     * @brief Batch read multiple key-value pairs from the database
     * 
     * @param keys The collection of keys to read
     * @param values The collection to store the read values
     * @param retStatus The collection to store the status of each key read
     * @return Whether the read was successful
     */
    bool multiReadData(const std::vector<rocksdb::Slice> &keys, std::vector<std::string> &values, std::vector<rocksdb::Status> &retStatus);

    /**
     * @brief Read a single key-value pair from the database
     * 
     * @param key The key to read
     * @param value The variable to store the read value
     * @param retStatus The variable to store the status of the read operation
     * @return Whether the read was successful
     */
    bool readData(const std::string &key, std::string &value, rocksdb::Status &retStatus);

private:
    rocksdb::ReadOptions read_options_;
    std::shared_ptr<RocksDB> rocksdb_;
};

#endif
