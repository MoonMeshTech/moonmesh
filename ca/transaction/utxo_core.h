#pragma once
#include "proto/transaction.pb.h"
#include "utxo_error.h"
#include "txhelper.h"
#include "utils/tmp_log.h"                                                                            
#include <cstdint>
#include "CowString.hpp"
#include "expected.h"







struct UTxoRulers{
    static const uint64_t max_vin_size=10000;//Prevent performance degradation caused by too many UTXO VINs
};



 namespace mmc
{

class UtxoCore
{
public:
    struct OtherToPayGas{
        mmc::CowString addr;
        mmc::CowString asset_type;
    };

    UtxoCore() : tx_() {}
    virtual ~UtxoCore(){}

    void print_debug_tx(CTransaction * tx=nullptr);
    virtual mmc::Expected<bool> trans()=0;
    mmc::Expected<bool> signTx(const mmc::CowString & addr);
protected:
    struct TransferToParam{
        mmc::CowString to;
        int64_t amount; 
    };

    struct TransferFromParam{
        mmc::CowString from;
        mmc::CowString asset_type;
        int64_t amount;
        bool is_find_utxo=false;
    };

    struct UtxoGasParam{
        mmc::CowString from;
        mmc::CowString asset_type;
        int64_t gas;
    };

    void createUtxoTransferTo(CTxUtxos *utxo,const TransferToParam &param);
    
    mmc::Expected<int64_t> createUtxoTransferFrom(CTxUtxos * utxo,const TransferFromParam & param,int64_t sq);

    mmc::Expected<int64_t> getStakeAmount(const mmc::CowString &addr, const mmc::CowString &assetType);

    mmc::Expected<bool> signUtxo(const mmc::CowString & addr,CTxUtxos * utxo);

   


    CTransaction * getTx(){
        return &tx;
    }
    void setTx(const CTransaction & tx_){
        tx=tx_;
    }
protected:
    virtual mmc::Expected<int64_t> createUtxoGas(CTransaction * tx,const UtxoGasParam & param);
    mmc::Expected<int64_t> createUtxoGasBylameda(CTransaction *tx, const UtxoGasParam &param,const std::function<uint64_t(CTxUtxos *u,int index)> & gasLamda) ;

    struct SingRe{
        mmc::CowString signature;
        mmc::CowString pubkey;
    };
    mmc::Expected<SingRe> sign(const mmc::CowString &addr,const mmc::CowString & data);

    virtual mmc::Expected<bool> makeTxInfor(CTransaction * tx)=0;

    virtual  mmc::Expected<bool> checkParams()=0;

    virtual mmc::Expected<int64_t> getGasValue(CTxUtxos * u,int index)=0;

    virtual mmc::Expected<bool> goMessage()=0;

protected:
    std::string calculate_all_hash(CTransaction * tx);

    CTransaction tx;
protected:
    
    struct FindUtxoParam{
        mmc::CowString addr;
        mmc::CowString asset_type;
        int64_t need_amount;
        bool need_wid_utxo;
    };
    mmc::Expected<std::vector<TxHelper::Utxo>> findUtxoByAddrAndCheckBalance(const FindUtxoParam & param,int64_t & _total);

    mmc::Expected<uint64_t> transactionDiscoveryHeight();


    int64_t GetChainId();

    int checkGasAssetsQualification(const std::pair<std::string, std::string> &gasAssets, const uint64_t &height);

    CTransaction * tx_; // Pointer to the transaction object

};

}


// #define DEFINITION_PARAM(CLASSNAME)\
//     struct CLASSNAME##Param

// #define DECLARATION_CREF(CLASSNAME) \
// private:\
//     CLASSNAME(){}\
//     CLASSNAME##Param s_##CLASSNAME##_param; \
// public:\
//   static CLASSNAME Create(const CLASSNAME##Param & s_param){ \
//     CLASSNAME t;\
//     t.s_##CLASSNAME##_param = s_param;return t; \
// }\
//     ~CLASSNAME(){}