/**
 * *****************************************************************************
 * @file        unregister_node.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef UNREGISTER_NODE_H
#define UNREGISTER_NODE_H

#include <shared_mutex>
#include <map>
#include <set>
#include <vector>
#include "node.hpp"
#include "db/db_api.h"
#include "utils/timer.hpp"
#include "utils/account_manager.h"

class UnregisterNode
{
public:
    UnregisterNode();
    UnregisterNode(UnregisterNode &&) = delete;
    UnregisterNode(const UnregisterNode &) = delete;
    UnregisterNode &operator=(UnregisterNode &&) = delete;
    UnregisterNode &operator=(const UnregisterNode &) = delete;
    ~UnregisterNode();
public:

    static std::vector<Node> intersect(const std::vector<Node>& vec1, const std::vector<Node>& vec2)
    {
        std::vector<Node> result;

        std::vector<Node> sortedVecOne = vec1;
        std::vector<Node> sorted_vec_two = vec2;
        std::sort(sortedVecOne.begin(), sortedVecOne.end(),NodeCompare());
        std::sort(sorted_vec_two.begin(), sorted_vec_two.end(),NodeCompare());

        std::set_intersection(sortedVecOne.begin(), sortedVecOne.end(),
                              sorted_vec_two.begin(), sorted_vec_two.end(),
                              std::back_inserter(result),NodeCompare());

        return result;
    }


    /**
     * @brief       
     * 
     * @param       node 
     * @return      int 
     */
    int Add(const Node & node);
    /**
     * @brief       
     * 
     * @param       node 
     * @return      int 
     */
    int Find(const Node & node);

    /**
     * @brief       
     * 
     * @param       nodeMap 
     * @return      true 
     * @return      false 
     */
    bool Register(std::map<uint32_t, Node> nodeMap);
    
    /**
     * @brief       
     * 
     * @param       serverList 
     * @return      true 
     * @return      false 
     */
    bool startRegistrationNode(std::map<std::string, int> &serverList);

    /**
     * @brief       
     * 
     * @return      true 
     * @return      false 
     */
    bool syncStartNode();

    struct NodeCompare
    {
        bool operator()(const Node& n1, const Node& n2) const {
            return n1.address < n2.address;
        }
    };

    static bool compareStructs(const Node& x1, const Node& x2) {
    return (x1.address == x2.address);
    }

    /**
     * @brief       Get the Ip Map object
     * 
     * @param       m1 
     */
    void GetIpMap(std::map<uint64_t, std::map<Node,int, NodeCompare>> & m1); 
    void GetIpMap(std::map<uint64_t, std::map<std::string, int>> & m1,std::map<uint64_t, std::map<std::string, int>> & m2);
    
    /**
     * @brief       Get the Consensus Node List object
     * 
     * @param       tergetNodeAddr 
     * @param       conNodeList 
     */
    

    /**
     * @brief       
     * 
     * @param        
     */
    void deleteSplitNodeList(const std::string & );

    /**
     * @brief       
     * 
     * @param       syncNodeCount 
     */

    /**
     * @brief       
     * 
     */

    void data_to_split_and_insert(const std::map<Node, int, NodeCompare>  syncNodeCount);
    void clear_split_node_list_data();

    void get_consensus_stake_node_list(std::map<std::string,int>& consensusStakeNodeMap_);
    void get_consensus_node_list(std::map<std::string,int>& consensus_node_map);
private:
    friend std::string PrintCache(int where);
    std::shared_mutex mutexForNodes;
    std::shared_mutex mutexConsensusNodes;
    std::mutex vrfNodeListMutex;
    std::mutex mutexStackList;
    std::map<std::string, Node> _nodes;

    //The IP address and the corresponding number of times are stored once and synchronized once
    std::map<uint64_t, std::map<Node,int, NodeCompare>> _consensusNodeList;

    std::map<uint64_t,std::map<std::string,int>> stake_node_list;
    std::map<uint64_t,std::map<std::string,int>> unstakeNodeList;
};

#endif 