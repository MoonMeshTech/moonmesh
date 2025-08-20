/**
 * *****************************************************************************
 * @file        txhelper.h
 * @brief       
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */

#ifndef TX_HELPER_HEADER_GUARD
#define TX_HELPER_HEADER_GUARD

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <iostream>
#include <nlohmann/json.hpp>
#include <google/protobuf/util/json_util.h>
#include <evmc/evmc.h>
#include <boost/circular_buffer.hpp>
#include "proto/ca_protomsg.pb.h"
#include "proto/transaction.pb.h"
#include "global.h"
#include "db/db_api.h"

class EvmHost;
/**
 * @brief       
 */
class TxHelper
{
public:
    struct ProposalInfo
    {
        ProposalInfo():
        version(0),
        BeginTime(0),
        EndTime(0),
        Name(""),
        ExchangeRate(""),
        ContractAddr(""),
        canBeStake(0),
        MIN_VOTE_NUM(0),
        Title(""),
        Identifier("")
        {
        }

        int version;
        uint64_t BeginTime;
        uint64_t EndTime;
        std::string Name;
        std::string ExchangeRate;
        std::string ContractAddr;
        uint64_t canBeStake;
        uint64_t MIN_VOTE_NUM;
        std::string Title;
        std::string Identifier;
    };

    struct VoteInfo
    {
        uint32_t TxType;
        uint64_t BeginTime;
        uint64_t EndTime;
        std::string AssetName;
        std::string ExchangeRate;
        std::string ContractAddr;
        std::string ProposalHash;
        uint64_t MIN_VOTE_NUM;
    };


    struct Utxo
    {
        std::uint64_t value;
        std::string addr;
        std::string hash;
        std::uint32_t n;
    };

    struct TransTable
    {
        std::string assetType;
        std::vector<std::string> fromAddr;
        std::map<std::string, int64_t> toAddrAmount;
    };

    class UnspentTxOutputComparator
    {
    public:
        bool operator()(const Utxo& utxo1, const Utxo& utxo2) const
        {
            return utxo1.value < utxo2.value;
        }
    };

    typedef enum emStakeType {
        STAKE_TYPE_UNKNOWN = -1,		// unknown
        STAKE_TYPE_NODE = 0,			// Node stake
    } stakeType;

     typedef enum emdelegateType {
        kdelegateType_Unknown = -1,		// unknown
        kdelegateType_NetLicence = 0,	    //NetLicence
    } delegateType;

    enum vrfAgentType
    {
        vrfAgentType_defalut = 0, 
        vrfAgentType_vrf,
        vrfAgentType_local ,
    };

    typedef enum emLockType {
        LOCK_UNKNOWN = -1,		// unknown
        kLock_Node = 0,			// Node lock
    } LockType;

    static const uint32_t MAX_VIN_SIZE;

    TxHelper() = default;
    ~TxHelper() = default;

    /**
     * @brief       
     * 
     * @param       tx: 
     * @param       txUtxoHeight: 
     * @return      int 
     */
    static int get_tx_utxo_height(const CTransaction &tx, uint64_t& txUtxoHeight);

    /**
     * @brief       Get the Tx Owner object
     * 
     * @param       tx: 
     * @return      std::vector<std::string> 
     */
    static std::set<std::string> getTransactionOwner(const CTransaction& tx);
    /**
     * @brief       Get the Utxos object
     * 
     * @param       address: 
     * @param       utxos: 
     * @return      int 
     */
    static int GetUtxos(const std::string & address, std::vector<TxHelper::Utxo>& utxos);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       height: 
     * @return      int 
     */
    static int Check(const std::vector<std::string>& fromAddr,const std::string& type ,uint64_t height);
    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       requiredUtxoAmount: 
     * @param       total: 
     * @param       outputUtxosSet: 
     * @return      int 
     */
    static uint64_t calculate_utxo_value(const std::string& balance);
    static int FindUtxo(const std::multimap<std::string, std::string>& fromAddr,const uint64_t requiredUtxoAmount,
						uint64_t& total,std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator>& outputUtxosSet, bool isFindUtxo);
    static int FindUtxo(const std::pair<std::string, std::string> addressCurrencyMapping,
						const uint64_t kMaxUtxoSize,const uint64_t expend,
						uint64_t& total, const uint64_t tempGas,
						std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator>& outputUtxosSet, 
						bool isFindUtxo,const std::string& sender);


    static int sendConfirmUtxoHashRequest(const std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator>& outputUtxosSet);

    /**
     * @brief       Create a Tx Transaction object
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       height: 
     * @param       outTx: 
     * @param       type: 
     * @param       information: 
     * @return      int 
     */
    static int createTransactionRequest(const std::vector<TxHelper::TransTable> txAsset, std::pair<std::string, std::string> &gasTrade, bool isGasTrade,
                                         const std::string &encodedInfo, uint64_t height, CTransaction &outTx,
                                         TxHelper::vrfAgentType &type, bool isFindUtxo);

    /**
     * @brief       Create a Stake Transaction object
     * 
     * @param       fromAddr: 
     * @param       stakeAmount: 
     * @param       height: 
     * @param       stakeType: 
     * @param       outTx: 
     * @param       outVin: 
     * @param       type: 
     * @param       information: 
     * @param       commission: commission percentage (0.05 - 0.2)
     * @return      int 
     */
    static int CreateStakingTransaction(const std::string & fromAddr,const std::string & assetType,uint64_t stakeAmount,uint64_t height,
                    TxHelper::stakeType stakeType,CTransaction &outTx,
                    std::vector<TxHelper::Utxo> & outVin,TxHelper::vrfAgentType &type,const std::pair<std::string,std::string>& gasTrade,bool isGasTrade,
                     double commissionRate, bool isFindUtxo, const std::string& encodedInfo);


    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @param       utxoHash: 
     * @param       height: 
     * @param       outTx: 
     * @param       outVin: 
     * @param       type: 
     * @param       information: 
     * @return      int 
     */
    static int CreateUnstakeTransaction(const std::string &fromAddr, const std::string & assetType,const std::string &utxoHash, uint64_t height,
                                              CTransaction &outTx, std::vector<TxHelper::Utxo> &outVin, TxHelper::vrfAgentType &type,
                                              const std::pair<std::string, std::string> &gasTrade, bool isGasTrade, bool isFindUtxo, const std::string &encodedInfo);

    /**
     * @brief       Create a Delegating Transaction object
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       delegateAmount: 
     * @param       height: 
     * @param       delegateType: 
     * @param       outTx: 
     * @param       outVin: 
     * @param       type: 
     * @param       information: 
     * @return      int 
     */
    static int CreateDelegatingTransaction(const std::string & fromAddr, const std::string assetType, const std::string& toAddr,
                                    uint64_t delegateAmount, uint64_t height, TxHelper::delegateType delegateType,CTransaction & outTx,
                                    std::vector<TxHelper::Utxo> & outVin,TxHelper::vrfAgentType &type,const std::pair<std::string,std::string>& gasTrade,bool isGasTrade,
                                    bool isFindUtxo, const std::string& encodedInfo);
    /**
     * @brief       Create a Undelegating Transaction object
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       utxoHash: 
     * @param       height: 
     * @param       outTx: 
     * @param       outVin: 
     * @param       type: 
     * @param       information: 
     * @return      int 
     */

    static int CreateUndelegatingTransaction(const std::string& fromAddr,const std::string &assetType, const std::string& toAddr,
                                    const std::string& utxoHash,uint64_t height,CTransaction& outTx,
                                    std::vector<TxHelper::Utxo> & outVin,TxHelper::vrfAgentType &type,const std::pair<std::string,std::string>& gasTrade,bool isGasTrade,
                                    bool isFindUtxo, const std::string& encodedInfo);
    
    /**
     * @brief       Create a Bonus Transaction object
     * 
     * @param       Addr: 
     * @param       height: 
     * @param       outTx: 
     * @param       outVin: 
     * @param       type: 
     * @param       information: 
     * @return      int 
     */
    static int CreateBonusTransaction(const std::string &Addr, const std::string assetType,uint64_t height, CTransaction &outTx,
                                      std::vector<TxHelper::Utxo> &outVin, TxHelper::vrfAgentType &type,
                                      const std::pair<std::string, std::string> &gasTrade, bool isGasTrade, bool isFindUtxo, const std::string &encodedInfo,bool firstChoose);

    static int make_evm_deploy_contract_transaction(CTransaction &outTx, std::vector<std::string> &dirtyContract, const std::pair<std::string, std::string> &gasTrade,
                                                TxHelper::vrfAgentType &type, EvmHost &host,
                                                evmc_message &message, bool isFindUtxo, const std::string &encodedInfo,
                                                std::function<int(CTransaction &, std::string &)> signFunction);

    static int
    makeEvmCallContractTransaction(CTransaction &outTx, std::vector<std::string> &dirtyContract,
                                   const std::pair<std::string, std::string> &gasTrade,
                                   TxHelper::vrfAgentType &type, EvmHost &host,
                                   evmc_message &message, const std::string &strInput,
                                   const uint64_t contractTransfer, const std::string &toAddr, bool isFindUtxo,
                                   const std::string &encodedInfo,
                                   std::function<int(CTransaction &, std::string &)> signFunction,bool isFlowOutGasTrade);

    /**
     * @brief       Create a Evm Deploy Contract Transaction object
     * 
     * @param       fromAddr: 
     * @param       contractOwnerEvmAddress: 
     * @param       code: 
     * @param       height: 
     * @param       contractInfo: 
     * @param       outTx: 
     * @param       type: 
     * @param       information: 
     * @return      int 
     */

    static int createEvmDeployContractTransactionRequest(uint64_t height, CTransaction &outTx, const std::pair<std::string, std::string> &gasTrade, std::vector<std::string> &dirtyContract,
                                                  TxHelper::vrfAgentType &type, std::string &code,
                                                  const std::string &from, uint64_t transfer, const std::string &to,
                                                  const std::string &encodedInfo, bool isFindUtxo = false, bool isRpc = false);



    /**
     * @brief       Create a Evm Call Contract Transaction object
     * 
     * @param       fromAddr: 
     * @param       toAddr: 
     * @param       strInput:
     * @param       contractOwnerEvmAddress: 
     * @param       height: 
     * @param       outTx: 
     * @param       type: 
     * @param       information: 
     * @param       contractTip: 
     * @param       contractTransfer: 
     * @return      int 
     */
    static int
    createEvmCallContractTransactionRequest(const std::string &from, const std::string &toAddr, const std::string &strInput,
                                     const std::string &encodedInfo, const std::pair<std::string, std::string> &gasTrade,
                                     uint64_t height, CTransaction &outTx, TxHelper::vrfAgentType &type,
                                     const uint64_t contractTransfer,
                                     std::vector<std::string> &dirtyContract, const std::string &to, 
                                     bool isFindUtxo = false, bool isRpc = false,bool isFlowOutGasTrade = false);

    int evmCallContractTransactionCreationRequest(const std::string &from, const std::string &toAddr,
                                               const std::string &strInput,const std::string &encodedInfo, const std::pair<std::string, std::string> &gasTrade,
                                               uint64_t height, CTransaction &outTx, TxHelper::vrfAgentType &type,
                                               const uint64_t contractTransfer,
                                               std::vector<std::string> &dirtyContract, const std::string &to,
                                               bool isFindUtxo,bool isRpc);
    /**
     * @brief       
     * 
     * @param       addr: 
     * @param       tx: 
     * @return      int 
     */
    static int addMultipleSigns(const std::string & addr, CTxUtxos &tx);

    /**
     * @brief       
     * 
     * @param       addr: 
     * @param       tx: 
     * @return      int 
     */
    static int addVerifySign(const std::string & addr, CTransaction &tx);

    /**
     * @brief       
     * 
     * @param       addr: 
     * @param       message: 
     * @param       signature: 
     * @param       pub: 
     * @return      int 
     */
    static int Sign(const std::string & addr, const std::string & message, 
                    std::string & signature, std::string & pub);

    /**
     * @brief       
     * 
     * @param       fromAddr: 
     * @return      true 
     * @return      false 
     */
    static bool requiresAgent(const std::vector<std::string> & fromAddr);

    /**
     * @brief       
     * 
     * @param       tx: 
     * @return      true 
     * @return      false 
     */
    static bool requiresAgent(const CTransaction &tx);

    /**
     * @brief       Whether the time gap with the current highly recent block is within timeout
     * 
     * @param       txTime: 
     * @param       timeout: 
     * @param       preHeight: 
     * @return      true 
     * @return      false 
     */
    static bool checkTransactionTimeout(const uint64_t & txTime, const uint64_t & timeout,const uint64_t & preHeight);

    /**
     * @brief
     * 
     * @param       tx: 
     * @param       preHeight: 
     * @return      TxHelper::vrfAgentType 
     */
    static TxHelper::vrfAgentType get_vrf_agent_type(const CTransaction &tx, uint64_t &preHeight);

    /**
     * @brief       Get the Tx Start Identity object
     * 
     * @param       fromaddr: 
     * @param       height: 
     * @param       currentTime: 
     * @param       type: 
     */
    static void getTransactionStartIdentity(const uint64_t &height,const uint64_t &currentTime,TxHelper::vrfAgentType &type);

    /**
    * @brief       Get the Eligible Nodes object
    * 
    * @return      std::string 
    */
    static std::string GetEligibleNodes();

    static std::string fetchIdentityNodes();
    static int fetchIdentityNodes(TxHelper::vrfAgentType &type, const std::string &txData, const uint64_t txTime, std::string &identity);
    
    static int createLockTransaction(const std::string & fromAddr, uint64_t lockAmount,uint64_t height,
			TxHelper::LockType lockType, CTransaction & outTx, std::vector<TxHelper::Utxo> & outVin,
			TxHelper::vrfAgentType &type ,const std::pair<std::string,std::string>& gasTrade,bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo);

    static int createUnlockTransactionRequest(const std::string& fromAddr,const std::string& utxoHash,uint64_t height,
            CTransaction &outTx, std::vector<TxHelper::Utxo> & outVin,
            TxHelper::vrfAgentType &type ,const std::pair<std::string,std::string>& gasTrade,bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo);
    
    static int CreateProposalTransaction(const std::string& fromAddr,             
            uint64_t height,CTransaction& outTx,TxHelper::vrfAgentType &type,
            const std::pair<std::string,std::string>& gasTrade, std::string& identifier, std::string& title,
            uint64_t beinTime, uint64_t endTime, std::string& assetName, std::string& rate, std::string& contractAddr, uint64_t minVote, bool isFindUtxo, const std::string& encodedInfo);

    static int CreateRevokeProposalTransaction(const std::string& fromAddr,             
            uint64_t height,CTransaction& outTx,TxHelper::vrfAgentType &type,
            const std::pair<std::string,std::string>& gasTrade, 
            uint64_t beinTime, uint64_t endTime, std::string proposalHash, uint64_t minVote, bool isFindUtxo, const std::string& encodedInfo);
    
    static int CreateVoteTransaction(const std::string& fromAddr,             
            uint64_t height,CTransaction& outTx,TxHelper::vrfAgentType &type,
            const std::pair<std::string,std::string>& gasTrade, 
            const std::string& voteHash, const int pollType, const global::ca::TxType& voteTxType, bool isFindUtxo, const std::string& encodedInfo);
            
    static int CreateFundTransaction(const std::string& addr,
										uint64_t height,
										CTransaction& outTx,
										std::vector<TxHelper::Utxo> & outVin,
										TxHelper::vrfAgentType &type,const std::pair<std::string,std::string>& gasTrade,
										bool isGasTrade,bool isFindUtxo, const std::string& encodedInfo);                                               

};

#endif



