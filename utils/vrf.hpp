#ifndef _VRF_H_
#define _VRF_H_

#include "account_manager.h"
#include "utils/timer.hpp"
#include "utils/tmp_log.h"
#include <cstdint>
#include <shared_mutex>

constexpr int cacheExpireTime = 300000000;

class VRF
{
    public:
        VRF() {
            ClearTimer.AsyncLoop(3000, [&]() {
                 uint64_t time_ = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
                {
                    std::unique_lock<std::shared_mutex> lck(vrfInfoMutex_);
                   
                    auto v1Iter = vrf_cache_data.begin();
                   
                    
                    for (; v1Iter != vrf_cache_data.end();) {
                        if ((time_ - v1Iter->second.second) > cacheExpireTime) {
                            DEBUGLOG("vrf_cache_data erase hash : {}  time", v1Iter->first, v1Iter->second.second);
                            v1Iter= vrf_cache_data.erase(v1Iter);
                        } else {
                            v1Iter++;
                        }
                    }
                    auto v2Iter = tx_vrf_cache_.begin();
                    for (; v2Iter != tx_vrf_cache_.end();) {
                        if ((time_ - v2Iter->second.second) > cacheExpireTime) {
                           DEBUGLOG("tx_vrf_cache_ erase hash : {}  time", v2Iter->first, v2Iter->second.second);
                           v2Iter= tx_vrf_cache_.erase(v2Iter);
                        } else {
                            v2Iter++;
                        }
                    }
                }

                {
                    std::unique_lock<std::shared_mutex> lck(vrfNodeMutex_);
                    auto v3Iter = vrfVerificationNodeRequest.begin();
                    for(;v3Iter!=vrfVerificationNodeRequest.end();){
                        if((time_ - v3Iter->second.second ) > cacheExpireTime){
                            DEBUGLOG("tx_vrf_cache_ erase hash : {}  time", v3Iter->first, v3Iter->second.second);
                            v3Iter= vrfVerificationNodeRequest.erase(v3Iter);
                        }else{
                            v3Iter++;
                        }
                    }

                    auto v3Iter_ = blockSignatureCacheData.begin();
                    for(;v3Iter_!=blockSignatureCacheData.end();){
                        if((time_ - v3Iter_->second.second ) > cacheExpireTime){
                            DEBUGLOG("tx_vrf_cache_ erase hash : {}  time", v3Iter_->first, v3Iter_->second.second);
                            v3Iter_= blockSignatureCacheData.erase(v3Iter_);
                        }else{
                            v3Iter_++;
                        }
                    }
                }
            
            });
        }
        ~VRF() = default;

        /**
         * @brief       
         * 
         * @param       pkey:
         * @param       input:
         * @param       output:
         * @param       proof:
         * @return      int
        */
        int CreateVRF(EVP_PKEY* pkey, const std::string& input, std::string & output, std::string & proof)
        {
            std::string hash = Getsha256hash(input);
	        if(ed25519SignMessage(hash, pkey, proof) == false)
            {
                return -1;
            }

            output = Getsha256hash(proof);
            return 0;
        }
        /**
         * @brief       
         * 
         * @param       pkey:
         * @param       input:
         * @param       output:
         * @param       proof:
         * @return      int
        */
        int VerifyVRF(EVP_PKEY* pkey, const std::string& input, std::string & output, std::string & proof)
        {
            std::string hash = Getsha256hash(input);
            if(ed25519VerificationMessage(hash, pkey, proof) == false)
            {
                return -1;
            }

            output = Getsha256hash(proof);
            return 0;
        }

        /**
         * @brief       
         * 
         * @param       data:
         * @param       limit:
         * @return      int
        */
        int GetRandNum(std::string data, uint32_t limit)
        {
            auto value = stringToll(data);
            return  value % limit;
        }
        /**
         * @brief       
         * 
         * @param       data:
         * @return      double
        */
        double GetRandNum(const std::string& data)
        {
            auto value = stringToll(data);
            return  double(value % 100) / 100.0;
        }
        /**
         * @brief       
         * 
         * @param       TxHash:
         * @param       info:
        */
        void vrfInfoAdder(const std::string & TxHash,Vrf & info){
            
            std::unique_lock<std::shared_mutex> lck(vrfInfoMutex_);
            uint64_t time_= MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
            vrf_cache_data[TxHash]={info,time_};
        }
        /**
         * @brief       
         * 
         * @param       TxHash:
         * @param       info:
        */
        void addTransactionVerificationInfo(const std::string & TxHash,const Vrf & info){
            std::unique_lock<std::shared_mutex> lck(vrfInfoMutex_);
            uint64_t time_= MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
            tx_vrf_cache_[TxHash]={info,time_};
        }
        /**
         * @brief       
         * 
         * @param       TxHash:
         * @param       vrf:
         * @return      true
         * @return      false
        */
        bool get_vrf_info(const std::string & TxHash,std::pair<std::string,Vrf> & vrf){
            std::shared_lock<std::shared_mutex> lck(vrfInfoMutex_);
            auto ite= vrf_cache_data.find(TxHash);
            if(ite!=vrf_cache_data.end()){
                vrf.first=ite->first;
                vrf.second=ite->second.first;
                return true;
            }
            return false;
        }
        /**
         * @brief       
         * 
         * @param       TxHash:
         * @param       vrf:
         * @return      true
         * @return      false
        */
        bool fetchTransactionVrfDetails(const std::string & TxHash,std::pair<std::string,Vrf> & vrf){
            std::shared_lock<std::shared_mutex> lck(vrfInfoMutex_);
            auto ite= tx_vrf_cache_.find(TxHash);
            if(ite!=tx_vrf_cache_.end()){
                vrf.first=ite->first;
                vrf.second=ite->second.first;
                return true;
            }
            return false;
        }
        /**
        * @brief       
        * 
        * @param       TxHash:
        * @param       AddressVector:
        */
        void addVerificationNodes(const std::string & TxHash,std::vector<std::string> & AddressVector){
            std::unique_lock<std::shared_mutex> lck(vrfNodeMutex_);
            uint64_t time_= MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
            vrfVerificationNodeRequest[TxHash] = {AddressVector, time_};
            std::set<std::string> nullVector;
            blockSignatureCacheData[TxHash] = {nullVector, time_};
        }
        /**
        * @brief       
        * 
        * @param       TxHash:
        * @param       signNodes:
        * @return      true
        * @return      false
        */

        bool getBlockFlowSignNodes(const std::string & hash, std::vector<std::string>& signNodes,const std::string blockSignAddress = "",bool isverifySign = false){
            std::unique_lock<std::shared_mutex> lck(vrfNodeMutex_);
            auto iter= vrfVerificationNodeRequest.find(hash);
            if(iter != vrfVerificationNodeRequest.end()){
                signNodes = iter->second.first;
            }

            if(isverifySign)
            {
                auto item= blockSignatureCacheData.find(hash);
                if(item != blockSignatureCacheData.end()){
                    auto it = std::find(item->second.first.begin(),item->second.first.end(),blockSignAddress);
                    if(it != item->second.first.end())
                    {
                        DEBUGLOG(" blockSignatureCacheData already added !");
                        return false;
                    }else
                    {
                        DEBUGLOG("insert blockSignatureCacheData");
                        item->second.first.insert(blockSignAddress);
                    }
                }
            }

            return true;
        }

        void StopTimer() { ClearTimer.Cancel(); }
     private:
        
        friend std::string PrintCache(int where);
        std::map<std::string,std::pair<Vrf,uint64_t>> vrf_cache_data; //Block flow vrf
        std::map<std::string,std::pair<Vrf,uint64_t>> tx_vrf_cache_; //Vrf of transaction flows
        std::map<std::string,std::pair<std::vector<std::string>,uint64_t>> vrfVerificationNodeRequest; //The vrf corresponds to the verification address required by the hash
        std::map<std::string,std::pair<std::set<std::string>,uint64_t>> blockSignatureCacheData;
        std::shared_mutex vrfInfoMutex_;
        std::shared_mutex vrfNodeMutex_;
        CTimer ClearTimer;
        /**
        * @brief       
        * 
        * @param       data:
        * @return       long long
        */
        long long stringToll(const std::string& data)
        {
            long long value = 0;
            for(int i = 0;i< data.size() ;i++)
            {
                    int a= (int )data[i];
                    value += a;
            }
            return value;
        }
};






#endif