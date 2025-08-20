#ifndef _VRF_CONSENSUS_H_
#define _VRF_CONSENSUS_H_

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <map>
#include <memory>

#include "proto/ca_protomsg.pb.h"
#include "net/msg_queue.h"

const int CYCLE_DURATION_SECONDS = 10; 
#define MAX_VRFS_SIZE 10
class VRFStructure {
public:
    VRFStructure() = default;
    VRFStructure(const std::string& seed, const std::string& signature, const std::string& nodeId);

    std::string getAddress() const;
    std::string getSignatureHash() const;
    std::string getSeed() const;
    double getFrequency() const;
    std::map<uint64_t, std::vector<std::string>> calculateHeightHashes() const;

    void addHashForHeight(uint64_t height, const std::string& hash);
    void addFrequency(const double frequency);
private:
    std::string seed;
    std::string signature;
    std::string nodeId;
    std::string signatureHash;
    double frequency;

    std::map<uint64_t, std::vector<std::string>> height_hashes;

};

class VRFConsensusNode {
public:
    void Process();
    int sign(const std::string& data, std::string& signature, std::string& pub);
    int generateVRFStructure(const std::string& seed, VRFConsensusInfo& vrf);
    void vrfBroadcastMessage(const VRFConsensusInfo& vrf);
    void receiveVRRequest(const VRFConsensusInfo& vrf);
    void run();
    bool extractVRFData(std::map<std::string, VRFStructure>& dataSource, uint64_t timestamp);
    bool extractPackagerVRFData(std::vector<std::string>& nodes, uint64_t timestamp);
    bool checkVRFExists(const std::string& seed, const std::string& nodeId) const;
    bool addVRFStructure(const VRFConsensusInfo& vrfInfo, const std::string& nodeId);

    
    void computeMaxHeightWithMinFrequency(const std::string& seed = "") ;

    std::string printfValidatedVRfs();
    std::string printfNewValidatedVRfs();
private:
    void pruneOldData();
    std::string generateVRSeed(std::chrono::system_clock::time_point time) const;
    std::chrono::system_clock::time_point cycleStartTimestamp(std::chrono::system_clock::time_point current_time) const;
    std::chrono::system_clock::time_point getPreviousCycleTime(uint64_t timestamp) const;
    bool validate_block_info(const VRFConsensusInfo& vrfInfo, const std::string& nodeId) const;

    std::map<std::string, std::map<std::string, VRFStructure>> validatedVRfs;
    std::map<std::string, std::map<std::string, VRFStructure>> newValidatedVRfs;
    mutable std::shared_mutex mutex_;
    std::thread VRFConsensusThread;
};

int HandleVRFConsensusInfoReq(const std::shared_ptr<VRFConsensusInfo>& msg, const MsgData& msgData);

#endif // _VRF_CONSENSUS_H_