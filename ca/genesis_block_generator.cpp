#include "genesis_block_generator.h"
#include "genesis_config.h"
#include "ca/algorithm.h"
#include "ca/global.h"
#include "common/global.h"
#include "include/logging.h"
#include "utils/hex_code.h"
#include "utils/magic_singleton.h"

namespace GenesisConfig {

GenesisBlockGenerator& GenesisBlockGenerator::GetInstance() {
    static GenesisBlockGenerator instance;
    return instance;
}

bool GenesisBlockGenerator::GenerateGenesisBlock(CBlock& block) {
    try {
        const auto& configManager = GenesisConfigManager::GetInstance();
        if (!configManager.IsConfigLoaded()) {
            ERRORLOG("Network config not loaded");
            return false;
        }
        
        const auto& genesisInfo = configManager.GetGenesisInfo();
        
        // Set basic information of the block
        block.set_time(genesisInfo.genesisTime);
        block.set_version(global::ca::kCurrentBlockVersion);
        block.set_prevhash(std::string(64, '0'));
        block.set_height(0);
        
        // Create the Genesis Transaction
        CTransaction tx;
        if (!CreateGenesisTransaction(tx)) {
            ERRORLOG("Failed to create genesis transaction");
            return false;
        }
        
        // Add the transaction to the block
        CTransaction* tx0 = block.add_txs();
        *tx0 = tx;
        
        // Set block data
        nlohmann::json blockData;
        for (const auto& data : genesisInfo.blockData) {
            blockData[data.first] = data.second;
        }
        block.set_data(blockData.dump());
        
        // Calculate the merkle root and the block hash
        block.set_merkleroot(ca_algorithm::calculateBlockMerkle(block));
        block.set_hash(Getsha256hash(block.SerializeAsString()));
        
        return true;
    }
    catch (const std::exception& e) {
        ERRORLOG("Failed to generate genesis block: {}", e.what());
        return false;
    }
}

std::string GenesisBlockGenerator::GenerateGenesisBlockRaw() {
    CBlock block;
    if (!GenerateGenesisBlock(block)) {
        ERRORLOG("Failed to generate genesis block");
        return "";
    }
    
    return Str2Hex(block.SerializeAsString());
}

bool GenesisBlockGenerator::CreateGenesisTransaction(CTransaction& tx) {
    try {
        const auto& configManager = GenesisConfigManager::GetInstance();
        const auto& genesisInfo = configManager.GetGenesisInfo();
        const auto& networkInfo = configManager.GetNetworkInfo();
        
        // Only conduct strict global parameter verification for the primary network.
        if (networkInfo.type == "primary") {
            // Verify whether the creation time is consistent with the global configuration
            if (genesisInfo.genesisTime != global::ca::GetGenesisTime()) {
                ERRORLOG("Primary network genesis time mismatch: config={}, global={}", 
                         genesisInfo.genesisTime, global::ca::GetGenesisTime());
                return false;
            }
            
            // Verify whether the Genesis account address is consistent with the global configuration.
            if (genesisInfo.initAccountAddr != global::ca::GetInitAccountAddr()) {
                ERRORLOG("Primary network genesis account address mismatch: config={}, global={}", 
                         genesisInfo.initAccountAddr, global::ca::GetInitAccountAddr());
                return false;
            }
            
            INFOLOG("Primary network genesis parameters validated against global constants");
        } else {
            INFOLOG("Non-primary network ({}), using flexible genesis parameters from config", networkInfo.type);
        }
        
        const std::string& addr = genesisInfo.initAccountAddr;
        
        // Set transaction basic information
        tx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);
        tx.set_time(genesisInfo.genesisTime);
        tx.set_n(0);
        tx.set_identity(addr);
        tx.set_type(global::ca::GENESIS_SIGN);
        
        // Create UTXO (simplified version, without signature) for each asset type
        for (const auto& balance : genesisInfo.initialBalance) {
            if(balance.first != global::ca::assetType_MM && balance.first != global::ca::ASSET_TYPE_VOTE)
            {
                continue;
            }
            const std::string& assetType = balance.first;
            uint64_t amount = balance.second;
            if (amount != 0)
            {
                std::cout << "The initial asset amount created must be 0." << std::endl;
                DEBUGLOG("The initial asset amount created must be 0.");
                amount = 0;
            }
            CTxUtxos* utxo = tx.add_utxos();
            if (populateUtxoData(utxo, addr, amount, assetType) != 0) {
                ERRORLOG("Failed to create UTXO for asset type: {}", assetType);
                return false;
            }
        }
        
        // Set the transaction type and hash
        tx.set_txtype((uint32_t)global::ca::TxType::kGenesisTxType);
        tx.set_hash(Getsha256hash(tx.SerializeAsString()));
        
        return true;
    }
    catch (const std::exception& e) {
        ERRORLOG("Failed to create genesis transaction: {}", e.what());
        return false;
    }
}

int GenesisBlockGenerator::populateUtxoData(CTxUtxos* utxo, const std::string& addr, 
                                           const uint64_t balance, const std::string& assetType) {
    try {
        // Use the global default decimal configuration
        const uint64_t kDecimalNum = global::ca::kDecimalNum;
        
        utxo->set_assettype(assetType);
        
        utxo->add_owner(addr);
        
        CTxInput* txin = utxo->add_vin();
        
        // set prevout
        CTxPrevOutput* prevOut = txin->add_prevout();
        prevOut->set_hash(std::string(64, '0'));
        prevOut->set_n(0);
        txin->set_sequence(0);
        
        // add vout
        CTxOutput* txout = utxo->add_vout();
        txout->set_value(balance * kDecimalNum);
        txout->set_addr(addr);
        
        return 0;
    }
    catch (const std::exception& e) {
        ERRORLOG("Failed to create simple UTXO: {}", e.what());
        return -1;
    }
}

bool GenesisBlockGenerator::ValidateGenesisBlock(const CBlock& block) {
    try {
        const auto& configManager = GenesisConfigManager::GetInstance();
        if (!configManager.IsConfigLoaded()) {
            ERRORLOG("Network config not loaded");
            return false;
        }
        
        const auto& genesisInfo = configManager.GetGenesisInfo();
        
        // Verify the basic information of the block
        if (block.height() != 0) {
            ERRORLOG("Invalid genesis block height: {}", block.height());
            return false;
        }
        
        if (block.time() != genesisInfo.genesisTime) {
            ERRORLOG("Invalid genesis block time: {}", block.time());
            return false;
        }
        
        if (block.prevhash() != std::string(64, '0')) {
            ERRORLOG("Invalid genesis block prev hash");
            return false;
        }
        
        // Verify the transaction quantity
        if (block.txs_size() != 1) {
            ERRORLOG("Invalid genesis block transaction count: {}", block.txs_size());
            return false;
        }
        
        // Verify the transaction
        const CTransaction& tx = block.txs(0);
        if (tx.identity() != genesisInfo.initAccountAddr) {
            ERRORLOG("Invalid genesis transaction identity");
            return false;
        }
        
        if (tx.type() != global::ca::GENESIS_SIGN) {
            ERRORLOG("Invalid genesis transaction type");
            return false;
        }
        
        // Verify the number of UTXOs
        if (tx.utxos_size() != genesisInfo.initialBalance.size()) {
            ERRORLOG("Invalid genesis transaction UTXO count");
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        ERRORLOG("Failed to validate genesis block: {}", e.what());
        return false;
    }
}

bool GenesisBlockGenerator::ValidateConfigGenesisHash() {
    try {
        const auto& configManager = GenesisConfigManager::GetInstance();
        if (!configManager.IsConfigLoaded()) {
            ERRORLOG("Network config not loaded");
            return false;
        }
        
        const auto& networkInfo = configManager.GetNetworkInfo();
        
        // Only perform strict hash verification on the primary network.
        if (networkInfo.type == "primary") {
            // Generate the genesis block hash based on the configuration parameters
            std::string generatedHash = GetGenesisBlockHash();
            if (generatedHash.empty()) {
                ERRORLOG("Failed to generate genesis block hash from config");
                return false;
            }
            
            if (generatedHash != global::ca::kGenesisBlockHash) {
                ERRORLOG("Primary network genesis block hash mismatch: expected={}, generated={}", 
                         global::ca::kGenesisBlockHash, generatedHash);
                return false;
            }
            
            INFOLOG("Primary network genesis block validation successful. Hash: {}", generatedHash);
        } else {
            // For the test and dev networks, only basic configuration verification is conducted, without verifying the hashes.
            INFOLOG("Non-primary network ({}), skipping hash validation for flexibility", networkInfo.type);
        }
        
        return true;
    }
    catch (const std::exception& e) {
        ERRORLOG("Failed to validate genesis block hash: {}", e.what());
        return false;
    }
}

std::string GenesisBlockGenerator::GetGenesisBlockHash() {
    CBlock block;
    if (!GenerateGenesisBlock(block)) {
        return "";
    }
    return block.hash();
}

} // namespace ca 