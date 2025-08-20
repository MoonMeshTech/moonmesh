/**
 * *****************************************************************************
 * @file        global_data.h
 * @brief       
 * @author  ()
 * @date        2023-09-28
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef COMMON_GLOBAL_HEADER_DATA
#define COMMON_GLOBAL_HEADER_DATA

#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <map>

struct GlobalData;
class GlobalDataManager
{
public:
    /**
     * @brief       Create a Wait object
     * 
     * @param       timeOutSec 
     * @param       retNum 
     * @param       outMsgId 
     * @return      true 
     * @return      false 
     */
    bool CreateWait(uint32_t timeOutSec, uint32_t retNum,
                    std::string &outMsgId);
    /**
     * @brief       
     * 
     * @param       msgId 
     * @param       data 
     * @return      true 
     * @return      false 
     */

    /**
     * @brief       
     * 
     * @param       msg_id 
     * @param       res_id 
     * @return      true 
     * @return      false 
     */
    
    bool AddResNode(const std::string &msgId, const std::string &res_id);
    
    /**
     * @brief       
     * 
     * @param       msg_id 
     * @param       res_id 
     * @param       data 
     * @return      true 
     * @return      false 
     */
    bool waitDataToAdd(const std::string &msgId, const std::string &res_id, const std::string &data);

    /**
     * @brief       
     * 
     * @param       msgId 
     * @param       retData 
     * @return      true 
     * @return      false 
     */
    bool WaitData(const std::string &msgId, std::vector<std::string> &retData);
    
    static GlobalDataManager &get_global_data_manager();


private:
    GlobalDataManager() = default;
    ~GlobalDataManager() = default;
    GlobalDataManager(GlobalDataManager &&) = delete;
    GlobalDataManager(const GlobalDataManager &) = delete;
    GlobalDataManager &operator=(GlobalDataManager &&) = delete;
    GlobalDataManager &operator=(const GlobalDataManager &) = delete;
    std::mutex data_mutex;
    friend std::string PrintCache(int where);
    std::map<std::string, std::shared_ptr<GlobalData>> _globalData;
};

#define dataMgrPtr GlobalDataManager::get_global_data_manager()

#endif
