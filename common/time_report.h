/**
 * *****************************************************************************
 * @file        time_report.h
 * @brief       
 * @author  ()
 * @date        2023-09-28
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef TIME_REPORT_HEADER_GUARD
#define TIME_REPORT_HEADER_GUARD

#include <string>
#include <cstdint>

class TimeReport
{
public:
    TimeReport();
    ~TimeReport();
    TimeReport(TimeReport&&) = delete;
    TimeReport& operator=(const TimeReport&) = delete;
    TimeReport& operator=(TimeReport&&) = delete;

public:
    
    /**
     * @brief       
     * 
     */
    void Init();
    
    /**
     * @brief       
     * 
     */
    void End();
    
    /**
     * @brief       
     * 
     */
    void Report();
    
    /**
     * @brief       
     * 
     * @param       title 
     */
    void Report(const std::string& title);
    
    /**
     * @brief       Get the Start object
     * 
     * @return      uint64_t 
     */
    uint64_t GetStart() { return _start; }

    /**
     * @brief       Get the End object
     * 
     * @return      uint64_t 
     */
    uint64_t GetEnd() { return _end; }

private:
    std::string _title;
    uint64_t _start;
    uint64_t _end;
};

#endif // TIME_REPORT_HEADER_GUARD