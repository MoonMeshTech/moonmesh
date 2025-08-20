#include <cstdint>
#include <iostream>
#include <sstream>
#include <stack>
#include <sys/types.h>
#include <time.h>
#include <fstream>
#include <string>

#include "ca/test.h"
#include "ca/global.h"
#include "ca/block_helper.h"
#include "ca/block_monitor.h"
#include "ca/block_stroage.h"
#include "ca/double_spend_cache.h"
#include "ca/block_http_callback.h"
#include "ca/failed_transaction_cache.h"
#include "ca/ca.h"
#include "ca/sync_block.h"
#include "ca/bonus_addr_cache.h"

#include "net/test.hpp"
#include "net/epoll_mode.h"
#include "net/socket_buf.h"
#include "net/http_server.h"
#include "net/unregister_node.h"
#include "net/work_thread.h"


#include <nlohmann/json.hpp>
#include "utils/envelop.h"
#include "utils/tmp_log.h"
#include "utils/console.h"
#include "utils/hex_code.h"
#include "utils/time_util.h"
#include "utils/magic_singleton.h"
#include "utils/account_manager.h"
#include "utils/contract_utils.h"

#include "db/db_api.h"
#include "include/logging.h"

int printFormatTime(uint64_t time, bool isConsoleOutput, std::ostream & stream)
{
    time_t s = (time_t)(time / 1000000);
    struct tm * gmDate;
    gmDate = localtime(&s);

	CaConsole tmColor(CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK, true);
    if(isConsoleOutput)
	{
        stream << tmColor.Color() << gmDate->tm_year + 1900 << "-" << gmDate->tm_mon + 1 << "-" << gmDate->tm_mday << " "  << gmDate->tm_hour << ":" << gmDate->tm_min << ":" << gmDate->tm_sec << "(" << time << ")" << tmColor.Reset() << std::endl;
	}
	else
	{
        stream << gmDate->tm_year + 1900 << "-" << gmDate->tm_mon + 1 << "-" << gmDate->tm_mday << " "  << gmDate->tm_hour << ":" << gmDate->tm_min << ":" << gmDate->tm_sec << "(" << time << ")" << std::endl;
    }

    return 0;
}

int printRocksdbInfo(uint64_t start, uint64_t end, bool isConsoleOutput, std::ostream & stream)
{
    if(start > end)
    {
        ERRORLOG("start > end");
        return -1;
    }

	DBReader dbReader;
    CaConsole bkColor(CONSOLE_COLOR_BLUE, CONSOLE_COLOR_BLACK, true);
    uint64_t count = 0 ;
    uint64_t height = end;
    for (; height >= start; --height) 
    {
	    std::vector<std::string> blockHashList;
	    dbReader.getBlockHashsByBlockHeight(height, blockHashList);
        std::vector<CBlock> blocks;
        for (auto hash : blockHashList)
        {
            std::string strHeader;
            dbReader.getBlockByBlockHash(hash, strHeader);
            DEBUGLOG("blockHash:{}, blockSize:{}", hash.substr(0,6), strHeader.size());
            CBlock block;
            block.ParseFromString(strHeader);
            blocks.push_back(block);
        }
        std::sort(blocks.begin(), blocks.end(), [](CBlock & a, CBlock & b){
            return a.time() < b.time();
        });

        count ++;
        std::cout << "rate of progress------>" << count << "/" << end << std::endl;
        for (auto & block : blocks)
        {

            PrintBlock(block, isConsoleOutput, stream);
        }
        if(height == start)break;
    }
    
    return 0;
}

int PrintBlock(const CBlock & block, bool isConsoleOutput, std::ostream & stream)
{
    CaConsole bkColor(CONSOLE_COLOR_BLUE, CONSOLE_COLOR_BLACK, true);
    CaConsole greenColor(CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK, true);
    stream << std::endl << "BlockInfo ---------------------- > height [" << block.height() << "]" << std::endl;
    stream << "HashMerkleRoot -> " << block.merkleroot() << std::endl;
    stream << "HashPrevBlock -> " << block.prevhash() << std::endl;
    if (isConsoleOutput)
    {
        stream << "BlockHash -> " << bkColor.Color() << block.hash() << bkColor.Reset() << std::endl;
    }
    else
    {
        stream << "BlockHash -> " << block.hash() << std::endl;
    }

    stream << "blockverifySign[" << block.sign_size() << "]" << std::endl;
    for (auto & verifySign : block.sign())
    {
        stream << "block Verify Sign " << Str2Hex(verifySign.sign()) << " : " << Str2Hex(verifySign.pub()) << "[" << greenColor.Color() << GenerateAddr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
    }
    
    for (auto & verifySign : block.sign())
    {
        stream << "block signer -> [" << greenColor.Color() << GenerateAddr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
    }
    
    stream << "Time-> ";
    printFormatTime(block.time(), isConsoleOutput, stream);
    
    for (int i = 0; i < block.txs_size(); i++) 
    {
        CTransaction tx = block.txs(i);
        stream << "TX_INFO -----------> index[" << i << "]" << std::endl;
        PrintTx(tx, isConsoleOutput, stream);
    }

    stream << "Block data ------->"<<  block.data() << std::endl;
    return 0;
}

std::string PrintBlocks(int num, bool preHashEnabled)
{
    DBReader dbRead;
    uint64_t top = 0;
    dbRead.getBlockTop(top);
    std::string str = "top:\n";
    str += "--------------\n";
    int j = 0;
    for(int i = top; i >= 0; i--){
        str += (std::to_string(i) + "\t");
        std::vector<std::string> blockHashList;
        dbRead.getBlockHashsByBlockHeight(i, blockHashList);
        std::sort(blockHashList.begin(), blockHashList.end());
        for (auto hash : blockHashList) {
            std::string strHeader;
            dbRead.getBlockByBlockHash(hash, strHeader);
            CBlock header;
            header.ParseFromString(strHeader);
            if(preHashEnabled)
            {
                str = str + hash.substr(0,6) + "(" + header.prevhash().substr(0,6) + ")" + " ";
            }else{
                str = str + hash.substr(0,6) + " ";
            }
        }
        str += "\n";
        j++;
        if(num > 0 && j >= num)
        {
            break;
        }
    }
    str += "--------------\n";
    return str;
}

std::string print_blocks_hash(int num, bool preHashEnabled)
{
    DBReader dbRead;
    uint64_t top = 0;
    dbRead.getBlockTop(top);
    std::string str = "top:\n";
    str += "--------------\n";
    int j = 0;
    for(int i = top; i >= 0; i--){
        str += (std::to_string(i) + "\n");
        std::vector<std::string> blockHashList;
        dbRead.getBlockHashsByBlockHeight(i, blockHashList);
        std::sort(blockHashList.begin(), blockHashList.end());
        for (auto hash : blockHashList) {
            std::string strHeader;
            dbRead.getBlockByBlockHash(hash, strHeader);
            CBlock header;
            header.ParseFromString(strHeader);
            if(preHashEnabled)
            {
                str = str + hash + "(" + header.prevhash().substr(0,6) + ")" + " \n";
            }else{
                str = str + hash + " \n";
            }
        }
        str += "\n";
        j++;
        if(num > 0 && j >= num)
        {
            break;
        }
    }
    str += "--------------\n";
    return str;
}

std::string printRangeBlocks(int startNum,int num, bool preHashEnabled)
{
    DBReader dbRead;
    uint64_t top = 0;
    dbRead.getBlockTop(top);
    std::string str = "top:\n";
    str += "--------------\n";

    if(startNum > top || startNum < 0)
    {
        std::string strTop = std::to_string(top);
        str += "height error,Current height ";
        str += strTop;
        return str;
    }
    if(num > startNum)
    {
        num = startNum;
    }

    int j = 0;
    for(int i = startNum; i >= 0; i--){
        str += (std::to_string(i) + "\t");
        std::vector<std::string> blockHashList;
        dbRead.getBlockHashsByBlockHeight(i, blockHashList);
        std::sort(blockHashList.begin(), blockHashList.end());
        for (auto hash : blockHashList) {
            std::string strHeader;
            dbRead.getBlockByBlockHash(hash, strHeader);
            CBlock header;
            header.ParseFromString(strHeader);
            if(preHashEnabled)
            {
                str = str + hash.substr(0,6) + "(" + header.prevhash().substr(0,6) + ")" + " ";
            }else{
                str = str + hash.substr(0,6) + " ";
            }
        }
        str += "\n";
        j++;
        if(num > 0 && j >= num)
        {
            break;
        }
    }

    str += "--------------\n";
    return str;
}

int PrintTx(const CTransaction & tx, bool isConsoleOutput, std::ostream & stream)
{
    if (isConsoleOutput)
    {
        CaConsole txColor(CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK, true);
        CaConsole greenColor(CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK, true);
        stream << "TxHash -> " << txColor.Color() << tx.hash() << txColor.Reset() << std::endl;
        stream << "n -> " << tx.n() << std::endl;
        stream << "identity -> " << "[" << greenColor.Color() << tx.identity() << greenColor.Reset() << "] " << std::endl;
        stream << "type -> " << tx.type() << std::endl;

        stream << "verifySign[" << tx.verifysign_size() << "]" << std::endl;

        for (auto & verifySign : tx.verifysign())
        {
            stream << "Verify Sign " << Str2Hex(verifySign.sign()) << " : " << Str2Hex(verifySign.pub()) << "[" << greenColor.Color() << GenerateAddr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
        }
        
        for (auto & verifySign : tx.verifysign())
        {
            stream << "Transaction signer -> [" << greenColor.Color() << GenerateAddr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
        }

        for(auto utxo : tx.utxos())
        {
            stream << "Owner -> ";
            for (auto & addr : utxo.owner())
            {
                stream << "[" << greenColor.Color() << addr << greenColor.Reset() << "]";
            }
            stream << std::endl;

            for (int j = 0; j < utxo.vin_size(); j++)
            {
                const CTxInput & vin = utxo.vin(j);
                stream << "vin[" << j << "] sequence -> " << vin.sequence() << std::endl;
                if (!vin.vinsign().sign().empty() && !vin.vinsign().pub().empty()) {
                    stream << "vin[" << j << "] sign -> " << Str2Hex(vin.vinsign().sign()) << " : " << Str2Hex(vin.vinsign().pub()) << "[" << greenColor.Color() << GenerateAddr(vin.vinsign().pub()) << greenColor.Reset() << "]" << std::endl;
                } else {
                    stream << "vin[" << j << "] sign -> [None] (Genesis Block)" << std::endl;
                }

                for (auto & prevout : vin.prevout())
                {
                    stream << "vin[" << j << "] Prev Output Hash -> " << prevout.n() << " : " << prevout.hash() << std::endl;
                }
            }

            for (int j = 0; j < utxo.vout_size(); j++)
            {
                const CTxOutput & vout = utxo.vout(j);
                CaConsole amount(CONSOLE_COLOR_YELLOW, CONSOLE_COLOR_BLACK, true);
                stream << "vout[" << j << "] public key -> [" << greenColor.Color() <<  vout.addr() << greenColor.Reset() << "]" << std::endl;
                stream << "vout[" << j << "] value -> [" << amount.Color() <<  vout.value() << amount.Reset() << "]" << std::endl;
            }

            for (int j = 0; j < utxo.multisign_size(); j++)
            {
                const CSign & multiSign = utxo.multisign(j);
                stream << "multiSign[" << j << "] -> " << Str2Hex(multiSign.sign()) << " : " << Str2Hex(multiSign.pub()) << "[" << greenColor.Color() << GenerateAddr(multiSign.pub()) << greenColor.Reset() << "]" << std::endl;
            }
            stream << "assetType: " << utxo.assettype() << std::endl;
        }
    }
    else
    {
        stream << "TxHash -> " << tx.hash() << std::endl;
        stream << "n -> " << tx.n() << std::endl;
        stream << "identity -> " << tx.identity() << std::endl;
        stream << "type -> " << tx.type() << std::endl;

        stream << "verifySign[" << tx.verifysign_size() << "]" << std::endl;

        for (auto & verifySign : tx.verifysign())
        {
            stream << "Verify Sign " << Str2Hex(verifySign.sign()) << " : " << Str2Hex(verifySign.pub()) << "[" << GenerateAddr(verifySign.pub()) << "]" << std::endl;  
        }
        
        for (auto & verifySign : tx.verifysign())
        {
            stream << "Transaction signer -> [" << GenerateAddr(verifySign.pub()) << "]" << std::endl;
        }

        for(auto utxo : tx.utxos())
        {
            stream << "Owner -> ";
            for (auto & addr : utxo.owner())
            {
                stream << "[" << addr << "]";
            }
            stream << std::endl;
            for (int j = 0; j < utxo.vin_size(); j++)
            {
                const CTxInput & vin = utxo.vin(j);
                stream << "vin[" << j << "] sequence -> " << vin.sequence() << std::endl;
                if (!vin.vinsign().sign().empty() && !vin.vinsign().pub().empty()) {
                    stream << "vin[" << j << "] sign -> " << Str2Hex(vin.vinsign().sign()) << " : " << Str2Hex(vin.vinsign().pub()) << "[" << GenerateAddr(vin.vinsign().pub()) << "]" << std::endl;
                } else {
                    stream << "vin[" << j << "] sign -> [None] (Genesis Block)" << std::endl;
                }

                for (auto & prevout : vin.prevout())
                {
                    stream << "vin[" << j << "] Prev Output Hash -> " << prevout.n() << " : " << prevout.hash() << std::endl;
                }
            }

            for (int j = 0; j < utxo.vout_size(); j++)
            {
                const CTxOutput & vout = utxo.vout(j);
                stream << "vout[" << j << "] public key -> [" << vout.addr() << "]" << std::endl;
                stream << "vout[" << j << "] value -> [" << vout.value() << "]" << std::endl;
            }

            for (int j = 0; j < utxo.multisign_size(); j++)
            {
                const CSign & multiSign = utxo.multisign(j);
                stream << "multiSign[" << j << "] -> " << Str2Hex(multiSign.sign()) << " : " << Str2Hex(multiSign.pub()) << GenerateAddr(multiSign.pub()) << std::endl;
            }
            
            stream << "assetType: " << utxo.assettype() << std::endl;
        }
    }

    stream << "Time -> " << MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(tx.time()) << std::endl;
    stream << "(" << tx.time() <<")" << std::endl;
    
    std::vector<std::pair<std::string, std::string>> dataMap;
    std::string strData;
    if((global::ca::TxType)tx.txtype() != global::ca::TxType::TX_TYPE_TX)
    {
        try
        {
            nlohmann::json dataJson = nlohmann::json::parse(tx.data());
            global::ca::TxType txType = (global::ca::TxType)tx.txtype();
            dataMap.push_back(std::make_pair("TxType", std::to_string((int32_t)txType)));
            dataMap.push_back(std::make_pair("Consensus", std::to_string(tx.consensus())));
            dataMap.push_back(std::make_pair("Gas", std::to_string(0)));
            dataMap.push_back(std::make_pair("Cost", std::to_string(0)));
            if (txType == global::ca::TxType::TX_TYPE_TX)
            {
               
            }
            else if (txType == global::ca::TxType::kTransactionTypeStake)
            {
                dataMap.push_back(std::make_pair("StakeType", dataJson["TxInfo"]["StakeType"].get<std::string>()));
                dataMap.push_back(std::make_pair("StakeAmount", std::to_string(dataJson["TxInfo"]["StakeAmount"].get<uint64_t>())));
                dataMap.push_back(std::make_pair("CommissionRate", std::to_string(dataJson["TxInfo"]["CommissionRate"].get<double>())));
            }
            else if (txType == global::ca::TxType::kTxTypeUnstake_)
            {
                dataMap.push_back(std::make_pair("UnstakeUtxo", dataJson["TxInfo"]["UnstakeUtxo"].get<std::string>()));
            }
            
            for (auto & item : dataMap)
            {
                strData += "  " + item.first + " : " + item.second + "\n";
            }
        }
        catch (...)
        {
        }
    }

    stream << "data -> " << std::endl;
    stream << strData;
    stream << "version -> " << tx.version() << std::endl;
    
    stream << "----------------------------" << std::endl;
    stream << tx.data() << std::endl;
    return 0;
}

void BlockInvert(const std::string & strHeader, nlohmann::json &blocks)
{
    CBlock block;
    if(!block.ParseFromString(strHeader))
    {
        ERRORLOG("block_raw parse fail!");
        return ;
        
    }

    nlohmann::json allTx;
    nlohmann::json jsonBlock;
    jsonBlock["merkleroot"] = addHexPrefix(block.merkleroot());
    jsonBlock["prevhash"] = addHexPrefix(block.prevhash());
    jsonBlock["hash"] = addHexPrefix(block.hash());
    jsonBlock["height"] = block.height();
    jsonBlock["time"] = block.time();
    jsonBlock["bytes"] = block.ByteSizeLong();
    
    if(!block.data().empty())
    {
        nlohmann::json blockdataJson = nlohmann::json::parse(block.data());
        if(FixBlockDataField(blockdataJson) != 0)
        {
            return;
        }
        nlohmann::json modifiedJsonData;

        for (auto it = blockdataJson.begin(); it != blockdataJson.end(); ++it)
        {

            std::string originalKey = it.key();
            auto value = it.value();
            if(value.contains("dependentCTx"))
            {
                std::set<std::string> tempVec;
                for(auto & dep : value["dependentCTx"])
                {
                    tempVec.insert(addHexPrefix(dep));
                }
                value["dependentCTx"] = tempVec;
            }
            if(value.contains("creation"))
            {
                nlohmann::json creationJson = nlohmann::json::parse(value["creation"].dump());
                nlohmann::json tempCreation;
                for(auto it = creationJson.begin(); it != creationJson.end(); ++it)
                {
                    std::string key = addHexPrefix(it.key());
                    tempCreation[key] = it.value();
                }
                value["creation"] = tempCreation;
            }

            std::string modifiedKey = addHexPrefix(originalKey);
            modifiedJsonData[modifiedKey] = value;
        }
        blockdataJson = modifiedJsonData;
        jsonBlock["data"] = blockdataJson;
    }
    else
    {
        jsonBlock["data"] = "";
    }

    for(auto & blocksign : block.sign())
    {
        nlohmann::json verifyBlockSignature;
        verifyBlockSignature["sign"] = Base64Encode(blocksign.sign());
        verifyBlockSignature["pub"] = Base64Encode(blocksign.pub());
        std::string sign_addr = GenerateAddr(blocksign.pub());
        verifyBlockSignature["signaddr"] = addHexPrefix(sign_addr);

        jsonBlock["blocksign"].push_back(verifyBlockSignature);
    }


    int k = 0;
    for(auto & tx : block.txs())
    {
        nlohmann::json Tx;
        if(tx.type() == global::ca::kTxSign)
        {   
            Tx["time"] = tx.time();
            Tx["txHash"] = addHexPrefix(tx.hash());
  
            if((global::ca::TxType)tx.txtype() != global::ca::TxType::TX_TYPE_TX)
            {
                nlohmann::json dataJson = nlohmann::json::parse(tx.data());
                FixTxDataField(dataJson, tx.txtype());
                Tx["data"] = dataJson;
            }

            Tx["identity"] = addHexPrefix(tx.identity());
            for(const auto& utxo : tx.utxos())
            {
                nlohmann::json utxoJs;
                utxo.assettype() == global::ca::ASSET_TYPE_VOTE ? utxoJs["assetType"] = utxo.assettype() : utxoJs["assetType"] = addHexPrefix(utxo.assettype());
                // utxoJs["assetType"] = addHexPrefix(utxo.assettype());
                utxoJs["gasutxo"] = utxo.gasutxo();

                for(auto & owner: utxo.owner())
                {
                    utxoJs["owner"].push_back(addHexPrefix(owner));
                }

                for(auto & vin : utxo.vin())
                {
                    for(auto &prevout : vin.prevout())
                    {
                        utxoJs["vin"]["prevout"]["hash"].push_back(addHexPrefix(prevout.hash()));
                    }

                    nlohmann::json utxoVinSign;
                    utxoVinSign["sign"] = Base64Encode(vin.vinsign().sign());
                    utxoVinSign["pub"] = Base64Encode(vin.vinsign().pub());

                    utxoJs["vin"]["vinsign"].push_back(utxoVinSign);
                }

                for(auto & vout : utxo.vout())
                {
                    nlohmann::json utxoVout;

                    if(vout.addr().substr(0, 6) == "Virtua" || vout.addr().substr(0, 4)== "Lock")
                    {
                        utxoVout["addr"] = vout.addr();
                    }
                    else
                    {
                        utxoVout["addr"] =  addHexPrefix(vout.addr());
                    }
                    utxoVout["value"] = vout.value();
                    utxoJs["vout"].push_back(utxoVout); 
                }

                for(auto & multiSign : utxo.multisign())
                {
                    nlohmann::json utxoMultiSign;
                    utxoMultiSign["sign"] = Base64Encode(multiSign.sign());
                    utxoMultiSign["pub"] = Base64Encode(multiSign.sign());

                    utxoJs["multisign"].push_back(utxoMultiSign);
                }
                Tx["utxo"].push_back(utxoJs);
            }
            Tx["GasTx"] = tx.gastx();
            Tx["Type"] = tx.type();
            Tx["info"] = tx.info();
            Tx["Consensus"] = tx.consensus();
            Tx["txType"] = tx.txtype();

            for(auto & verifySign : tx.verifysign())
            {
                nlohmann::json utxoSignVerifier;
                utxoSignVerifier["sign"] = Base64Encode(verifySign.sign());
                utxoSignVerifier["pub"] = Base64Encode(verifySign.pub());
                std::string signAddr = GenerateAddr(verifySign.pub());
                utxoSignVerifier["signaddr"] = addHexPrefix(signAddr);

                Tx["verifySign"].push_back(utxoSignVerifier);
            }
            
            allTx[k++] = Tx;
        }
        else if(tx.type() == global::ca::GENESIS_SIGN)
        {
            Tx["time"] = tx.time();
            Tx["txHash"] = addHexPrefix(tx.hash());
            Tx["identity"] = addHexPrefix(tx.identity());

            for(const auto& utxo : tx.utxos())
            {
                for(auto & owner: utxo.owner())
                {
                    Tx["utxo"]["owner"].push_back(addHexPrefix(owner));
                }

                for(auto & vin : utxo.vin())
                {
                    for(auto &prevout : vin.prevout())
                    {
                        Tx["utxo"]["vin"]["prevout"]["hash"].push_back(addHexPrefix(prevout.hash()));
                    }

                    nlohmann::json utxoVinSign;
                    utxoVinSign["sign"] = Base64Encode(vin.vinsign().sign());
                    utxoVinSign["pub"] = Base64Encode(vin.vinsign().pub());

                    Tx["utxo"]["vin"]["vinsign"].push_back(utxoVinSign);
                }

                for(auto & vout : utxo.vout())
                {
                    nlohmann::json utxoVout;
                    if(vout.addr().substr(0, 6) == "Virtua" || vout.addr().substr(0, 4)== "Lock")
                    {
                        utxoVout["addr"] = vout.addr();    
                    }
                    else
                    {
                        utxoVout["addr"] =  addHexPrefix(vout.addr());
                    }
                    utxoVout["value"] = vout.value();

                    Tx["utxo"]["vout"].push_back(utxoVout); 
                }
            }
            Tx["type"] = tx.type();
            allTx[k++] = Tx;
        }
    }

    blocks["block"] = jsonBlock;
    blocks["tx"] = allTx;

}


std::string PrintCache(int where){
    std::string rocksdb_usage;
    MagicSingleton<RocksDB>::GetInstance()->getDBMemoryUsage(rocksdb_usage);
    std::cout << rocksdb_usage << std::endl;
    std::stringstream ss;

    auto cacheString=[&](const std::string & cacheData,uint64_t cacheSize,bool isEnd=false){
        std::time_t t = std::time(NULL);
        char mbstr[100];
        if (std::strftime(mbstr, sizeof(mbstr), "%A %c", std::localtime(&t))) {
        }

       ss << cacheSize <<( (isEnd) ? ",\n" :",");
       
    };

    auto blockHelper= MagicSingleton<BlockHelper>::GetInstance();
    auto blockMonitor= MagicSingleton<BlockMonitor>::GetInstance();
    auto blockStorage=MagicSingleton<BlockStorage>::GetInstance(); 
    auto double_spend_cache_count=MagicSingleton<doubleSpendCache>::GetInstance(); 
    auto  failedTransactionCacheCount=MagicSingleton<failedTransactionCache>::GetInstance(); 
    auto SyncBlockInfo= MagicSingleton<SyncBlock>::GetInstance();
    auto transaction_cache_=MagicSingleton<TransactionCache>::GetInstance();
    auto vrfo= MagicSingleton<VRF>::GetInstance();
    auto TFP_BENCHMARK_C=  MagicSingleton<Benchmark>::GetInstance();

    GlobalDataManager & manager=GlobalDataManager::get_global_data_manager();
    
    
    auto unregisterNodeRequest= MagicSingleton<UnregisterNode>::GetInstance();

    auto bufcontrol =MagicSingleton<bufferControl>::GetInstance();

    auto pernode = MagicSingleton<PeerNode>::GetInstance();

    auto dispach=MagicSingleton<ProtobufDispatcher>::GetInstance();

    auto echoCatch = MagicSingleton<echoTest>::GetInstance();

    auto workThread =MagicSingleton<WorkThreads>::GetInstance();

    auto phone_list = global::Phone_List;
    auto httpCallbackForCBlock_ = MagicSingleton<BlockHttpCallbackHandler>::GetInstance();


    std::stack<std::string> emyp;

    {
        cacheString("",blockStorage->preHashMap.size());
        cacheString("",blockStorage->blockStatusMap.size());

        cacheString("",vrfo->vrf_cache_data.size());
        cacheString("",vrfo->tx_vrf_cache_.size());
        cacheString("", vrfo->vrfVerificationNodeRequest.size());

        cacheString("",manager._globalData.size());
        cacheString("",unregisterNodeRequest->_nodes.size());
        cacheString("",unregisterNodeRequest->_consensusNodeList.size());
        cacheString("", bufcontrol->_BufferMap.size());
        cacheString("",pernode->_nodeMap.size());
        cacheString("",dispach->chainProtocolCallbacks.size());
        cacheString("",dispach->netProtocolCallbacks.size());
        cacheString("",dispach->broadcastProtocol.size());
        cacheString("",dispach->txProtocolCallbacks.size());
        cacheString("",dispach->syncBlockProtocolCallbacks.size());
        cacheString("",dispach->saveBlockProtocolCallbacks.size());
        cacheString("",dispach->blockProtocolCallbacks.size());
        cacheString("",global::requestCountMap.size());
        cacheString("",HttpServer::rpcCbs.size());
        cacheString("",HttpServer::_cbs.size());
        cacheString("",echoCatch->echoCatch.size());
        cacheString("",workThread->threadsWorkList.size());
        cacheString("",workThread->threadsReadList.size());
        cacheString("",workThread->threadsTransList.size());
        cacheString("",phone_list.size());
        cacheString("",httpCallbackForCBlock_->_addblocks.size());
        cacheString("",mutex_size());
        cacheString("",httpCallbackForCBlock_->rollbackBlocks_.size(),true);
    }

    switch (where) {

        case 0:
        {
            std::cout << ss.str();
            return std::string("hh");
        }break;
        case 1:{
            blockStorage->preHashMap.clear();
            blockStorage->blockStatusMap.clear();
    

            vrfo->vrf_cache_data.clear();
            vrfo->tx_vrf_cache_.clear();
            vrfo->vrfVerificationNodeRequest.clear();
            manager._globalData.clear();

            phone_list.clear();
        }break;
        case 2:
        {
            std::ofstream file("cache.txt",std::ios::app);
            file << ss.str();
            file.close();
            return std::string("k");
        }break;
        case 3:
        {
            std::cout<<"start DesInstance"<<std::endl;
            MagicSingleton<Config>::DesInstance();
            MagicSingleton<Benchmark>::DesInstance();
            MagicSingleton<ProtobufDispatcher>::DesInstance();
            MagicSingleton<AccountManager>::DesInstance();
            MagicSingleton<PeerNode>::DesInstance();
            MagicSingleton<UnregisterNode>::DesInstance();
            MagicSingleton<TimeUtil>::DesInstance();
            MagicSingleton<netTest>::DesInstance();
            MagicSingleton<Envelop>::DesInstance();
            MagicSingleton<echoTest>::DesInstance();
            MagicSingleton<bufferControl>::DesInstance();
            MagicSingleton<BlockHelper>::DesInstance();
            MagicSingleton<TaskPool>::DesInstance();
            MagicSingleton<BlockHttpCallbackHandler>::DesInstance();
            MagicSingleton<VRF>::DesInstance();
            MagicSingleton<BlockMonitor>::DesInstance();
            MagicSingleton<BlockStorage>::DesInstance();
            MagicSingleton<mm::ca::BonusAddrCache>::DesInstance();
            MagicSingleton<doubleSpendCache>::DesInstance();
            MagicSingleton<failedTransactionCache>::DesInstance();
            MagicSingleton<SyncBlock>::DesInstance();
            MagicSingleton<TransactionCache>::DesInstance();
            MagicSingleton<EpollMode>::DesInstance();
            MagicSingleton<WorkThreads>::DesInstance();

            MagicSingleton<RocksDB>::GetInstance()->sestoryDB();
            MagicSingleton<RocksDB>::DesInstance();

            std::map<std::string, std::shared_ptr<GlobalData>> dataTemp;
            GlobalDataManager & manager=GlobalDataManager::get_global_data_manager();

            manager._globalData.swap(dataTemp);

            
            std::cout<<"end DesInstance"<<std::endl;

        }break;
    
    }
    return std::string("ddd");

}

int print_contract_block(const CBlock & block, bool isConsoleOutput, std::ostream & stream)
{
    CaConsole bkColor(CONSOLE_COLOR_BLUE, CONSOLE_COLOR_BLACK, true);
    CaConsole greenColor(CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK, true);
    auto tempTransactions = block.txs();
    if((global::ca::TxType)tempTransactions[0].txtype()!=global::ca::TxType::TX_TYPE_INVOKE_CONTRACT && (global::ca::TxType)tempTransactions[0].txtype()!=global::ca::TxType::kTransactionTypeDeploy)
    {
        return -1;
    }
    stream << std::endl << "BlockInfo ---------------------- > height [" << block.height() << "]" << std::endl;
    stream << "HashMerkleRoot -> " << block.merkleroot() << std::endl;
    stream << "HashPrevBlock -> " << block.prevhash() << std::endl;
    if (isConsoleOutput)
    {
        stream << "BlockHash -> " << bkColor.Color() << block.hash() << bkColor.Reset() << std::endl;
    }
    else
    {
        stream << "BlockHash -> " << block.hash() << std::endl;
    }

    stream << "blockverifySign[" << block.sign_size() << "]" << std::endl;
    for (auto & verifySign : block.sign())
    {
        stream << "block Verify Sign " << Str2Hex(verifySign.sign()) << " : " << Str2Hex(verifySign.pub()) << "[" << greenColor.Color() << GenerateAddr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
    }
    
    for (auto & verifySign : block.sign())
    {
        stream << "block signer -> [" << greenColor.Color() << GenerateAddr(verifySign.pub()) << greenColor.Reset() << "]" << std::endl;
    }
    
    stream << "Time-> ";
    printFormatTime(block.time(), isConsoleOutput, stream);
    
    for (int i = 0; i < block.txs_size(); i++) 
    {
        CTransaction tx = block.txs(i);
        stream << "TX_INFO -----------> index[" << i << "]" << std::endl;
        PrintTx(tx, isConsoleOutput, stream);
    }
    
    return 0;
}

std::string DisplayContractSections(int num, bool preHashEnabled)
{
    DBReader dbRead;
    uint64_t top = 0;
    dbRead.getBlockTop(top);
    std::string str = "top:\n";
    str += "--------------\n";
    int j = 0;
    for(int i = top; i >= 0; i--)
    {
        str += (std::to_string(i) + "\t");
        std::vector<std::string> blockHashList;
        dbRead.getBlockHashsByBlockHeight(i, blockHashList);
        std::sort(blockHashList.begin(), blockHashList.end());
        for (auto hash : blockHashList) 
        {
            std::string strHeader;
            dbRead.getBlockByBlockHash(hash, strHeader);
            CBlock header;
            header.ParseFromString(strHeader);
            auto tempTransactions = header.txs();
            if(preHashEnabled)
            {
                if((global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::TX_TYPE_INVOKE_CONTRACT || (global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::kTransactionTypeDeploy)
                {
                    str = str + hash.substr(0,6) + "(" + header.prevhash().substr(0,6) + ")" + " ";
                }
                else
                {
                    str = str + hash.substr(0,6) +"(c)"+ "(" + header.prevhash().substr(0,6) + ")" + " ";
                }
            }
            else
            {
                if((global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::TX_TYPE_INVOKE_CONTRACT || (global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::kTransactionTypeDeploy)
                {
                    str = str + hash.substr(0,6) +"(c)"+ " " ;
                }
                else
                {
                    str = str + hash.substr(0,6) + " ";
                }
            }
        }
        str += "\n";
        j++;
        if(num > 0 && j >= num)
        {
            break;
        }
    }
    str += "--------------\n";
    return str;
}

std::string print_range_contract_blocks(int startNum,int num, bool preHashEnabled)
{
    DBReader dbRead;
    uint64_t top = 0;
    dbRead.getBlockTop(top);
    std::string str = "top:\n";
    str += "--------------\n";

    if(startNum > top || startNum < 0)
    {
        std::string strTop = std::to_string(top);
        str += "height error,Current height ";
        str += strTop;
        return str;
    }
    if(num > startNum)
    {
        num = startNum;
    }

    int j = 0;
    for(int i = startNum; i >= 0; i--){
        str += (std::to_string(i) + "\t");
        std::vector<std::string> blockHashList;
        dbRead.getBlockHashsByBlockHeight(i, blockHashList);
        std::sort(blockHashList.begin(), blockHashList.end());
        for (auto hash : blockHashList) {
            std::string strHeader;
            dbRead.getBlockByBlockHash(hash, strHeader);
            CBlock header;
            header.ParseFromString(strHeader);
            auto tempTransactions = header.txs();
            if(preHashEnabled)
            {
                if((global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::TX_TYPE_INVOKE_CONTRACT || (global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::kTransactionTypeDeploy)
                {
                    str = str + hash.substr(0,6) +"(c)"+ "(" + header.prevhash().substr(0,6) + ")" + " ";
                }
                else
                {
                    str = str + hash.substr(0,6)  +"(" + header.prevhash().substr(0,6) + ")" + " ";
                }
            }
            else
            {
                if((global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::TX_TYPE_INVOKE_CONTRACT || (global::ca::TxType)tempTransactions[0].txtype()==global::ca::TxType::kTransactionTypeDeploy)
                {
                    str = str + hash.substr(0,6) +"(c)"+ " " ;
                }
                else
                {
                    str = str + hash.substr(0,6) + " ";
                }
            }
        }
        str += "\n";
        j++;
        if(num > 0 && j >= num)
        {
            break;
        }
    }

    str += "--------------\n";
    return str;
}

nlohmann::json fixedTxField(const nlohmann::json& inTx)
{
    nlohmann::json outTx = inTx;
    if (outTx.contains("data") && outTx["data"].contains("TxInfo") && outTx["data"]["TxInfo"].contains("recipient") && outTx["data"]["TxInfo"].contains("sender")) 
    {
        std::string recipient = outTx["data"]["TxInfo"]["recipient"].get<std::string>();
        outTx["data"]["TxInfo"]["recipient"] = addHexPrefix(recipient);

        std::string sender = outTx["data"]["TxInfo"]["sender"].get<std::string>();
        outTx["data"]["TxInfo"]["sender"] = addHexPrefix(sender);
    }

    if (outTx.contains("data") && outTx["data"].contains("TxInfo") && outTx["data"]["TxInfo"].contains("contractDeployer"))     
    {
        std::string contractDeployer = outTx["data"]["TxInfo"]["contractDeployer"].get<std::string>();
        outTx["data"]["TxInfo"]["contractDeployer"] = addHexPrefix(contractDeployer);
    }

    return outTx;
}

    
std::string transactionInvestment(const CTransaction& tx)
{
    nlohmann::json Tx;
    if(tx.type() == global::ca::kTxSign)
    {   
        Tx["time"] = tx.time();
        Tx["txHash"] = addHexPrefix(tx.hash());

        if((global::ca::TxType)tx.txtype() != global::ca::TxType::TX_TYPE_TX)
        {
            nlohmann::json dataJson = nlohmann::json::parse(tx.data());
            FixTxDataField(dataJson, tx.txtype());
            Tx["data"] = dataJson;
        }

        Tx["identity"] = addHexPrefix(tx.identity());
        for(const auto& utxo : tx.utxos())
        {
            nlohmann::json utxoJs;
            utxoJs["assetType"] = utxo.assettype();
            utxoJs["gasutxo"] = utxo.gasutxo();

            for(auto & owner: utxo.owner())
            {
                utxoJs["owner"].push_back(addHexPrefix(owner));
            }

            for(auto & vin : utxo.vin())
            {
                for(auto &prevout : vin.prevout())
                {
                    utxoJs["vin"]["prevout"]["hash"].push_back(addHexPrefix(prevout.hash()));
                }

                nlohmann::json utxoVinSign;
                utxoVinSign["sign"] = Base64Encode(vin.vinsign().sign());
                utxoVinSign["pub"] = Base64Encode(vin.vinsign().pub());

                utxoJs["vin"]["vinsign"].push_back(utxoVinSign);
            }

            for(auto & vout : utxo.vout())
            {
                nlohmann::json utxoVout;

                if(vout.addr().substr(0, 6) == "Virtua" || vout.addr().substr(0, 4)== "Lock")
                {
                    utxoVout["addr"] = vout.addr();
                }
                else
                {
                    utxoVout["addr"] =  addHexPrefix(vout.addr());
                }
                utxoVout["value"] = vout.value();
                utxoJs["vout"].push_back(utxoVout); 
            }

            for(auto & multiSign : utxo.multisign())
            {
                nlohmann::json utxoMultiSign;
                utxoMultiSign["sign"] = Base64Encode(multiSign.sign());
                utxoMultiSign["pub"] = Base64Encode(multiSign.sign());

                utxoJs["multisign"].push_back(utxoMultiSign);
            }
            Tx["utxo"].push_back(utxoJs);
        }
        Tx["GasTx"] = tx.gastx();
        Tx["Type"] = tx.type();
        Tx["info"] = tx.info();
        Tx["Consensus"] = tx.consensus();
        Tx["txType"] = tx.txtype();

        for(auto & verifySign : tx.verifysign())
        {
            nlohmann::json utxoSignVerifier;
            utxoSignVerifier["sign"] = Base64Encode(verifySign.sign());
            utxoSignVerifier["pub"] = Base64Encode(verifySign.pub());
            std::string signAddr = GenerateAddr(verifySign.pub());
            utxoSignVerifier["signaddr"] = addHexPrefix(signAddr);

            Tx["verifySign"].push_back(utxoSignVerifier);
        }
    }
    else if(tx.type() == global::ca::GENESIS_SIGN)
    {
        Tx["time"] = tx.time();
        Tx["txHash"] = addHexPrefix(tx.hash());
        Tx["identity"] = addHexPrefix(tx.identity());

        for(const auto& utxo : tx.utxos())
        {
            for(auto & owner: utxo.owner())
            {
                Tx["utxo"]["owner"].push_back(addHexPrefix(owner));
            }

            for(auto & vin : utxo.vin())
            {
                for(auto &prevout : vin.prevout())
                {
                    Tx["utxo"]["vin"]["prevout"]["hash"].push_back(addHexPrefix(prevout.hash()));
                }

                nlohmann::json utxoVinSign;
                utxoVinSign["sign"] = Base64Encode(vin.vinsign().sign());
                utxoVinSign["pub"] = Base64Encode(vin.vinsign().pub());

                Tx["utxo"]["vin"]["vinsign"].push_back(utxoVinSign);
            }

            for(auto & vout : utxo.vout())
            {
                nlohmann::json utxoVout;
                if(vout.addr().substr(0, 6) == "Virtua" || vout.addr().substr(0, 4)== "Lock")
                {
                    utxoVout["addr"] = vout.addr();    
                }
                else
                {
                    utxoVout["addr"] =  addHexPrefix(vout.addr());
                }
                utxoVout["value"] = vout.value();

                Tx["utxo"]["vout"].push_back(utxoVout); 
            }
        }
        Tx["type"] = tx.type();
    }

    nlohmann::json txJs = fixedTxField(Tx);
    return txJs.dump();
}

int FixBlockDataField(nlohmann::json &data)
{
    if (data.contains("TxInfo") && data["TxInfo"].contains("BonusAddr"))
    {
        std::string bonusAddress = data["TxInfo"]["BonusAddr"].get<std::string>();
        data["TxInfo"]["BonusAddr"] = addHexPrefix(bonusAddress);
    }

    if (data.contains("TxInfo") && data["TxInfo"].contains("recipient") && data["TxInfo"].contains("sender"))
    {
        std::string recipient = data["TxInfo"]["recipient"].get<std::string>();
        data["TxInfo"]["recipient"] = addHexPrefix(recipient);

        std::string sender = data["TxInfo"]["sender"].get<std::string>();
        data["TxInfo"]["sender"] = addHexPrefix(sender);
    }

    if (data.contains("TxInfo") && data["TxInfo"].contains("contractDeployer"))
    {
        std::string contractDeployer = data["TxInfo"]["contractDeployer"].get<std::string>();
        data["TxInfo"]["contractDeployer"] = addHexPrefix(contractDeployer);
    }

    return 0;
}


int FixTxDataField(nlohmann::json &dataJson, uint32_t type)
{
    auto txType = (global::ca::TxType)type;
    nlohmann::json txInfo = dataJson["TxInfo"].get<nlohmann::json>();

    if(global::ca::TxType::kTxTypeUnstake_ == txType)
    {
        txInfo["UnstakeUtxo"] = addHexPrefix(txInfo["UnstakeUtxo"].get<std::string>());
    }
    else if(global::ca::TxType::kTxTypeUnLock == txType)
    {
        txInfo["UnLockUtxo"] = addHexPrefix(txInfo["UnLockUtxo"].get<std::string>());
    }
    else if (global::ca::TxType::kTxTypeDelegate == txType)
    {
        txInfo["BonusAddr"] = addHexPrefix(txInfo["BonusAddr"].get<std::string>());
    }
    else if (global::ca::TxType::kTxTypeUndelegate == txType)
    {
        txInfo["UndelegatingUtxo"] = addHexPrefix(txInfo["UndelegatingUtxo"].get<std::string>());
        txInfo["BonusAddr"] = addHexPrefix(txInfo["BonusAddr"].get<std::string>());
    }
    else if(txType == global::ca::TxType::kTransactionTypeDeploy)
    {
        txInfo["recipient"] = addHexPrefix(txInfo["recipient"].get<std::string>());
        txInfo["sender"] = addHexPrefix(txInfo["sender"].get<std::string>());
    }
    else if(txType == global::ca::TxType::TX_TYPE_INVOKE_CONTRACT)
    {
        txInfo["contractDeployer"] = addHexPrefix(txInfo["contractDeployer"].get<std::string>());
        txInfo["input"] = addHexPrefix(txInfo["input"].get<std::string>());
        txInfo["recipient"] = addHexPrefix(txInfo["recipient"].get<std::string>());
        txInfo["sender"] = addHexPrefix(txInfo["sender"].get<std::string>());
    }
    else if(global::ca::TxType::KTXTypeProposal == txType)
    {
        txInfo["ContractAddr"] = addHexPrefix(txInfo["ContractAddr"].get<std::string>());
    }
    else if (global::ca::TxType::KTXTyRevokeProposal == txType)
    {
        txInfo["ProposalHash"] = addHexPrefix(txInfo["ProposalHash"].get<std::string>());
    }
    else if(global::ca::TxType::KTXTyVote == txType)
    {
        txInfo["VoteHash"] = addHexPrefix(txInfo["VoteHash"].get<std::string>());
    }
    else if(global::ca::TxType::kTXTypeFund == txType)
    {
        if(txInfo.contains("FundLockedAmount"))
        {
            std::map<std::string, uint64_t> lockedReward;
            lockedReward = txInfo["FundLockedAmount"].get<std::map<std::string, uint64_t>>();

            std::map<std::string, uint64_t> lockedRewardSecond;
            for(auto &it : lockedReward)
            {
                if(it.first == global::ca::ASSET_TYPE_VOTE)
                {
                    continue;
                }
                lockedRewardSecond[addHexPrefix(it.first)] = it.second;
            }
            txInfo["FundLockedAmount"] = lockedRewardSecond;
        }

        if(txInfo.contains("FundPackageAmount"))
        {
            std::map<std::string, uint64_t> packageReward;
            packageReward = txInfo["FundPackageAmount"].get<std::map<std::string, uint64_t>>();

            std::map<std::string, uint64_t> packageReward2;
            for(auto &it : packageReward)
            {
                if(it.first == global::ca::ASSET_TYPE_VOTE)
                {
                    continue;
                }
                packageReward2[addHexPrefix(it.first)] = it.second;
            }
            txInfo["FundPackageAmount"] = packageReward2;
        }
    }

    dataJson["TxInfo"] = txInfo;

}