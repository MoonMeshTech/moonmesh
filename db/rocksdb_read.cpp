#include "db/rocksdb_read.h"
#include "include/logging.h"
#include "utils/string_util.h"
#include "db/db_api.h"
#include "rocksdb/db.h"

RocksDBDataReader::RocksDBDataReader(std::shared_ptr<RocksDB> rocksdb)
{
    rocksdb_ = rocksdb;
}

bool RocksDBDataReader::multiReadData(const std::vector<rocksdb::Slice> &keys, std::vector<std::string> &values, std::vector<rocksdb::Status> &retStatus)
{
    retStatus.clear();
    if (!rocksdb_->isInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus.push_back(rocksdb::Status::Aborted());
        return false;
    }
    if (keys.empty())
    {
        ERRORLOG("key is empty");
        retStatus.push_back(rocksdb::Status::Aborted());
        return false;
    }
    {
        retStatus = rocksdb_->db_->MultiGet(read_options_, keys, &values);
    }
    bool flag = true;
    for(size_t i = 0; i < retStatus.size(); ++i)
    {
        auto status = retStatus.at(i);
        if (!status.ok())
        {
            flag = false;
            std::string key;
            if(keys.size() > i)
            {
                key = keys.at(i).data();
            }
            if (status.IsNotFound())
            {
                TRACELOG("rocksdb readData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                         key, status.code(), status.subcode(), status.severity(), status.ToString());
            }
            else
            {
                ERRORLOG("rocksdb readData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                         key, status.code(), status.subcode(), status.severity(), status.ToString());
                if(status.code() == rocksdb::Status::Code::kIOError)
                {
                    destroyDatabase();
                    exit(-1);
                }
            }
        }
    }
    return flag;
}

bool RocksDBDataReader::readData(const std::string &key, std::string &value, rocksdb::Status &retStatus)
{
    if (!rocksdb_->isInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    {
        retStatus = rocksdb_->db_->Get(read_options_, key, &value);
    }
    if (retStatus.ok())
    {
        return true;
    }
    if (retStatus.IsNotFound())
    {
        TRACELOG("rocksdb readData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    else
    {
        ERRORLOG("rocksdb readData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    if(retStatus.code() == rocksdb::Status::Code::kIOError)
    {
        destroyDatabase();
        exit(-1);
    }
    return false;
}
