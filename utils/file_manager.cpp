#include "file_manager.h"
#include "utils/tmp_log.h"

static FileManager g_fileManager;

FileManager& GetGlobalFileManager() {
    return g_fileManager;
}

bool FileManager::EnsureDirectoryExists(const std::string& path) {
    if(access(path.c_str(), F_OK)) {
        if(mkdir(path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH)) {
            ERRORLOG("mkdir {} fail!", path);
            return false;
        }
    }
    return true;
}

std::string FileManager::GetKeystoreFilePath(const std::string& address) const {
    return path_constants::KEYSTORE_PATH + address + ".json";
}

std::string FileManager::getContractFilePath(const std::string& contractName) const {
    return path_constants::CONTRACT_PATH + contractName;
} 