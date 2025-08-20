#ifndef _FILE_MANAGER_H_
#define _FILE_MANAGER_H_

#include <string>
#include <unistd.h>
#include <sys/stat.h>

namespace path_constants {
    const std::string KEYSTORE_PATH = "./keystore/";
    const std::string CONTRACT_PATH = "./contract/";
}

class FileManager {
public:
    FileManager() = default;
    ~FileManager() = default;

    bool EnsureDirectoryExists(const std::string& path);

    std::string GetKeystoreFilePath(const std::string& address) const;

    std::string getContractFilePath(const std::string& contractName) const;
};

FileManager& GetGlobalFileManager();

#endif // _FILE_MANAGER_H_ 