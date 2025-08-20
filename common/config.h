/**
 * *****************************************************************************
 * @file        config.h
 * @brief       
 * @author  ()
 * @date        2023-09-28
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <string>
#include <set>
#include <type_traits>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <utility>
#include <chrono>
#include <boost/asio/deadline_timer.hpp>
#include <thread>
#include <functional>

#include "../utils/timer.hpp"
#include <nlohmann/json.hpp>
#include "../utils/magic_singleton.h"
#define kServerMainPort (MagicSingleton<Config>::GetInstance()->GetServerPort())

void update_config_ip(const std::string &newIP);
bool isValidIPv4(const std::string &ip);
class Node;
class Config
{
public:
    struct Info
    {
        std::string logo;
        std::string name;
    };

    struct Log
    {
        bool console;
        std::string level;
        std::string path;
    };

    struct HttpCallback
    {
        std::string ip;
        std::string path;
        uint32_t port;
    };

    const std::string kConfigFilename = "config.json";
    const std::string HTTP_CONFIG_CALLBACK = "http_callback";
    const std::string kHttpCallbackIp = "ip";
    const std::string CFG_HTTP_CALLBACK_PORT = "port";
    const std::string CFG_HTTP_CALLBACK_PATH = "path";
    const std::string CFG_HTTP_PORT = "http_port";
    const std::string KRpcApi = "rpc";
    const std::string kCfgInfo = "info";
    const std::string CFG_INFO_LOGO = "logo";
    const std::string kConfigInfoName = "name";
    const std::string kCfgIp = "ip";
    const std::string k_config_log = "log";
    const std::string CFG_LOG_LEVEL = "level";
    const std::string CONFIG_LOG_PATH = "path";
    const std::string kConfigLogToConsole = "console";
    const std::string kCfgServerPort = "server_port";
    const std::string CONFIG_KEY_VERSION = "version";

    nlohmann::json tmpJson ;
    int count = 0;

public:
    Config()
    {
        InitFile();
        _thread = std::thread(std::bind(&Config::_Check, this));
    };

    Config(bool & flag)
    {
        int ret = InitFile();
        if ( ret == 0 ) flag = true;
        _thread = std::thread(std::bind(&Config::_Check, this));
    };
    
    ~Config()
    {  
        _exitThread = true;
        if (_thread.joinable())
        {
            _thread.join();
        }
    };

    /**
     * @brief       
     * 
     * @return      int 
     */
    int InitFile();

    /**
     * @brief       
     * 
     * @param       _node 
     * @return      int 
     */
    int _Verify(const Node& _node);

    /**
     * @brief       Get the Server Port object
     * 
     * @return      int 
     */
    int GetServerPort();

    /**
     * @brief       Get the Http Port object
     * 
     * @return      int 
     */
    int GetHttpPort();

    /**
     * @brief       Get the Info object
     * 
     * @param       info 
     * @return      int 
     */
    int GetInfo(Config::Info & info);

    /**
     * @brief       Get the Log object
     * 
     * @param       log 
     * @return      int 
     */
    int GetLog(Config::Log & log);

    /**
     * @brief       Get the Http Callback object
     * 
     * @param       httpCallback 
     * @return      int 
     */
    int GetHttpCallback(Config::HttpCallback & httpCallback);

    /**
     * @brief       Get the Rpc object
     * 
     * @return      true 
     * @return      false 
     */
    bool GetRpc();

    /**
     * @brief       
     * 
     * @return      std::string 
     */
    std::string GetIP();
    
    /**
     * @brief       
     * 
     * @param       ip 
     * @return      int 
     */
    int SetIP(const std::string & ip);

    /**
     * @brief       Get the Server object
     * 
     * @return      std::set<std::string> 
     */
    std::set<std::string> GetServer();

    std::string GetVersion();

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       t 
     * @return      int 
     */
    template <typename T>
    int FILTER_SERVER_PORT(T &t);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       t 
     * @return      int 
     */
    template <typename T>
    int filter_http_port(T &t);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       t 
     * @return      int 
     */
    template <typename T> 
    int FilterIp(T &t);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       t 
     * @return      int 
     */
    template <typename T> 
    int filter_server_ip(T & t);

    /**
     * @brief       
     * 
     * @tparam T 
     * @param       t 
     * @return      true 
     * @return      false 
     */
    template <typename T>
    bool FilterBool(T  &t);

    /**
     * @brief       
     * 
     * @param       log 
     * @return      int 
     */
    int FilterLog(Log &log);

    /**
     * @brief       
     * 
     * @param       httpcallback 
     * @return      int 
     */
    int handleFilterHttpCallback(HttpCallback &httpcallback);

    /**
     * @brief       Get the All Server Address object
     * 
     */
    void GetAllServerAddress();

    std::string Get_RequestIp(const std::string& host, const std::string& path, int port);

    std::string execute_based_on_global_options();

private:
    /**
     * @brief       
     * 
     * @param       json 
     * @return      int 
     */
    int _Parse(nlohmann::json json);

    /**
     * @brief       
     * 
     * @return      int 
     */
    int _Verify();

    /**
     * @brief       
     * 
     * @return      int 
     */
    int _Filter();

    /**
     * @brief       
     * 
     */
    void _Check();

    std::vector<std::string> _ReadTrackerIPs();

private:

    Info _info;
    std::string _ip;
    Log _log;
    bool _rpc;
    HttpCallback _httpCallback;
    uint32_t _httpPort;
    std::set<std::string> _server;
    uint32_t _serverPort;
    std::string _version;
    std::thread _thread;
    std::atomic<bool> _exitThread{false};
    std::vector<std::string> sentinelNode = _ReadTrackerIPs();

};

#endif