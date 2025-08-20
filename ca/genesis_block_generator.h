#ifndef __GENESIS_BLOCK_GENERATOR_H__
#define __GENESIS_BLOCK_GENERATOR_H__

#include <string>
#include "ca/genesis_config.h"
#include "proto/block.pb.h"
#include "proto/transaction.pb.h"
#include "proto/ca_protomsg.pb.h"
#include "utils/account_manager.h"

namespace GenesisConfig
{

    class GenesisBlockGenerator
    {
    public:
        static GenesisBlockGenerator &GetInstance();

        // Generate the genesis block
        bool GenerateGenesisBlock(CBlock &block);

        // Generate the serialized string of the genesis block sequence
        std::string GenerateGenesisBlockRaw();

        // Verify the genesis block
        bool ValidateGenesisBlock(const CBlock &block);

        // Verify the genesis block hash in the configuration file
        bool ValidateConfigGenesisHash();

        // Obtain the hash of the genesis block
        std::string GetGenesisBlockHash();

    private:
        GenesisBlockGenerator() = default;
        ~GenesisBlockGenerator() = default;
        GenesisBlockGenerator(const GenesisBlockGenerator &) = delete;
        GenesisBlockGenerator &operator=(const GenesisBlockGenerator &) = delete;

        bool CreateGenesisTransaction(CTransaction &tx);

        int populateUtxoData(CTxUtxos *utxo, const std::string &addr,
                         const uint64_t balance, const std::string &assetType);
    };

} // namespace ca

#endif // __GENESIS_BLOCK_GENERATOR_H__ 