#ifndef COMMON_LOGGING_H_
#define COMMON_LOGGING_H_

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <mutex>
#include <utils/magic_singleton.h>
#include <map>

typedef enum {
    LOGMAIN = 0,
    LOGEND  = 1
} LOGSINK;


static std::map<std::string,spdlog::level::level_enum> levelMap
{
    {"trace",spdlog::level::level_enum::trace},
    {"debug",spdlog::level::level_enum::debug},
    {"info",spdlog::level::level_enum::info},
    {"warn",spdlog::level::level_enum::warn},
    {"err",spdlog::level::level_enum::err},
    {"critical",spdlog::level::level_enum::critical},
    {"off",spdlog::level::level_enum::off},
};

class Log
{
public:
    Log(){}
    ~Log()
    {  
    for(auto& logger : _logger) 
    {  
        logger.reset(); 
    }
    _logMutex.lock();
    spdlog::shutdown();
    _logMutex.unlock(); 
    }   

/**
 * @brief       Init the log string
 * 
 * @param       path 
 * @param       console_out 
 * @param       level 
 * @return      true 
 * @return      false 
 */
bool LogInit(const std::string &path, bool console_out = false, const std::string &level = "OFF");

/**
 * @brief       Init the log level
 * 
 * @param       path 
 * @param       console_out 
 * @param       level 
 * @return      true 
 * @return      false 
 */
bool LogInit(const std::string &path, bool console_out = false, spdlog::level::level_enum level = spdlog::level::off);


/**
 * @brief  Close the log
 */
void LogDeinit();

/**
 * @brief       Get the Log Level object
 * 
 * @param       level 
 * @return      spdlog::level::level_enum 
 */
spdlog::level::level_enum GetLogLevel(const std::string & level);

/**
 * @brief       Get the Sink object
 * 
 * @param       sink 
 * @return      std::shared_ptr<spdlog::logger> 
 */
std::shared_ptr<spdlog::logger> GetSink(LOGSINK sink);

private:
    std::mutex _logMutex;
    std::shared_ptr<spdlog::logger> _logger[LOGEND] = {nullptr};//_logger
    const std::vector<std::string>  _loggerType = {"mm","net","ca","contract"};//_loggerType
    const std::vector<std::string>  _loggerLevel = {"trace","debug","info","warn","error","critical","off"};//_loggerLevel
    std::vector<spdlog::sink_ptr>   _sinks;
};


/**
 * @brief       
 * 
 * @param       signum 
 */
void SystemErrorHandler(int signum);

std::string formatColorLog(spdlog::level::level_enum level);
#define LOGSINK(sink, level, format, ...)                                                                       \
    do {                                                                                                        \
       auto sink_ptr = MagicSingleton<Log>::GetInstance()->GetSink(sink);                                   \
        if(nullptr != sink_ptr){                                                                                \
            sink_ptr->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, level, format,##__VA_ARGS__);\
        }\
   } while (0);                                                                                                 \

#define traceLogSink(sink, format, ...) LOGSINK(sink, spdlog::level::trace, format, ##__VA_ARGS__)
#define DEBUGLOGSINK(sink, format, ...) LOGSINK(sink, spdlog::level::debug, format, ##__VA_ARGS__)
#define INFOLOGSINK(sink, format, ...) LOGSINK(sink, spdlog::level::info, format, ##__VA_ARGS__)
#define WARNING_LOG_SINK(sink, format, ...) LOGSINK(sink, spdlog::level::warn, format, ##__VA_ARGS__)
#define ERRORLOGSINK(sink, format, ...) LOGSINK(sink, spdlog::level::err, format, ##__VA_ARGS__)
#define CRITICAL_LOG_SINK(sink, format, ...) LOGSINK(sink, spdlog::level::critical, format, ##__VA_ARGS__)

#define TRACE_RAW_DATA_LOG_SINK(sink, prefix, container) traceLogSink(sink, prefix + std::string(":{}"), spdlog::to_hex(container))
#define DEBUG_RAW_DATA_LOG_SINK(sink, prefix, container) DEBUGLOGSINK(sink, prefix + std::string(":{}"), spdlog::to_hex(container))
#define INFO_RAW_DATA_LOG_SINK(sink, prefix, container) INFOLOGSINK(sink, prefix + std::string(":{}"), spdlog::to_hex(container))
#define WARN_RAW_DATA_LOG_SINK(sink, prefix, container) WARNING_LOG_SINK(sink, prefix + std::string(":{}"), spdlog::to_hex(container))
#define ERROR_RAW_DATA_LOG_SINK(sink, prefix, container) ERRORLOGSINK(sink, prefix + std::string(":{}"), spdlog::to_hex(container))
#define critical_raw_data_log_sink(sink, prefix, container) CRITICAL_LOG_SINK(sink, prefix + std::string(":{}"), spdlog::to_hex(container))

#define TRACELOG(format, ...) traceLogSink(LOGMAIN, format, ##__VA_ARGS__)
#define DEBUGLOG(format, ...) DEBUGLOGSINK(LOGMAIN, format, ##__VA_ARGS__)
#define INFOLOG(format, ...) INFOLOGSINK(LOGMAIN, format, ##__VA_ARGS__)
#define WARNLOG(format, ...) WARNING_LOG_SINK(LOGMAIN, format, ##__VA_ARGS__)
#define ERRORLOG(format, ...) ERRORLOGSINK(LOGMAIN, format, ##__VA_ARGS__)
#define CRITICALLOG(format, ...) CRITICAL_LOG_SINK(LOGMAIN, format, ##__VA_ARGS__)

#define TRACE_RAW_DATA_LOG(prefix, container) TRACE_RAW_DATA_LOG_SINK(LOGMAIN, prefix, container)
#define DEBUG_RAW_DATA_LOG(prefix, container) DEBUG_RAW_DATA_LOG_SINK(LOGMAIN, prefix, container)
#define INFO_FORWARD_LOG(prefix, container) INFO_RAW_DATA_LOG_SINK(LOGMAIN, prefix, container)
#define LOG_WARN_RAW_DATA(prefix, container) WARN_RAW_DATA_LOG_SINK(LOGMAIN, prefix, container)
#define kErrorRawDataLog(prefix, container) ERROR_RAW_DATA_LOG_SINK(LOGMAIN, prefix, container)
#define CRITICAL_RAW_DATA_LOG(prefix, container) critical_raw_data_log_sink(LOGMAIN, prefix, container)

#endif
