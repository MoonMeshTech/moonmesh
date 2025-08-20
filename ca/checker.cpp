#include "ca/checker.h"
#include "utils/contract_utils.h"

bool Checker::CheckConflict(const CTransaction &tx, const std::vector<TransactionEntity>  &cache)
{
    for(const auto& txEntity : cache)
    {
        if(CheckConflict(txEntity.GetTransaction(), tx))
        {
            return true;
        }
    }

    return false;
}

bool Checker::CheckConflict(const CTransaction &tx, const std::set<CBlock, compator::blockTimeAscending> &blocks)
{
    for (const auto& block : blocks)
    {
        for(const auto& curTx : block.txs())
        {
            if(GetTransactionType(tx) != kTransactionTypeTx)
            {
                continue;
            }

            if(CheckConflict(curTx, tx) == true)
            {
                return true;
            }
        }
    }

    return false;
}

bool Checker::CheckConflict(const CBlock &block, const std::set<CBlock, compator::blockTimeAscending> &blocks, std::string* txHashRef)
{
    for (const auto& currentBlock : blocks)
    {
        if(txHashRef != NULL)
        {
            std::string txHash = "";
            if(CheckConflict(currentBlock, block, &txHash) == true)
            {
                *txHashRef = txHash;
                return true;
            }
        }
        else
        {
            if(CheckConflict(currentBlock, block) == true)
            {
                return true;
            }
        }
    }

    return false;
}

bool Checker::CheckConflict(const CBlock &block1, const CBlock &block2, std::string* txHashRef)
{
    for(const auto& tx1 : block1.txs())
    {
        if(GetTransactionType(tx1) != kTransactionTypeTx)
        {
            continue;
        }

        for(const auto& tx2 : block2.txs())
        {
            if(GetTransactionType(tx2) != kTransactionTypeTx)
            {
                continue;
            }

            if(CheckConflict(tx1, tx2) == true)
            {
                if(txHashRef != NULL)
                {
                    CTransaction copyTx = tx1;
                    copyTx.clear_hash();
                    copyTx.clear_verifysign();
                    *txHashRef = Getsha256hash(copyTx.SerializeAsString());
                }
                return true;
            }
        }
    }

    return false;
}

bool Checker::CheckConflict(const CTransaction &tx1, const CTransaction &tx2)
{
    std::vector<std::string> vec1;
    for(const auto& utxo : tx1.utxos())
    {
        std::string currency = utxo.assettype();
        for(const auto& vin : utxo.vin())
        {
            for (auto & prevout : vin.prevout())
            {
                if(!vin.contractaddr().empty()){
                    DEBUGLOG("---- 1 utxo : {}", prevout.hash());
                    vec1.push_back(prevout.hash());
                }else{
                    DEBUGLOG("---- 1 prevout_addr : {}",prevout.hash() + "_" + GenerateAddr(vin.vinsign().pub()));
                    vec1.push_back(prevout.hash() + "_" + GenerateAddr(vin.vinsign().pub()) + "_" + currency);
                }
            }
        }
    }

    std::vector<std::string> vec2;
    for(const auto& utxo : tx2.utxos())
    {
        std::string currency = utxo.assettype();
        for(const auto& vin : utxo.vin())
        {
            for (auto & prevout : vin.prevout())
            {
                if(!vin.contractaddr().empty()){
                    DEBUGLOG("---- 2 utxo : {}", prevout.hash());
                    vec2.push_back(prevout.hash());
                }else{
                    DEBUGLOG("---- 2 prevout_addr : {}",prevout.hash() + "_" + GenerateAddr(vin.vinsign().pub()));
                    vec2.push_back(prevout.hash() + "_" + GenerateAddr(vin.vinsign().pub()) + "_" + currency);
                }
            }
        }
    }

    std::vector<std::string> vecIntersection;
    std::sort(vec1.begin(), vec1.end());
    std::sort(vec2.begin(), vec2.end());
    std::set_intersection(vec1.begin(), vec1.end(), vec2.begin(), vec2.end(), std::back_inserter(vecIntersection));
    return !vecIntersection.empty();
}

void Checker::CheckConflict(const CBlock &block, std::vector<CTransaction> &double_spent_transactions)
{
    std::map<std::string, std::vector<CTransaction>> transactionPool;
    for (const auto &tx : block.txs())
    {
        global::ca::TxType txType = (global::ca::TxType)tx.txtype();
        for (const auto &txUtxo : tx.utxos())
        {
            std::string currency = txUtxo.assettype();
            for (const auto &vin : txUtxo.vin())
            {
                for (auto &prevout : vin.prevout())
                {
                    std::string &&utxo = prevout.hash() + "_" + GenerateAddr(vin.vinsign().pub()) + "_" + currency;
                    if (transactionPool.find(utxo) != transactionPool.end() && global::ca::TxType::kTxTypeUnstake_ == txType || global::ca::TxType::kTxTypeUndelegate == txType || global::ca::TxType::kTxTypeUnLock == txType)
                    {
                        continue;
                    }
                    transactionPool[utxo].push_back(tx);
                }
            }
        }
    }
    for (auto &iter : transactionPool)
    {
        if (iter.second.size() > 1)
        {
            std::sort(iter.second.begin(), iter.second.end(), [](const CTransaction &a, const CTransaction &b)
                        { return a.time() < b.time(); });
            double_spent_transactions.insert(double_spent_transactions.end(), iter.second.begin() + 1, iter.second.end());
        }
    }
}
