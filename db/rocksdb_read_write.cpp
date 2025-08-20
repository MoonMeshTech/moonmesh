#include "db/rocksdb_read_write.h"
#include "include/logging.h"
#include "utils/string_util.h"
#include "db/db_api.h"

RocksDBReadWriter::RocksDBReadWriter(std::shared_ptr<RocksDB> rocksdb, const std::string &txn_name)
{
    txn_ = nullptr;
    rocksdb_ = rocksdb;
    txn_name_ = txn_name;
}

RocksDBReadWriter::~RocksDBReadWriter()
{

}
bool RocksDBReadWriter::transactionInit()
{
    txn_ = rocksdb_->db_->BeginTransaction(write_options_);
    if (nullptr == txn_)
    {
        return false;
    }
    return true;
}

bool RocksDBReadWriter::transactionCommit(rocksdb::Status &retStatus)
{
    if (!rocksdb_->isInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    retStatus = txn_->Commit();
    if (!retStatus.ok())
    {
        ERRORLOG("{} transction commit failed code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
        return false;
    }
    delete txn_;
    txn_ = nullptr;
    return true;
}

bool RocksDBReadWriter::transactionRollBack(rocksdb::Status &retStatus)
{
    if (!rocksdb_->isInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    auto status = txn_->Rollback();
    if (!status.ok())
    {
        ERRORLOG("{} transction rollback failed code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, status.code(), status.subcode(), status.severity(), status.ToString());
        return false;
    }
    delete txn_;
    txn_ = nullptr;
    return true;
}

bool RocksDBReadWriter::multiReadData(const std::vector<rocksdb::Slice> &keys, std::vector<std::string> &values, std::vector<rocksdb::Status> &retStatus)
{
    if (!rocksdb_->isInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus.push_back(rocksdb::Status::Aborted());
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus.push_back(rocksdb::Status::Aborted());
        return false;
    }
    if (keys.empty())
    {
        ERRORLOG("key is empty");
        retStatus.push_back(rocksdb::Status::Aborted());
        return false;
    }
    retStatus.clear();
    {
        retStatus = txn_->MultiGet(read_options_, keys, &values);
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
                TRACELOG("{} rocksdb readData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                         txn_name_, key, status.code(), status.subcode(), status.severity(), status.ToString());
            }
            else
            {
                ERRORLOG("{} rocksdb readData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                         txn_name_, key, status.code(), status.subcode(), status.severity(), status.ToString());
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

bool RocksDBReadWriter::mergeValue(const std::string &key, const std::string &value, rocksdb::Status &retStatus, bool firstOrLast)
{
    if (!rocksdb_->isInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (value.empty())
    {
        ERRORLOG("value is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    std::string ret_value;
    if (readForUpdate(key, ret_value, retStatus))
    {
        std::vector<std::string> split_values;
        StringUtil::SplitString(ret_value, "_", split_values);
        auto it = std::find(split_values.begin(), split_values.end(), value);
        if(split_values.end() == it)
        {
            if(firstOrLast)
            {
                std::vector<std::string> tmp_values;
                tmp_values.push_back(value);
                tmp_values.insert(tmp_values.end(), split_values.begin(), split_values.end());
                split_values.swap(tmp_values);
            }
            else
            {
                split_values.push_back(value);
            }
        }
        ret_value.clear();
        for(auto split_value : split_values)
        {
            if(split_value.empty())
            {
                continue;
            }
            ret_value += split_value;
            ret_value += "_";
        }
        return writeData(key, ret_value, retStatus);
    }
    else
    {
        if (retStatus.IsNotFound())
        {
            return writeData(key, value, retStatus);
        }
    }
    return false;
}

bool RocksDBReadWriter::removeMergeValue(const std::string &key, const std::string &value, rocksdb::Status &retStatus)
{
    if (!rocksdb_->isInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (value.empty())
    {
        ERRORLOG("value is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    std::string ret_value;
    if (readForUpdate(key, ret_value, retStatus))
    {
        std::vector<std::string> split_values;
        StringUtil::SplitString(ret_value, "_", split_values);
        auto it = std::find(split_values.begin(), split_values.end(), value);
        while(split_values.end() != it)
        {
            split_values.erase(it);
            it = std::find(split_values.begin(), split_values.end(), value);
        }
        ret_value.clear();
        for(auto split_value : split_values)
        {
            if(split_value.empty())
            {
                continue;
            }
            ret_value += split_value;
            ret_value += "_";
        }
        if(ret_value.empty())
        {
            return deleteData(key, retStatus);
        }
        return writeData(key, ret_value, retStatus);
    }
    else
    {
        if(retStatus.IsNotFound())
        {
            return true;
        }
    }
    return false;
}
bool RocksDBReadWriter::readData(const std::string &key, std::string &value, rocksdb::Status &retStatus)
{
    if (!rocksdb_->isInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
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
        retStatus = txn_->Get(read_options_, key, &value);
    }
    if (retStatus.ok())
    {
        return true;
    }
    if (retStatus.IsNotFound())
    {
        TRACELOG("{} rocksdb readData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    else
    {
        ERRORLOG("{} rocksdb readData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    if(retStatus.code() == rocksdb::Status::Code::kIOError)
    {
        destroyDatabase();
        exit(-1);
    }
    return false;
}

bool RocksDBReadWriter::writeData(const std::string &key, const std::string &value, rocksdb::Status &retStatus)
{
    if (!rocksdb_->isInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (key.empty())
    {
        ERRORLOG("key is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (value.empty())
    {
        ERRORLOG("value is empty");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    {
        retStatus = txn_->Put(key, value);
    }
    if (retStatus.ok())
    {
        return true;
    }
    if (retStatus.IsNotFound())
    {
        TRACELOG("{} rocksdb writeData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    else
    {
        ERRORLOG("{} rocksdb writeData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    return false;
}
bool RocksDBReadWriter::deleteData(const std::string &key, rocksdb::Status &retStatus)
{
    if (!rocksdb_->isInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
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
        retStatus = txn_->Delete(key);
    }
    if (retStatus.ok())
    {
        return true;
    }
    if (retStatus.IsNotFound())
    {
        TRACELOG("{} rocksdb deleteData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    else
    {
        ERRORLOG("{} rocksdb deleteData failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    return false;
}
bool RocksDBReadWriter::readForUpdate(const std::string &key, std::string &value, rocksdb::Status &retStatus)
{
    if (!rocksdb_->isInitSuccess())
    {
        ERRORLOG("rocksdb not init");
        retStatus = rocksdb::Status::Aborted();
        return false;
    }
    if (nullptr == txn_)
    {
        ERRORLOG("transaction is null");
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
        retStatus = txn_->GetForUpdate(read_options_, key, &value);
    }
    if (retStatus.ok())
    {
        return true;
    }
    if (retStatus.IsNotFound())
    {
        TRACELOG("{} rocksdb readForUpdate failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    else
    {
        ERRORLOG("{} rocksdb readForUpdate failed key:{} code:({}),subcode:({}),severity:({}),info:({})",
                 txn_name_, key, retStatus.code(), retStatus.subcode(), retStatus.severity(), retStatus.ToString());
    }
    if(retStatus.code() == rocksdb::Status::Code::kIOError)
    {
        destroyDatabase();
        exit(-1);
    }
    return false;
}
