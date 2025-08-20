#include "vrf_consensus.h"
#include <openssl/sha.h>
#include <numeric>
#include <functional>
#include "net/api.h"
#include "utils/account_manager.h"
#include "ca/txhelper.h"
#include "ca/algorithm.h"
#include "common/task_pool.h"
#include "ca/transaction.h"

std::string convertToReadableDate(const std::chrono::system_clock::time_point& timePoint) {
    std::time_t timeT = std::chrono::system_clock::to_time_t(timePoint);

    std::tm tm = *std::gmtime(&timeT);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

// VRFStructure implementations
VRFStructure::VRFStructure(const std::string& seed, const std::string& signature, const std::string& nodeId)
    : seed(seed), signature(signature), nodeId(nodeId), signatureHash(Getsha256hash(signature)), frequency(0.0) {}

std::string VRFStructure::getAddress() const {
    return nodeId;
}

std::string VRFStructure::getSignatureHash() const {
    return signatureHash;
}

std::string VRFStructure::getSeed() const{
    return seed;
}

double VRFStructure::getFrequency() const{
    return frequency;
}

std::map<uint64_t, std::vector<std::string>> VRFStructure::calculateHeightHashes() const{
    return height_hashes;
}

void VRFStructure::addHashForHeight(uint64_t height, const std::string& hash) {
    height_hashes[height].push_back(hash);
}

void VRFStructure::addFrequency(const double frequency)
{
    this->frequency = frequency;
}

// VRFConsensusNode implementations
int VRFConsensusNode::sign(const std::string& data, std::string& signature, std::string& pub) {
    std::string hash = Getsha256hash(data);
    std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
    if (TxHelper::Sign(defaultAddr, hash, signature, pub) != 0) {
        ERRORLOG("Signature failed");
        return -1;
    }
    return 0;
}

int VRFConsensusNode::generateVRFStructure(const std::string& seed, VRFConsensusInfo& vrf) {
    std::string signature;
    std::string pub;

    if (sign(seed, signature, pub) != 0) {
        return -1;
    }
    vrf.set_vrfseed(seed);

    CSign* sign = vrf.mutable_vrfsign();
    sign->set_sign(signature);
    sign->set_pub(pub);


    DBReader dbReader;
    uint64_t top = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(top))
    {
        ERRORLOG("getBlockTop error! top:{}",top);
        return -2;
    }

    for(int i = top; i >= top - 10; i--)
    {
        std::vector<std::string> block_hashes_temp;
        if (DBStatus::DB_SUCCESS != dbReader.getBlockHashsByBlockHeight(i, block_hashes_temp)) //hash Get the hash of all blocks with the height of this block
        {
            ERRORLOG("getBlockHashsByBlockHeight  block.height:{}", i);
            return -3;
        }
        
        for(const auto& hash : block_hashes_temp)
        {
            auto blockinfo = vrf.add_blockinfo();
            blockinfo->set_blockhash(hash.substr(0,8));
            blockinfo->set_height(i);
            if(vrf.blockinfo_size() >= 10)
            {
                return 0;
            }
        }
    }
    return 0;
}

void VRFConsensusNode::vrfBroadcastMessage(const VRFConsensusInfo& vrf) {
    auto vrfPtr = std::make_shared<VRFConsensusInfo>(vrf);
    const auto& public_node_list = MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    for (const auto& node : public_node_list) {
        MagicSingleton<TaskPool>::GetInstance()->commitBroadcastRequest(
            [node, vrfPtr]() {
                net_com::SendVRFConsensusInfoTask(node, *vrfPtr);
            }
        );
    }
    return;
}

bool VRFConsensusNode::validate_block_info(const VRFConsensusInfo& vrfInfo, const std::string& nodeId) const{

    int ret = VerifyBonusAddr(nodeId);
    int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(nodeId, global::ca::StakeType::STAKE_TYPE_NODE);
    if (stakeTime <= 0 || ret != 0) {
        return false;
    }

    if(vrfInfo.blockinfo_size() != 10){
        ERRORLOG("vrf.blockinfo_size:{} != 10", vrfInfo.blockinfo_size());
        return false;
    }

    std::unordered_set<std::string> hashSet;
    for (const auto& blockinfo : vrfInfo.blockinfo()) {
        const std::string& hash = blockinfo.blockhash();

        if (hashSet.find(hash) != hashSet.end()) {
            return false;
        }
        hashSet.insert(hash);
    }
    
    return true;
}


void VRFConsensusNode::receiveVRRequest(const VRFConsensusInfo& vrf) {
    auto generatedSeedTime = std::chrono::system_clock::now();
    const CSign& sign = vrf.vrfsign();
    std::string nodeId = GenerateAddr(sign.pub());

    std::string receivedSeed = vrf.vrfseed();
    auto receivedSeedTime = std::chrono::system_clock::from_time_t(std::stoll(receivedSeed));
    auto generatedSeedTimeInSeconds = std::chrono::system_clock::to_time_t(generatedSeedTime);
    auto adjustedGeneratedSeedTime = std::chrono::system_clock::from_time_t(generatedSeedTimeInSeconds);
    
    auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(adjustedGeneratedSeedTime - receivedSeedTime).count();
    DEBUGLOG("seed:{}, receivedSeedTime:{}, generatedSeedTime:{}, timeDiff:{}, id:{}",receivedSeed, convertToReadableDate(receivedSeedTime), convertToReadableDate(adjustedGeneratedSeedTime), timeDiff, nodeId);

    if (std::abs(timeDiff) > 10) {
        TRACELOG("seed:{} time difference is greater than 10 seconds", receivedSeed);
        return;
    }

    {
        DBReader dbReader;
        uint64_t top = 0;
        dbReader.getBlockTop(top);
        if (top >= global::ca::MIN_UNSTAKE_HEIGHT) {

            if(!validate_block_info(vrf, nodeId)){
                ERRORLOG("validateVRFConsensusInfo fail!!! seed:{}", receivedSeed);
                return;
            }
        }
    }

    std::string vrfSeedHashValue = Getsha256hash(receivedSeed);
    if (ca_algorithm::VerifySign(sign, vrfSeedHashValue) != 0) {
        ERRORLOG("VerifySign fail, addr:{}, seed:{}", GenerateAddr(sign.pub()), receivedSeed);
        return;
    }

    if(!addVRFStructure(vrf, nodeId))
    {
        return;
    }

    DEBUGLOG("BroadcastVRF addr:{}, seed:{}", nodeId, receivedSeed);
    vrfBroadcastMessage(vrf);
}

void VRFConsensusNode::pruneOldData() {
    if (validatedVRfs.size() > MAX_VRFS_SIZE) {
        while (validatedVRfs.size() > MAX_VRFS_SIZE) {
            validatedVRfs.erase(validatedVRfs.begin());
        }
    }

    if (newValidatedVRfs.size() > MAX_VRFS_SIZE) {
        while (newValidatedVRfs.size() > MAX_VRFS_SIZE) {
            newValidatedVRfs.erase(newValidatedVRfs.begin());
        }
    }
}

bool VRFConsensusNode::addVRFStructure(const VRFConsensusInfo& vrfInfo, const std::string& nodeId) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    const std::string& seed = vrfInfo.vrfseed();
    const std::string& signature = vrfInfo.vrfsign().sign();
    if (validatedVRfs[seed].find(nodeId) == validatedVRfs[seed].end()) 
    {
        VRFStructure vrf(seed, signature, nodeId);
        
        for(const auto& blockinfo : vrfInfo.blockinfo())
        {
            vrf.addHashForHeight(blockinfo.height(), blockinfo.blockhash());
        }

        validatedVRfs[seed][nodeId] = vrf;
        pruneOldData();
        DEBUGLOG("validatedVRfs.second size:{}, vrfSeed:{}, id:{}", validatedVRfs[seed].size(), seed, nodeId);
        return true;
    }

    return false;
}


bool VRFConsensusNode::checkVRFExists(const std::string& seed, const std::string& nodeId) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = validatedVRfs.find(seed);
    if (it != validatedVRfs.end()) {
        const auto& nodeMap = it->second;
        if (nodeMap.find(nodeId) != nodeMap.end()) {
            return true;
        }
    }
    return false;
}

void VRFConsensusNode::Process()
{
    VRFConsensusThread = std::thread(std::bind(&VRFConsensusNode::run, this));
    VRFConsensusThread.detach();
}

void VRFConsensusNode::run() {
    while (true) {
        auto currentTime = std::chrono::system_clock::now();
        auto nextCycleStart = cycleStartTimestamp(currentTime);

        DEBUGLOG("Current Time: {}, nextCycleStart:{}", convertToReadableDate(currentTime), convertToReadableDate(nextCycleStart));
        
        std::this_thread::sleep_until(nextCycleStart);

        std::string vrfSeed = generateVRSeed(nextCycleStart);
        DEBUGLOG("vrSeed:{}", vrfSeed);
        VRFConsensusInfo vrf;
        if (generateVRFStructure(vrfSeed, vrf) != 0) {
            ERRORLOG("GenerateVRFStructure fail, vrfSeed:{}", vrfSeed);
            continue;
        }

        std::string strFromAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
        {
            DBReader dbReader;
            uint64_t top = 0;
            dbReader.getBlockTop(top);
            if (top >= global::ca::MIN_UNSTAKE_HEIGHT) {

                if(validate_block_info(vrf, strFromAddr)) 
                {
                    addVRFStructure(vrf, strFromAddr);
                }
                else 
                {
                    DEBUGLOG("I don't meet the qualifications. vrfSeed:{}", vrfSeed);
                    continue;
                }
            }

        }
        DEBUGLOG("vrfBroadcastMessage vrfSeed:{}", vrfSeed);
        vrfBroadcastMessage(vrf);
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            computeMaxHeightWithMinFrequency();
        }

    }
}


void VRFConsensusNode::computeMaxHeightWithMinFrequency(const std::string& seed) {
    std::map<uint64_t, std::unordered_map<std::string, int>> heightHashCounts;
    std::map<std::string, std::set<std::string>> nodeHashes;

    // Check if there is enough validated VRFs data
    if (validatedVRfs.size() < 3) {
        DEBUGLOG("Not enough validated VRFs data. size:{}", validatedVRfs.size());
        return;
    }

    // Extract data from the second last period
    auto it = std::prev(validatedVRfs.end(), 3);
    if(!seed.empty())
    {
        it = validatedVRfs.find(seed);
        if(it == validatedVRfs.end())
        {
            ERRORLOG("seed no found!!!, seed:{}", seed);
            return;
        }
    }
    DEBUGLOG("seed:{}", it->first);
    // Iterate over all nodes' data in that period
    for (const auto& nodePair : it->second) {
        DEBUGLOG("11seed:{}, id:{}", it->first, nodePair.first);
        const auto& vrfStructure = nodePair.second;
        // Iterate over each height and its corresponding hash list
        for (const auto& heightHashesPair : vrfStructure.calculateHeightHashes()) {
            uint64_t height = heightHashesPair.first;
            const auto& hashes = heightHashesPair.second;
            // Count occurrences of each hash at each height
            for (const auto& hash : hashes) {
                heightHashCounts[height][hash]++;
                nodeHashes[nodePair.first].insert(hash);
            }
        }
    }

    if(nodeHashes.empty())
    {
        DEBUGLOG("nodeHashes.empty() == true");
        return;
    }

    std::unordered_map<uint64_t, std::map<std::string, double>> mostFrequentHashes;
    std::map<uint64_t, std::vector<double>> heightHashRatios;
    // Find the most frequent hash at each height
    for (const auto& heightPair : heightHashCounts) {
        uint64_t height = heightPair.first;
        const auto& hashCounts = heightPair.second;

        for (const auto& hash : hashCounts) {
            double ratio = static_cast<double>(hash.second) / it->second.size();
            mostFrequentHashes[height][hash.first] = ratio;
            heightHashRatios[height].push_back(ratio);
        }
    }

    // Calculate the average frequency for each height
    std::vector<std::pair<uint64_t, double>> averagedHeightRatios;
    for (const auto& heightRatioPair : heightHashRatios) {
        uint64_t height = heightRatioPair.first;
        const auto& ratios = heightRatioPair.second;
        double averageRatio = std::accumulate(ratios.begin(), ratios.end(), 0.0) / ratios.size();
        averagedHeightRatios.push_back({height, averageRatio});
    }

    // Calculate the weighted average frequency as the optimal frequency
    double totalWeightedFreq = 0.0;
    double totalWeight = 0.0;
    for (const auto& heightRatioPair : averagedHeightRatios) {
        totalWeightedFreq += heightRatioPair.second * heightRatioPair.first;
        totalWeight += heightRatioPair.first;
    }

    double optimalFrequency = (totalWeight != 0) ? (totalWeightedFreq / totalWeight) : 0.0;

    DEBUGLOG("11seed:{}, Optimal Frequency:{}",it->first, optimalFrequency);

    std::set<std::string> optimalHashes;
    for (const auto& mostPair : mostFrequentHashes) {

        for(const auto& hashPair : mostPair.second) {
            DEBUGLOG("11seed:{}, height:{}, optimalHashes:{}, Ratio:{}",it->first, mostPair.first, hashPair.first.substr(0,6), hashPair.second);
            if (hashPair.second >= optimalFrequency) {
                optimalHashes.insert(hashPair.first);
            }
        }

    }

    for(const auto& hash : optimalHashes)
    {
        DEBUGLOG("11seed:{}, hash:{}",it->first, hash.substr(0,6));
    }
    DEBUGLOG("------------------------");
    std::unordered_map<std::string, int> nodeHashCount;
    // Count the number of optimal hashes each node has
    for (const auto& nodePair : nodeHashes) {
        nodeHashCount[nodePair.first] = 0;
        for (const auto& hash : nodePair.second) {
            DEBUGLOG("11seed:{}, id:{}, hash:{}",it->first, nodePair.first, hash.substr(0,6));
            if (optimalHashes.find(hash) != optimalHashes.end()) {
                nodeHashCount[nodePair.first]++;
            }
        }
    }

    double frequencyThreshold = 1.0;
    std::vector<std::string> qualifiedNodes;
    uint64_t validateNodeThreshold = it->second.size() * 0.55;
    DEBUGLOG("11seed:{}, size:{}, validateNodeThreshold:{}", it->first, it->second.size(), validateNodeThreshold);
    // Lower the frequency threshold by 0.1 until at least validateNodeThreshold are found
    do {
        qualifiedNodes.clear();
        for (const auto& nodeCountPair : nodeHashCount) {
            double frequency = static_cast<double>(nodeCountPair.second) / optimalHashes.size();
            DEBUGLOG("11seed:{}, nodeHashCount, hash:{}, count:{}, frequency:{}",it->first, nodeCountPair.first, nodeCountPair.second, frequency)
            if (frequency >= frequencyThreshold) {

                auto found = it->second.find(nodeCountPair.first);
                if (found != it->second.end()) {
                    found->second.addFrequency(frequency);
                    newValidatedVRfs[it->first].emplace(found->first, found->second);
                    DEBUGLOG("11seed:{}, id:{}, size:{}",it->first, found->first, newValidatedVRfs[it->first].size());
                }

                qualifiedNodes.push_back(nodeCountPair.first);
            }
        }
        frequencyThreshold = std::round(frequencyThreshold * 10.0) / 10.0;
        frequencyThreshold -= 0.1;
    } while (qualifiedNodes.size() < validateNodeThreshold && frequencyThreshold > 0);

    return;
}


bool VRFConsensusNode::extractVRFData(std::map<std::string, VRFStructure>& dataSource, uint64_t timestamp) {
    auto previousCycleTime = getPreviousCycleTime(timestamp);
    auto prevCycleTimeSec = std::chrono::duration_cast<std::chrono::seconds>(previousCycleTime.time_since_epoch()).count();
    std::string seed = std::to_string(prevCycleTimeSec);
    DEBUGLOG("seed:{}, previousCycleTime:{}", seed, convertToReadableDate(previousCycleTime));

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = newValidatedVRfs.find(seed);
    if (it == newValidatedVRfs.end() || it->second.empty()) {
        auto oldIt = validatedVRfs.find(seed);
        if (oldIt == validatedVRfs.end()) {
            return false;
        }

        computeMaxHeightWithMinFrequency(seed);
        it = newValidatedVRfs.find(seed);
        if(it != newValidatedVRfs.end())
        {
            DEBUGLOG("11seed:{}, AnewValidatedVRfs:{}", seed, newValidatedVRfs.size());
            dataSource.insert(it->second.begin(), it->second.end());
        }
        else
        {
            DEBUGLOG("11seed:{}, validatedVRfs:{}", seed, validatedVRfs.size());
            dataSource.insert(oldIt->second.begin(), oldIt->second.end());
        }
    } 
    else 
    {
        DEBUGLOG("11seed:{}, BnewValidatedVRfs:{}, newSize:{}", seed, newValidatedVRfs.size(), it->second.size());
        dataSource.insert(it->second.begin(), it->second.end());
    }
    return true;
}

void selectRandomNodes(const std::map<std::string, VRFStructure>& dataSource, std::vector<std::string>& nodes, size_t count) {
    
    std::string sumSeed;
	for(auto iter = dataSource.begin(); iter != dataSource.end(); ++iter){
		sumSeed += iter->second.getSignatureHash();
	}

	std::string seed = Getsha256hash(sumSeed); 

	std::vector<std::pair<std::string, VRFStructure>> datas = shuffleMap(dataSource, seed);
    for(auto & it : datas)
    {
        nodes.push_back(it.first);
        if (nodes.size() >= count) 
        {
            break;
        }
    }

    return;
}

bool VRFConsensusNode::extractPackagerVRFData(std::vector<std::string>& nodes, uint64_t timestamp) {
    auto previousCycleTime = getPreviousCycleTime(timestamp);
    auto prevCycleTimeSec = std::chrono::duration_cast<std::chrono::seconds>(previousCycleTime.time_since_epoch()).count();
    std::string seed = std::to_string(prevCycleTimeSec);
    DEBUGLOG("seed:{}, previousCycleTime:{}", seed, convertToReadableDate(previousCycleTime));
    


    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::map<std::string, VRFStructure> dataSource;
    auto it = newValidatedVRfs.find(seed);
    if (it == newValidatedVRfs.end() || it->second.empty()) {
        auto oldIt = validatedVRfs.find(seed);
        if (oldIt == validatedVRfs.end()) {
            return false;
        }

        computeMaxHeightWithMinFrequency(seed);
        it = newValidatedVRfs.find(seed);
        if(it != newValidatedVRfs.end())
        {
            DEBUGLOG("11seed:{}, AnewValidatedVRfs:{}", seed, newValidatedVRfs.size());
            dataSource.insert(it->second.begin(), it->second.end());
        }
        else
        {
            DEBUGLOG("11seed:{}, validatedVRfs:{}", seed, validatedVRfs.size());
            dataSource.insert(oldIt->second.begin(), oldIt->second.end());
        }
    } 
    else 
    {
        DEBUGLOG("11seed:{}, BnewValidatedVRfs:{}, newSize:{}", seed, newValidatedVRfs.size(), it->second.size());
        dataSource.insert(it->second.begin(), it->second.end());
    }

    std::vector<std::pair<std::string, VRFStructure>> sortedData(dataSource.begin(), dataSource.end());
    std::sort(sortedData.begin(), sortedData.end(), [](const auto& a, const auto& b) {
        return a.second.getFrequency() < b.second.getFrequency();
    });

    double maxValue = 1.0;
    const size_t minNodes = global::ca::kConsensus;
    std::map<std::string, VRFStructure> filteredDataSource;

    while (filteredDataSource.size() < minNodes) {
        for (const auto& data : sortedData) {
            if (data.second.getFrequency() >= maxValue) {
                filteredDataSource.insert({data.first, data.second});
            }
        }
        maxValue = std::round(maxValue * 10.0) / 10.0;
        if (filteredDataSource.size() < minNodes) {
            filteredDataSource.clear();
            maxValue -= 0.1;
            if (maxValue < 0.0) {
                break;
            }
        }
    }

    if(dataSource.size() < global::ca::kConsensus)
    {
        ERRORLOG("dataSource.size() < global::ca::kConsensus");
        return false;
    }

    dataSource = std::move(filteredDataSource);

    selectRandomNodes(dataSource, nodes, global::ca::kConsensus);
    DEBUGLOG("seed:{}, maxValue:{}",seed, maxValue);
    for(const auto& id : nodes)
    {
        DEBUGLOG("id:{}", id);
    }
    return true;
}


std::string VRFConsensusNode::generateVRSeed(std::chrono::system_clock::time_point time) const {
    auto duration = time.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    seconds -= seconds % CYCLE_DURATION_SECONDS; 
    return std::to_string(seconds);
}

std::chrono::system_clock::time_point VRFConsensusNode::cycleStartTimestamp(std::chrono::system_clock::time_point current_time) const {
    auto duration = current_time.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    seconds += CYCLE_DURATION_SECONDS - (seconds % CYCLE_DURATION_SECONDS); 
    return std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
}

std::chrono::system_clock::time_point VRFConsensusNode::getPreviousCycleTime(uint64_t timestamp) const {
    auto duration = std::chrono::microseconds(timestamp);
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    seconds -= seconds % CYCLE_DURATION_SECONDS; 
    seconds -= 2 * CYCLE_DURATION_SECONDS; 
    return std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
}

std::string VRFConsensusNode::printfValidatedVRfs()
{
    std::ostringstream oss;

    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (validatedVRfs.empty()) {
        oss << "No validated VRFs available." << std::endl;
    } else {
        oss << "Validated VRFs:" << std::endl;
        oss << "==============================" << std::endl;
        for (const auto& pair : validatedVRfs) {
            oss << "Key: " << pair.first << std::endl;
            for (const auto& vrfKeyPair : pair.second) {
                oss << "  NodeId: " << vrfKeyPair.second.getAddress() << std::endl;
            }
            oss << "------------------------------" << std::endl;
        }
    }
    return oss.str();
}

std::string VRFConsensusNode::printfNewValidatedVRfs()
{
    std::ostringstream oss;

    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (newValidatedVRfs.empty()) {
        oss << "No newValidated VRFs available." << std::endl;
    } else {
        oss << "newValidated VRFs:" << std::endl;
        oss << "==============================" << std::endl;
        for (const auto& pair : newValidatedVRfs) {
            oss << "Key: " << pair.first << std::endl;
            for (const auto& vrfKeyPair : pair.second) {
                oss << "  NodeId: " << vrfKeyPair.first << "     frequency:" << vrfKeyPair.second.getFrequency() << std::endl;
            }
            oss << "------------------------------" << std::endl;
        }
    }
    return oss.str();
}

int HandleVRFConsensusInfoReq(const std::shared_ptr<VRFConsensusInfo> &msg, const MsgData &msgData)
{
	Node node;
    if(!MagicSingleton<PeerNode>::GetInstance()->locateNodeByFd(msgData.fd, node))
    {
        ERRORLOG("Invalid message fd:{}, node.address:{}", msgData.fd, node.address);
        return -1;
    }

    MagicSingleton<VRFConsensusNode>::GetInstance()->receiveVRRequest(*msg);
    return 0;
}