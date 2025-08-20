#include <sstream>
#include <random>

#include "ca/global.h"
#include "ca/block_http_callback.h"

#include "include/logging.h"
#include "include/scope_guard.h"

#include "db/db_api.h"
#include "net/httplib.h"
#include "common/config.h"
#include "utils/magic_singleton.h"
#include "test.h"
#include "ca.h"


BlockHttpCallbackHandler::BlockHttpCallbackHandler() : _running(false),_ip("localhost"),_port(11190),_path("/Browser/block")
{
    Config::HttpCallback httpCallback = {};
    MagicSingleton<Config>::GetInstance()->GetHttpCallback(httpCallback);
    if (!httpCallback.ip.empty() && httpCallback.port > 0)
    {
        this->Start(httpCallback.ip, httpCallback.port, httpCallback.path);
    }
    else
    {
        ERRORLOG("Http callback is not config!");
    }
}

bool BlockHttpCallbackHandler::AddBlock(const std::string& block)
{
    if (block.empty())
        return false;
    std::unique_lock<std::mutex> lck(_addMutex);
    _addblocks.push_back( block );
    _cvadd.notify_all();

    return true;
}

bool BlockHttpCallbackHandler::AddBlock(const CBlock& block)
{
    std::string json = ToJson(block);
    return AddBlock(json);
}

bool BlockHttpCallbackHandler::rollback_block_str(const std::string& block)
{
      if (block.empty())
        return false;
        
    std::unique_lock<std::mutex> lck(rollbackMutex);
    rollbackBlocks_.push_back( block );
    cvRollback.notify_all();

    return true;
}


bool BlockHttpCallbackHandler::RollbackBlock(const CBlock& block)
{
    std::string json = ToJson(block);
    return rollback_block_str(json);
}


void BlockHttpCallbackHandler::addBlockWork(const std::string &method)
{
    while (_running)
    {
        std::string currentBlock;
        {
            std::unique_lock<std::mutex> lck(_addMutex);
            while (_addblocks.empty())
            {
                DEBUGLOG("Enter waiting for condition variable.");
                _cvadd.wait(lck);
            }
            DEBUGLOG("Handle the first block...");
            currentBlock = _addblocks.front();
            _addblocks.erase(_addblocks.begin());
        }

        sendBlockHttpRequest(currentBlock,method);
    }
}


void BlockHttpCallbackHandler::rollbackBlockWork_(const std::string &method)
{
    while (_running)
    {
        std::string currentBlock;
        {
            std::unique_lock<std::mutex> lck(rollbackMutex);
            while (rollbackBlocks_.empty())
            {
                DEBUGLOG("Enter waiting for condition variable.");
                cvRollback.wait(lck);
            }
            DEBUGLOG("Handle the first block...");
            currentBlock = rollbackBlocks_.front();
            rollbackBlocks_.erase(rollbackBlocks_.begin());
        }
        sendBlockHttpRequest(currentBlock,method);
    }
}

void BlockHttpCallbackHandler::Start(const std::string& ip, int port,const std::string& path)
{
    _ip = ip;
    _port = port;
    _path = path;
    _running = true;
    const std::string method1 = "/addblock";
    const std::string method2 = "/rollbackblock";
    workAddBlockThread = std::thread(std::bind(&BlockHttpCallbackHandler::addBlockWork, this, method1));
    workRollbackThread = std::thread(std::bind(&BlockHttpCallbackHandler::rollbackBlockWork_, this, method2));
    workAddBlockThread.detach();
    workRollbackThread.detach();
}

void BlockHttpCallbackHandler::Stop()
{
    _running = false;
}

bool BlockHttpCallbackHandler::IsRunning()
{
    return _running;
}

bool BlockHttpCallbackHandler::sendBlockHttpRequest(const std::string& block,const std::string &method)
{
    httplib::Client client(_ip, _port);
    std::string path = _path + method;
    auto res = client.Post(path.data(), block, "application/json");
    if (res)
    {
        DEBUGLOG("status:{}, Content-Type:{}, body:{}", res->status, res->get_header_value("Content-Type"), res->body);
    }
    else
    {
        DEBUGLOG("Client post failed");
    }

    return (bool)res;
}

std::string BlockHttpCallbackHandler::ToJson(const CBlock& block)
{
    nlohmann::json allTx;
    nlohmann::json jsonBlock;
    jsonBlock["hash"] = addHexPrefix(block.hash());
    jsonBlock["height"] = block.height();
    jsonBlock["time"] = block.time();
    if(!block.data().empty())
    {
        nlohmann::json blockdataJson = nlohmann::json::parse(block.data());
        nlohmann::json modifiedJsonData;

        for (auto it = blockdataJson.begin(); it != blockdataJson.end(); ++it) {
            std::string originalKey = it.key();
            auto value = it.value();

            std::string modifiedKey = addHexPrefix(originalKey);

            modifiedJsonData[modifiedKey] = value;
        }
        blockdataJson = modifiedJsonData;

        if(blockdataJson.contains("dependentCTx") && !blockdataJson["dependentCTx"].get<std::string>().empty()){
            blockdataJson["dependentCTx"] = addHexPrefix(blockdataJson["dependentCTx"].get<std::string>());
        }
        else
        {
            blockdataJson["dependentCTx"] = "";
        }
        jsonBlock["blockdata"] = blockdataJson;
        
    }
    else
    {
        jsonBlock["blockdata"] = "";
    }
    int k = 0;
    for(auto & tx : block.txs())
    {
        nlohmann::json Tx;
        if(tx.type() == global::ca::kTxSign)
        {   
            if((global::ca::TxType)tx.txtype() != global::ca::TxType::TX_TYPE_TX)
            {
                
                nlohmann::json dataJson = nlohmann::json::parse(tx.data());
                if (dataJson.contains("TxInfo") && dataJson["TxInfo"].contains("BonusAddr")) 
                {
                    std::string bonusAddr = dataJson["TxInfo"]["BonusAddr"].get<std::string>();
                    dataJson["TxInfo"]["BonusAddr"] = addHexPrefix(bonusAddr);
                }
                
                if (dataJson.contains("TxInfo") && dataJson["TxInfo"].contains("UndelegatingUtxo")) 
                {
                    std::string UndelegatingUtxo = dataJson["TxInfo"]["UndelegatingUtxo"].get<std::string>();
                    dataJson["TxInfo"]["UndelegatingUtxo"] = addHexPrefix(UndelegatingUtxo);
                }
                Tx["data"] = dataJson;
                
            }
            
            Tx["time"] = tx.time();
            Tx["hash"] = addHexPrefix(tx.hash());

            for(const auto& utxo : tx.utxos())
            {
                for(auto & owner: utxo.owner())
                {
                    Tx["from"].push_back(addHexPrefix(owner));
                }

                for(auto & vout : utxo.vout())
                {
                    nlohmann::json utxoVout;
                    utxoVout["pub"] = addHexPrefix(vout.addr());
                    utxoVout["value"] = vout.value();

                    Tx["to"].push_back(utxoVout); 
                }
            }
            Tx["type"] = tx.txtype();

            allTx[k++] = fixedTxField(Tx);
        }
        else if(tx.type() == global::ca::GENESIS_SIGN)
        {
            Tx["time"] = tx.time();
            Tx["hash"] = addHexPrefix(tx.hash());
            for(const auto& utxo : tx.utxos())
            {
                for(auto & owner: utxo.owner())
                {
                    Tx["from"].push_back(addHexPrefix(owner));
                }

                for(auto & vout : utxo.vout())
                {
                    nlohmann::json utxoVout;
                    utxoVout["addr"] = addHexPrefix(vout.addr());
                    utxoVout["value"] = vout.value();

                    Tx["to"].push_back(utxoVout); 
                }
            }
            Tx["type"] = tx.txtype();
            allTx[k++] = fixedTxField(Tx);
        }
    }
    
    jsonBlock["tx"] = allTx;
    
    std::string json = jsonBlock.dump(4);
    return json;
}

void BlockHttpCallbackHandler::Test()
{
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist(1, 32767);

    std::stringstream stream;
    stream << "Test http callback, ID: " << dist(mt);
    std::string testStr = stream.str();
    AddBlock(testStr);
}

