#pragma once


#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <stack>
#include <zlib.h>
//#include "utils/qrcode.h"
#include "utils/tmp_log.h"
#include <variant>
#include <boost/stacktrace.hpp>
namespace mmc{


enum ERROR_LEVE{
    ERROR_TEMP,
    ERROR_WARING,
    ERROR_DEBUG,
    ERROR_BAD,
};

const static std::string error_leve_string[]={
    "ERROR_TEMP",
    "ERROR_WARING",
    "ERROR_DEBUG",
    "ERROR_BAD"
};




class ErrorInterface;

class Error{
public:
    struct ErrorMassage{
        uint64_t line;
        std::string file;
        std::string func;
        std::string error_str;
        uint64_t code;
        std::string debug_data;
    };

protected:
    ERROR_LEVE leve=ERROR_BAD;
    std::stack<ErrorMassage> error_trace_;
    friend ErrorInterface;
protected:
public:
    Error()=default;
    Error(mmc::ERROR_LEVE leve){
        this->leve=leve;
     }
    Error(Error const & )=default;
    Error(Error &&ohter){
        this->leve=ohter.leve;
        this->error_trace_=std::move(ohter.error_trace_);
    }
    Error & operator=(Error const &) = delete;
    Error & operator=(Error &&ohter ){
        this->leve=ohter.leve;
        this->error_trace_=std::move(ohter.error_trace_);
       
    }
     

    std::string to_log();
   
   
    virtual ~Error() = default;
    
};


class ErrorInterface{
    protected:
    ERROR_LEVE leve;
    public:
    


    template<typename... ARGS>
    std::shared_ptr<Error> trace(const std::string & file,uint64_t line,const std::string & func,const std::string & k,const ARGS &... args){
        std::shared_ptr<Error> ret=std::make_shared<Error>();
        std::string debug_data=Sutil::Format(k,args...);
        auto code_str=code_to_string();
        Error::ErrorMassage msg;
        msg.code=(uint64_t)code_str.first;
        msg.error_str=code_str.second;
        msg.file=file;
        msg.line=line;
        msg.func=func;
        msg.debug_data=debug_data;
        ret->error_trace_.push(msg);
        ret->leve=leve;
        return ret;
    }


    std::shared_ptr<Error> trace(const std::string & file,uint64_t line,const std::string & func){
        std::shared_ptr<Error> ret=std::make_shared<Error>();
        auto code_str=code_to_string();
        Error::ErrorMassage msg;
        msg.code=(uint64_t)code_str.first;
        msg.error_str=code_str.second;
        msg.file=file;
        msg.line=line;
        msg.func=func;
        ret->leve=leve;
        ret->error_trace_.push(msg);
        return ret;
    }


    template<typename... ARGS>
    std::shared_ptr<Error> trace( std::shared_ptr<Error> ret,const std::string & file,uint64_t line,const std::string & func,const std::string & k,const ARGS &... args){
        std::string debug_data=Sutil::Format(k,args...);
        auto code_str=code_to_string();
        Error::ErrorMassage msg;
        msg.code=(uint64_t)code_str.first;
        msg.error_str=code_str.second;
        msg.file=file;
        msg.line=line;
        msg.func=func;
        msg.debug_data=debug_data;
        ret->error_trace_.push(msg);
        return ret;
    }


    std::shared_ptr<Error> trace(std::shared_ptr<Error> ret,const std::string & file,uint64_t line,const std::string & func){
        auto code_str=code_to_string();
        Error::ErrorMassage msg;
        msg.code=(uint64_t)code_str.first;
        msg.error_str=code_str.second;
        msg.file=file;
        msg.line=line;
        ret->error_trace_.push(msg);
        return ret;
    }

    


    virtual std::pair<uint64_t,std::string> code_to_string()const =0;

};

#define ENABLEDEBUGLOG 1


template< typename T>
class Expected{
    private:
    bool has_error;
    std::variant<T,std::shared_ptr<Error>> Data;
    public:
     Expected(const T && v) : has_error(false), Data(std::move(v)) {}
     Expected(const T & v) : has_error(false), Data(v) {}
     //Expected(const E & err): has_error(true), Data(std::make_shared<E>(std::move(err))) {}
     Expected(Error &err):has_error(true){
        Data=std::make_shared<Error>(std::move(err));
#if ENABLEDEBUGLOG
        std::cout << std::get<std::shared_ptr<Error>>(Data)->to_log() << std::endl;
#endif
     }
     Expected(std::shared_ptr<Error> && _error):has_error(true),Data(std::move(_error)){
#if ENABLEDEBUGLOG
        std::cout << std::get<std::shared_ptr<Error>>(Data)->to_log() << std::endl;
#endif
     }
    ~Expected() {
       
    }
    Expected(Expected const &) =delete;
    Expected & operator=(Expected const &) =delete;

    Expected(Expected && other) noexcept : has_error(other.has_error) {
       
        Data=std::move(other.Data);
    }
    Expected & operator=(Expected && other) noexcept {
        
        Data=std::move(other.Data);
        return *this;
    }
    std::shared_ptr<Error> get_error() {
        if (!has_error) {
            std::cerr<< boost::stacktrace::stacktrace() << std::endl;
           
            throw std::runtime_error("Expected does not contain an error");
        }
        return std::get<std::shared_ptr<Error>>(Data);
    }
    
    T & unwrap() {
        if (has_error) {
           std::cerr<< boost::stacktrace::stacktrace() << std::endl;
           
            throw std::runtime_error("Expected contains an error");
        }
        return std::get<T>(Data);
    }

    [[nodiscard]] bool is_ok() const {
        return !has_error;
    }
    [[nodiscard]] bool is_error() const {
        return has_error;
    }
};








}


#define EMATE(...) __FILE__,__LINE__,__PRETTY_FUNCTION__,##__VA_ARGS__


#define NET __FILE__,__LINE__,__PRETTY_FUNCTION__

#define MEXPEAND(x) x

#define RE(E_TYPE,E_CODE,...)\
    E_TYPE(E_CODE).trace( MEXPEAND( NET ) ,##__VA_ARGS__)

#define TRACE(E_TYPE,E_CODE,OTHER,...)\
    E_TYPE(E_CODE).trace(OTHER,MEXPEAND( NET ) ,##__VA_ARGS__)