#ifndef ROCKSDB_HEADER_INCLUDED
#define ROCKSDB_HEADER_INCLUDED

#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/status.h"
#include "rocksdb/utilities/transaction.h"
#include "rocksdb/utilities/transaction_db.h"
#include <mutex>

class BackgroundErrorListener : public rocksdb::EventListener
{
public:
    // Callback function when a background error occurs
    void OnBackgroundError(rocksdb::BackgroundErrorReason reason, rocksdb::Status* errorStatus) override;
};

class RocksDBDataReader;
class RocksDBReadWriter;
class RocksDB
{
public:
    RocksDB();
    ~RocksDB();
    RocksDB(RocksDB &&) = delete;
    RocksDB(const RocksDB &) = delete;
    RocksDB &operator=(RocksDB &&) = delete;
    RocksDB &operator=(const RocksDB &) = delete;

    /**
     * Set the database path
     * @param dbPath The file path of the database
     */
    void setDBPath(const std::string &dbPath);

    /**
     * Initialize the database
     * @param retStatus Returned status information
     * @return Whether initialization is successful
     */
    bool initDB(rocksdb::Status &retStatus);

    /**
     * Destroy the database
     */
    void sestoryDB();

    /**
     * Check if the database was initialized successfully
     * @return Whether initialization was successful
     */
    bool isInitSuccess();


    /**
     * Get database memory usage information
     * @param info String to store memory usage information
     */
    void getDBMemoryUsage(std::string& info);

private:
    friend class BackgroundErrorListener;
    friend class RocksDBDataReader;
    friend class RocksDBReadWriter;
    std::string db_path_;
    rocksdb::TransactionDB *db_;
    std::mutex initSuccessMutex;
    bool isInitialized;
};



#endif
