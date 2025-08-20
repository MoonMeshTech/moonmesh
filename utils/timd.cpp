#include "timd.h"


std::unique_ptr<AsyncLogger> AsyncLogger::instance_;
std::mutex AsyncLogger::initMutex_;
bool AsyncLogger::initialized_ = false;