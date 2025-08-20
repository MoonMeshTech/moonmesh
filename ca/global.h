#ifndef CA_GLOBAL_HEADER
#define CA_GLOBAL_HEADER
#include <boost/multiprecision/cpp_bin_float.hpp>

#include <unordered_set>

#include "common/global.h"
#include "proto/ca_protomsg.pb.h"
#include "utils/timer.hpp"
#include "ca/genesis_config.h"
#include "ca/genesis_block_generator.h"

namespace global{

    namespace ca{

        extern const std::string kInitialAccountAddress;
        extern const uint64_t kGenesisTime ;
        extern const std::string kConfigJson ;

        extern const std::string kGenesisBlockHash ;
        // consensus
        extern const int kConsensus ;
        // const int randomNodeGroup = 1; 
        extern const int send_node_threshold ;
        extern const int KReSend_node_threshold ;
        extern const uint32_t MIN_SYNC_QUAL_NODES;

        extern const int TX_TIMEOUT_MIN ;
                                       
        // timer
        extern CTimer BLOCK_POOL_TIMER_INTERVAL;
        extern CTimer SEEK_BLOCK_TIMER;
        extern CTimer DATABASE_TIMER;

        // mutex
        extern std::mutex bonusMutex;
        extern std::mutex kDelegatingMutex;
        extern std::mutex BURN_MUTEX;

        // ca
        extern const uint64_t kDecimalNum ;
        extern const double   MIN_DOUBLE_CONSTANT_PRECISION ;
        extern const uint32_t LOCK_AMOUNT ;

        extern const uint64_t MIN_STAKE_AMOUNT ;
        extern const uint64_t kMinKGetBonusDelegatingAmt ;
        extern const uint64_t kMinDelegatingAmt ;
        extern const std::string GENESIS_SIGN ;
        extern const std::string kTxSign ;
        extern const std::string kTargetAddress ;
        extern const std::string kVirtualDelegatingAddr ;
        extern const std::string VIRTUAL_BURN_GAS_ADDR ;
        extern const std::string STAKE_TYPE_NET ;
        extern const std::string kdelegateTypeNormal ;
        extern const uint64_t MIN_UNSTAKE_HEIGHT ;
        extern const std::string kVirtualDeploymentContractAddress ;
        extern const std::string VIRTUAL_CALL_CONTRACT_ADDR ;
        extern const std::string kVirtualCallFlowOutAddr ;
        extern const std::string kLockTypeNet ;
        extern const std::string VIRTUAL_LOCK_ADDRESS ;
		
        extern const uint64_t KtxTimeout ;

        extern const double MAX_COMMISSION_RATE ;
        extern const double MIN_COMMISSION_RATE ;

        extern const  std::string ASSET_TYPE_VOTE ;
        extern const  std::string assetType_MM ;

        enum class StakeType
        {
            STAKE_TYPE_UNKNOWN = 0,
            STAKE_TYPE_NODE = 1
        };
        
        // Transacatione Type
        enum class TxType
        {
            kGenesisTxType = -1,
            TX_TYPE_UNKNOWN, 
            TX_TYPE_TX, 
            kTransactionTypeStake, 
            kTxTypeUnstake_, 
            kTxTypeDelegate, 
            kTxTypeUndelegate, 
            TX_TYPE_DECLARATION, 
            kTransactionTypeDeploy, 
            TX_TYPE_INVOKE_CONTRACT,  
            kTxTypeLock,            
            kTxTypeUnLock,          
            KTXTypeProposal,        
            KTXTyRevokeProposal,    
            KTXTyVote,             
            kTXTypeFund = 98,  
            TX_TYPE_BONUS = 99
        };

        // Sync
        enum class SaveType
        {
            SyncNormal,
            SyncFromZero,
            Broadcast,
            Unknow
        };

        enum class blockMean
        {
            Normal,
            processByPreHash,
            ByUtxo
        };
        extern const uint64_t hashRangeSum;
        extern const uint64_t sumHashRange;

        namespace DoubleSpend {
            const int SingleBlock = -66;
            const int DoubleBlock = -99;
        };

        extern const uint32_t KVoteCycle;
        extern const uint32_t KVrfVoteBeginTime;
        extern const boost::multiprecision::cpp_bin_float_100 KLowestExchangeRate;
        
        enum class VoteType
        {
            Against = 0,
            Approve = 1,  
            Count 
        };

        extern const uint8_t KProposalInfoVersion;
        extern const uint8_t KRevokeProposalInfoVersion;

        // contract
        enum VmType {
            EVM
        };
        //test
        extern std::atomic<uint64_t> TxNumber;
        extern const uint32_t CURRENT_TRANSACTION_VERSION;
        extern const uint32_t kCurrentBlockVersion;

        inline uint64_t GetGenesisTime()
        {
            if (global::GetBuildType() != GenesisConfig::BuildType::BUILD_TYPE_PRIMARY)
            {
                return GenesisConfig::GenesisConfigManager::GetInstance().GetGenesisTime();
            }
            return kGenesisTime;
        }
        inline std::string GetInitAccountAddr()
        {
            if (global::GetBuildType() != GenesisConfig::BuildType::BUILD_TYPE_PRIMARY)
            {
                return GenesisConfig::GenesisConfigManager::GetInstance().GetInitAccountAddr();
            }
            return kInitialAccountAddress;
        }
    }
}


#endif
