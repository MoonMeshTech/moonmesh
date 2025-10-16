#include "expected.h"
#include "utils/tmp_log.h"
#include <strstream>

std::string mmc::Error::to_log(){
     std::stringstream ss;
    ss << Sutil::Format("%s:\n", error_leve_string[leve]);
    int all_size=error_trace_.size();
    for(int i=0;i<all_size;i++){
        auto et=error_trace_.top();
        ss <<Sutil::Format("\t%s:%s:%s\t-> {%s}:{%s}\n",et.file, et.line,et.func,et.error_str, et.debug_data);
        error_trace_.pop();
    }
    return ss.str();
}