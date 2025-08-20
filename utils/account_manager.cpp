#include "account_manager.h"
#include <evmone/evmone.h>
#include "contract_utils.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/core_names.h>
#include "openssl/rand.h"
#include "utils/hex_code.h"
#include <dirent.h>
#include <string>
#include <charconv>
#include "../utils/keccak_cryopp.hpp"
#include "verified_address.h"
#include "address_cache.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <termios.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>
#include <random>
#include <sstream>
#include <limits>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <functional>

void DisplayProgressBar(int current, int total, int barWidth) {
    float progress = (float)current / total;
    int pos = barWidth * progress;
    
    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    
    int percent = static_cast<int>(progress * 100.0);
    std::cout << "] " << current << "/" << total << " (" << percent << "%) " << std::flush;
}

//Account Initialize 
Account::Account(const Account& other):
    _pkey(nullptr, &EVP_PKEY_free),
    _pubStr(other._pubStr),
    _priStr(other._priStr),
    _Addr(other._Addr)
{
    if (other._pkey) {
        EVP_PKEY *pkeyDup = EVP_PKEY_dup(other._pkey.get());
        _pkey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(pkeyDup, &EVP_PKEY_free);
    }
}

Account::Account(Account&& other) noexcept :
    _pkey(std::move(other._pkey)),
    _pubStr(std::move(other._pubStr)),
    _priStr(std::move(other._priStr)),
    _Addr(std::move(other._Addr))
    {}
//self copy
Account& Account::operator=(const Account& other)
{
    if (this != &other) {
        _pubStr = other._pubStr;
        _priStr = other._priStr;
        _Addr = other._Addr;
        if (other._pkey) {
            EVP_PKEY *pkeyDup = EVP_PKEY_dup(other._pkey.get());
            _pkey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(pkeyDup, &EVP_PKEY_free);
        } else {
            _pkey.reset();
        }

    }
    return *this;
}
//self copy
Account& Account::operator=(Account&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    _pkey = std::move(other._pkey);
    _pubStr = std::move(other._pubStr);
    _priStr = std::move(other._priStr);
    _Addr = std::move(other._Addr);
    return *this;
}

bool Account::Sign(const std::string &message, std::string &signature)
{
    EVP_MD_CTX *mdctx = NULL;
    const char * sig_name = message.c_str();
    unsigned char *sigValue = NULL;
    size_t sig_len = strlen(sig_name);

    ON_SCOPE_EXIT{
        if(mdctx != NULL){EVP_MD_CTX_free(mdctx);}
        if(sigValue != NULL){OPENSSL_free(sigValue);}
    };

    // Create the Message Digest Context 
    if(!(mdctx = EVP_MD_CTX_new())) 
    {
        return false;
    }
    
    // Initialise the DigestSign operation
    if(1 != EVP_DigestSignInit(mdctx, NULL, NULL, NULL, _pkey.get())) 
    {
        return false;
    }

    size_t message_length = 0;
    if( 1 != EVP_DigestSign(mdctx, NULL, &message_length, (const unsigned char *)sig_name, sig_len))
    {
        return false;
    }

    sigValue = (unsigned char *)OPENSSL_malloc(message_length);

    if( 1 != EVP_DigestSign(mdctx, sigValue, &message_length, (const unsigned char *)sig_name, sig_len))
    {
        return false;
    }
    
    std::string hashString((char*)sigValue, message_length);
    signature = hashString;

    return true;
}

bool Account::Verify(const std::string &message, std::string &signature)
{
    EVP_MD_CTX *mdctx = NULL;
    const char *msg = message.c_str();
    unsigned char *sig = (unsigned char *)signature.data();
    size_t slen = signature.size();
    size_t msg_len = strlen(msg);

    if(!(mdctx = EVP_MD_CTX_new())) 
    {
        return false;
    }
    

    /* Initialize `key` with a public key */
    if(1 != EVP_DigestVerifyInit(mdctx, NULL, NULL, NULL, _pkey.get())) 
    {
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    if (1 != EVP_DigestVerify(mdctx, sig, slen ,(const unsigned char *)msg, msg_len)) 
    {
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    EVP_MD_CTX_free(mdctx);
    return true;
}

void Account::generatePublicString(EVP_PKEY* pkeyPtr)
{
    //The binary of the resulting public key is stored in a string serialized
    if(!_pkey)
    {
        return;
    }

    unsigned char *pkeyDer = NULL;
    int publen = i2d_PUBKEY(pkeyPtr ,&pkeyDer);

    for(int i = 0; i < publen; ++i)
    {
        _pubStr += pkeyDer[i];
    }
    OPENSSL_free(pkeyDer);
}

void Account::generatePrivateString(EVP_PKEY* pkeyPtr)
{
    size_t len = 80;
    char pkeyData[80] = {0};

    if( EVP_PKEY_get_raw_private_key(pkeyPtr, (unsigned char *)pkeyData, &len) == 0)
    {
        return;
    }

    std::string data(reinterpret_cast<char*>(pkeyData), len);
    _priStr = data;
}


void Account::_GenerateAddr()
{
    _Addr = GenerateAddr(_pubStr);
}

void Account::_GenerateRandomSeed(uint8_t _seed[PRIME_SEED_NUM])
{
    RAND_bytes(_seed,PRIME_SEED_NUM);
}

void Account::generatePkey()
{
    _pkey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(EVP_PKEY_new(), &EVP_PKEY_free);
    EVP_PKEY* pkeyPtr = _pkey.get();
    //do we neet do continue a sha256
    uint8_t seed[PRIME_SEED_NUM] = {};
    _GenerateRandomSeed(seed); 

    uint8_t outputArr[SHA256_DIGEST_LENGTH];
    Sha256Hash(seed, PRIME_SEED_NUM, outputArr);

    pkeyPtr = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, outputArr, PRIME_SEED_NUM);
    if (!pkeyPtr) {
    ERRORLOG("Failed to create OpenSSL private key from seed.");
    }
    if(!_pkey)
    {
        ERRORLOG("generatepk false");
        return;
    }
    _pkey.reset(pkeyPtr);
    generatePublicString(pkeyPtr);   
    generatePrivateString(pkeyPtr);
    _GenerateAddr();
}

void Account::generateFilePkey(const std::string &address)
{
    _pkey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(EVP_PKEY_new(), &EVP_PKEY_free);
    std::string privFileFormat = path_constants::KEYSTORE_PATH + address;
    const char * priPath = privFileFormat.c_str();

    BIO* privateBioFile = BIO_new_file(priPath, "r");
    if (!privateBioFile)
    {
        BIO_free(privateBioFile);
        printf("Error: privateBioFile err\n");
        return;
    }

    std::string hexPrivateKey;
    if (ReadPrivateKey(privFileFormat, hexPrivateKey) != 0)
    {
        return;
    }
    EVP_PKEY *pkeyPtr = HexPrivateKeyToPkey(hexPrivateKey);

    if(!_pkey)
    {
        std::cout << "_pkey read from file false" << std::endl;
        return;
    }
    _pkey.reset(pkeyPtr);
    generatePublicString(pkeyPtr);
    generatePrivateString(pkeyPtr);
    _Addr = address;
}

int AccountManager::AddAccount(Account &account)
{
    auto iter = _accountList.find(account.GetAddr());
    if(iter != _accountList.end())
    {
        std::cout << "addresss repeat" << std::endl;
        return -1;
    }
    _accountList.emplace(account.GetAddr(), std::make_shared<Account>(account));
    return 0;
}

void AccountManager::PrintAllAccount() const
{
    auto iter = _accountList.begin();
    while(iter != _accountList.end())
    {
        if (iter->first == _defaultAddress)
        {
            
            std::cout << "0x"+ iter->first<< " [default]" << std::endl;
        }
        else
        {
            std::cout << "0x"+iter->first << std::endl;
        }
        ++iter;
    }
}

int AccountManager::DeleteAccount(const std::string& addr)
{
    if(addr == _defaultAddress) {
        ERRORLOG("Cannot delete default account");
        return -1; 
    }

    auto iter = _accountList.find(addr);
    if (iter == _accountList.end()) {
        ERRORLOG("Account not found: {}", addr);
        return -2; 
    }

    try {
        _accountList.erase(iter);
    } catch (const std::exception& e) {
        ERRORLOG("Exception when removing account from list: {}", e.what());
        return -3; 
    }

    try {
        std::string keystoreFile = GetGlobalFileManager().GetKeystoreFilePath(addr);

        if (!std::ifstream(keystoreFile).good()) {
            WARNLOG("Keystore file not found for account: {}", addr);
            return 1;
        }

        if (std::remove(keystoreFile.c_str()) != 0) {
            ERRORLOG("Failed to delete keystore file: {}", addr);
            return -4; 
        }
        
        DEBUGLOG("Account deleted successfully: {}", addr);
        return 0;
    } catch (const std::exception& e) {
        ERRORLOG("Exception when deleting keystore file: {}", e.what());
        return -5; 
    }
}

void AccountManager::SetDefaultAddr(const std::string & address)
{
    _defaultAddress = address;
}

std::string AccountManager::GetDefaultAddr() const
{
    return _defaultAddress;
}

int AccountManager::SetDefaultAccount(const std::string & address)
{
    if (_accountList.size() == 0)
    {
        return -1;
    }

    if (address.size() == 0)
    {
        _defaultAddress = _accountList.begin()->first;
        return 0;
    }

    auto iter = _accountList.find(address);
    if(iter == _accountList.end())
    {
        ERRORLOG("not found address {} in the _accountList ",address);
        return -2;
    }
    _defaultAddress = address;
    
    return 0;
}

bool AccountManager::IsExist(const std::string & address)
{
    auto iter = _accountList.find(address);
    if(iter == _accountList.end())
    {
        return false;
    }
    return true;
}

int AccountManager::FindAccount(const std::string & address, Account & account)
{
    auto iter = _accountList.find(address);
    if(iter == _accountList.end())
    {
        ERRORLOG("not found address {} in the _accountList ",address);
        return -1;
    }
    
    account = *iter->second;
    return 0;
}

int AccountManager::GetDefaultAccount(Account & account)
{
    auto iter = _accountList.find(_defaultAddress);
    if(iter == _accountList.end())
    {
        ERRORLOG("not found DefaultKeyBs58Addr {} in the _accountList ", _defaultAddress);
        return -1;
    }
    account = *iter->second;

    return 0;
}

void AccountManager::GetAccountList(std::vector<std::string> & _list)
{
    auto iter = _accountList.begin();
    while(iter != _accountList.end())
    {
        _list.push_back(iter->first);
        iter++;
    }
}


bool AccountManager::getAccountPublicKeyByBytes(const std::string &pubStr, Account &account)
{
    unsigned char* bufPtr = (unsigned char *)pubStr.data();
    const unsigned char *pk_str = bufPtr;
    int lenPtr = pubStr.size();
    
    if(lenPtr == 0)
    {
        ERRORLOG("public key Binary is empty");
        return false;
    }

    EVP_PKEY *peerPubKey = d2i_PUBKEY(NULL, &pk_str, lenPtr);

    if(peerPubKey == nullptr)
    {
        return false;
    }
    account.SetKey(peerPubKey);

    return true;
}

int AccountManager::ImportHexPrivateKey(const std::string & hexPrivateKey, std::string &address) {
    std::string pubStr_;
    std::string privateString;
    EVP_PKEY *pkey = HexPrivateKeyToPkey(hexPrivateKey);
    if(pkey == nullptr) {
        return -1;
    }
    
    unsigned char *pkeyDer = NULL;
    int publen = i2d_PUBKEY(pkey ,&pkeyDer);

    for(int i = 0; i < publen; ++i)
    {
        pubStr_ += pkeyDer[i];
    }
    size_t len = 80;
    char pkeyData[80] = {0};

    if( EVP_PKEY_get_raw_private_key(pkey, (unsigned char *)pkeyData, &len) == 0)
    {
        return -2;
    }

    std::string data(reinterpret_cast<char*>(pkeyData), len);
    privateString = data;
    
    address = GenerateAddr(pubStr_);
    Account acc;
    acc.SetKey(pkey);
    acc.SetPubStr(pubStr_);
    acc.setPriorityString(privateString);
    acc.SetAddr(address);

    std::cout << "final pubStr " << Str2Hex(acc.GetPubStr()) << std::endl;
    std::cout << "final priStr " << Str2Hex(acc.GetPriStr()) << std::endl;

    MagicSingleton<AccountManager>::GetInstance()->AddAccount(acc);
    
    return 0;
}

std::string Getsha256hash(const std::string & text)
{
	unsigned char mdStr[65] = {0};
	SHA256((const unsigned char *)text.c_str(), text.size(), mdStr);
 
	char tmp[3] = {0};

    std::string encodedHexStr;
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		sprintf(tmp, "%02x", mdStr[i]);
        encodedHexStr += tmp;
	}

    return encodedHexStr;
}

accountInitResult AccountManager::Initialize() {
    try {
        if (!GetGlobalFileManager().EnsureDirectoryExists(path_constants::KEYSTORE_PATH)) {
            return accountInitResult::Error(
                accountInitResult::Status::FATAL_ERROR,
                "Failed to create keystore directory"
            );
        }

        std::vector<std::string> keystoreFiles;
        auto scanResult = _scanKeystoreDirectory(keystoreFiles);
        if (scanResult.status != accountInitResult::Status::SUCCESS) {
            return scanResult;
        }

        if (keystoreFiles.empty()) {
            return accountInitResult::Error(
                accountInitResult::Status::EMPTY_KEYSTORE,
                "No keystore files found"
            );
        }

        auto loadResult = loadExistingAccounts(keystoreFiles);
        if (loadResult.status != accountInitResult::Status::SUCCESS) {
            return loadResult;
        }

        if (_accountList.empty()) {
            return accountInitResult::Error(
                accountInitResult::Status::kAccountsNotInitialized,
                "No accounts available after loading"
            );
        }
        
        if (IsExist(global::ca::GetInitAccountAddr())) {
            if (SetDefaultAccount(global::ca::GetInitAccountAddr()) != 0) {
                return accountInitResult::Error(
                    accountInitResult::Status::FATAL_ERROR,
                    "Failed to set default account"
                );
            }
        } else {
            SetDefaultAddr(_accountList.begin()->first);
        }

        bool inconsistencyFound = false;
        for (const auto& pair : _accountList) {
            std::string keystoreFile = GetGlobalFileManager().GetKeystoreFilePath(pair.first);
            if (!std::ifstream(keystoreFile).good()) {
                ERRORLOG("Inconsistency found: Account {} has no corresponding keystore file", pair.first);
                inconsistencyFound = true;
            }
        }
        
        if (inconsistencyFound) {
            return accountInitResult::Error(
                accountInitResult::Status::FATAL_ERROR,
                "System state is inconsistent. Some accounts may not have keystore files."
            );
        }
        
        return accountInitResult::Success();
    }
    catch (const std::exception& e) {
        return accountInitResult::Error(
            accountInitResult::Status::FATAL_ERROR,
            std::string("Exception during initialization: ") + e.what()
        );
    }
}

accountInitResult AccountManager::_scanKeystoreDirectory(std::vector<std::string>& keystoreFiles) {
    try {
        DIR *dir;
        struct dirent *ptr;
        
        if ((dir = opendir(path_constants::KEYSTORE_PATH.c_str())) == NULL) {
            return accountInitResult::Error(
                accountInitResult::Status::FATAL_ERROR,
                "Failed to open keystore directory"
            );
        }
        
        // Only search for .json files
        while ((ptr = readdir(dir)) != NULL) {
            if(strcmp(ptr->d_name,".") == 0 || strcmp(ptr->d_name, "..") == 0) {
                continue;
            }
            
            std::string filename(ptr->d_name);
            if(filename.size() == 0) {
                continue;
            }
            
            // Only process .json files
            if(filename.find(".json") != std::string::npos) {
                keystoreFiles.push_back(path_constants::KEYSTORE_PATH + "/" + filename);
            }
        }
        closedir(dir);
        
        return accountInitResult::Success();
    }
    catch (const std::exception& e) {
        return accountInitResult::Error(
            accountInitResult::Status::FATAL_ERROR,
            std::string("Exception during keystore scanning: ") + e.what()
        );
    }
}

accountInitResult AccountManager::loadExistingAccounts(const std::vector<std::string>& keystoreFiles) {
    try {
        int totalAccounts = keystoreFiles.size();
        int successCount = 0;
        std::vector<std::string> failedAccounts;
        
        std::cout << "Found " << totalAccounts << " account(s) to load." << std::endl;

        const int BATCH_THRESHOLD = 10; 
        const int MAX_THREADS = 8;      
        bool useBatchMode = false;
        
        if (totalAccounts > BATCH_THRESHOLD) {
            char batchOption = 'n';
            std::cout << "Large number of accounts detected (" << totalAccounts << "). Use multi-threaded batch processing? (y/n): ";
            std::cin >> batchOption;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            useBatchMode = (batchOption == 'y' || batchOption == 'Y');
        }
        
        if(global::GetBuildType() == GenesisConfig::BuildType::BUILD_TYPE_DEV){
            char useSharedPassword = 'n';
            if (totalAccounts > 1) {
                std::cout << "Development mode: Would you like to use a single password for all accounts? (y/n): ";
                std::cin >> useSharedPassword;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
            
            if ((totalAccounts > 1 && (useSharedPassword == 'y' || useSharedPassword == 'Y')) || totalAccounts == 1) {
                int maxAttempts = 3;
                int attemptCount = 0;
                bool allAccountsLoaded = false;
                
                while (attemptCount < maxAttempts && !allAccountsLoaded) {
                    std::string prompt = "Enter common password" + (attemptCount > 0 ? 
                        " (attempt " + std::to_string(attemptCount+1) + "/" + std::to_string(maxAttempts) + ")" : "") + ": ";
                        
                    std::string commonPassword = GetPasswordFromUser(false, prompt);
                    
                    std::vector<std::string> successfulAddressList;
                    std::vector<std::string> failedAddresses;
                    std::mutex resultMutex;
                    
                    if (useBatchMode) {

                        std::cout << "Using multi-threaded batch unlocking..." << std::endl;
                        
 
                        int numThreads = std::min(MAX_THREADS, totalAccounts);
                        int filesPerThread = totalAccounts / numThreads;
                        int remainingFiles = totalAccounts % numThreads;
                        
                        std::vector<std::thread> threads;
                        std::atomic<int> progress(0);
                        
                        auto updateProgress = [&]() {
                            int current = ++progress;
                            if (current % 10 == 0 || current == totalAccounts) {
                                std::cout << "\rUnlocking progress: " << current << "/" << totalAccounts 
                                        << " (" << (current * 100 / totalAccounts) << "%)" << std::flush;
                            }
                        };
                        
                        auto processFiles = [&](int startIdx, int count) {
                            for (int i = 0; i < count; i++) {
                                int idx = startIdx + i;
                                const auto& file = keystoreFiles[idx];

                                std::string filename;
                                size_t lastSlash = file.find_last_of("/\\");
                                if (lastSlash != std::string::npos) {
                                    filename = file.substr(lastSlash + 1);
                                } else {
                                    filename = file;
                                }
                                
                                std::string address;
                                size_t dotPos = filename.find(".json");
                                if (dotPos != std::string::npos) {
                                    address = filename.substr(0, dotPos);
                                } else {
                                    address = filename;
                                }

                                if (load_account_from_key_store(file, commonPassword) == 0) {
                                    std::lock_guard<std::mutex> lock(resultMutex);
                                    successfulAddressList.push_back(address);
                                } else {
                                    std::lock_guard<std::mutex> lock(resultMutex);
                                    failedAddresses.push_back(address);
                                }
                                
                                updateProgress();
                            }
                        };

                        int startIdx = 0;
                        for (int i = 0; i < numThreads; i++) {
                            int count = filesPerThread + (i < remainingFiles ? 1 : 0);
                            threads.emplace_back(processFiles, startIdx, count);
                            startIdx += count;
                        }

                        for (auto& t : threads) {
                            t.join();
                        }
                        
                        std::cout << std::endl; 
                    } else {

                        std::cout << "Using single-thread unlocking..." << std::endl;
                        
                        int current = 0;
                        for (const auto& file : keystoreFiles) {
                            std::string filename;
                            size_t lastSlash = file.find_last_of("/\\");
                            if (lastSlash != std::string::npos) {
                                filename = file.substr(lastSlash + 1);
                            } else {
                                filename = file;
                            }
                            
                            std::string address;
                            size_t dotPos = filename.find(".json");
                            if (dotPos != std::string::npos) {
                                address = filename.substr(0, dotPos);
                            } else {
                                address = filename;
                            }

                            if (load_account_from_key_store(file, commonPassword) == 0) {
                                successfulAddressList.push_back(address);
                            } else {
                                failedAddresses.push_back(address);
                            }

                            current++;
                            if (current % 10 == 0 || current == totalAccounts) {
                                std::cout << "\rUnlocking progress: " << current << "/" << totalAccounts 
                                        << " (" << (current * 100 / totalAccounts) << "%)" << std::flush;
                            }
                        }
                        std::cout << std::endl; 
                    }

                    std::cout << "\n===== Account Unlocking Results =====" << std::endl;
                    std::cout << "Successfully unlocked: " << successfulAddressList.size() << " account(s)" << std::endl;
                    
                    if (!successfulAddressList.empty()) {
                        if (successfulAddressList.size() <= 10) {

                            std::cout << "\nSuccessfully unlocked accounts:" << std::endl;
                            for (const auto& addr : successfulAddressList) {
                                std::cout << "✓ 0x" << addr << std::endl;
                            }
                        } else {

                            std::cout << "\nSuccessfully unlocked accounts (showing first 10):" << std::endl;
                            for (int i = 0; i < 10; i++) {
                                std::cout << "✓ 0x" << successfulAddressList[i] << std::endl;
                            }
                            std::cout << "... and " << (successfulAddressList.size() - 10) << " more accounts" << std::endl;
                        }
                    }
                    
                    if (!failedAddresses.empty()) {
                        if (failedAddresses.size() <= 10) {

                            std::cout << "\nFailed to unlock accounts:" << std::endl;
                            for (const auto& addr : failedAddresses) {
                                std::cout << "✗ 0x" << addr << std::endl;
                            }
                        } else {

                            std::cout << "\nFailed to unlock accounts (showing first 10):" << std::endl;
                            for (int i = 0; i < 10; i++) {
                                std::cout << "✗ 0x" << failedAddresses[i] << std::endl;
                            }
                            std::cout << "... and " << (failedAddresses.size() - 10) << " more accounts" << std::endl;
                        }
                    }

                    if (successfulAddressList.size() == totalAccounts) {
                        allAccountsLoaded = true;
                        successCount = totalAccounts;
                        std::cout << "\nAll accounts have been successfully unlocked." << std::endl;
                        return accountInitResult::Success();
                    }

                    if (!failedAddresses.empty() && attemptCount < maxAttempts - 1) {
                        char retryOption;
                        std::cout << "\nSome accounts failed to unlock. Would you like to:" << std::endl;
                        std::cout << "1. Try again with a different common password" << std::endl;
                        std::cout << "2. Continue with the " << successfulAddressList.size() << " unlocked account(s)" << std::endl;
                        std::cout << "3. Switch to individual password mode" << std::endl;
                        std::cout << "Enter option (1-3): ";
                        std::cin >> retryOption;
                        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                        
                        if (retryOption == '2') {
                            successCount = successfulAddressList.size();
                            return accountInitResult::Success();
                        } else if (retryOption == '3') {
                            std::cout << "\nSwitching to individual password mode for remaining accounts..." << std::endl;
                            
                            std::vector<std::string> remainingFiles;
                            for (const auto& addr : failedAddresses) {
                                std::string failedFile = GetGlobalFileManager().GetKeystoreFilePath(addr);
                                remainingFiles.push_back(failedFile);
                            }

                            successCount = successfulAddressList.size();

                            failedAccounts = failedAddresses;

                            std::vector<std::string> files_to_process_remaining;
                            for (const auto& addr : failedAddresses) {
                                std::string failedFile = GetGlobalFileManager().GetKeystoreFilePath(addr);
                                files_to_process_remaining.push_back(failedFile);
                            }

                            auto additionalResult = loadExistingAccounts(files_to_process_remaining);
                            if (additionalResult.status == accountInitResult::Status::SUCCESS) {
                                successCount++;
                            }

                            return accountInitResult::Success();
                        }
                    }
                    
                    attemptCount++;
                }

                if (!allAccountsLoaded && attemptCount >= maxAttempts) {
                    char continueOption;
                    std::cout << "\nMaximum attempts reached. Would you like to:" << std::endl;
                    std::cout << "1. Continue with the " << (totalAccounts - failedAccounts.size()) << " unlocked account(s)" << std::endl;
                    std::cout << "2. Try individual passwords for remaining accounts" << std::endl;
                    std::cout << "Enter option (1-2): ";
                    std::cin >> continueOption;
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    
                    if (continueOption == '1') {
                        successCount = totalAccounts - failedAccounts.size();
                        return accountInitResult::Success();
                    }
                }
            }
    }

        if (useBatchMode && totalAccounts > BATCH_THRESHOLD) {
            UnlockAccountsBatchMode(keystoreFiles, totalAccounts, MAX_THREADS, successCount, failedAccounts);
        } else {
            for(int i = 0; i < totalAccounts; i++) {
                const auto& file = keystoreFiles[i];

                std::string filename;
                size_t lastSlash = file.find_last_of("/\\");
                if (lastSlash != std::string::npos) {
                    filename = file.substr(lastSlash + 1);
                } else {
                    filename = file;
                }
                
                std::string address;
                size_t dotPos = filename.find(".json");
                if (dotPos != std::string::npos) {
                    address = filename.substr(0, dotPos);
                } else {
                    address = filename;
                }
                
                std::cout << "[" << (i+1) << "/" << totalAccounts << "] Account: " << address << std::endl;
                
                int retryCount = 0;
                int maxRetries = 3; 
                bool accountLoaded = false;
                
                while (retryCount < maxRetries && !accountLoaded) {
                    std::string prompt = "Enter password" + (retryCount > 0 ? 
                        " (attempt " + std::to_string(retryCount+1) + "/" + 
                        std::to_string(maxRetries) + ")" : "") + ": ";
                        
                    std::string password = GetPasswordFromUser(false, prompt);
                    
                    if(load_account_from_key_store(file, password) == 0) {
                        std::cout << "Account unlocked successfully!" << std::endl;
                        accountLoaded = true;
                        successCount++;
                    } else {
                        retryCount++;
                        
                        if (retryCount < maxRetries) {
                            std::cout << "Incorrect password. Please try again." << std::endl;
                        } else {
                            std::cout << "Failed to unlock account after " << maxRetries << " attempts." << std::endl;
                            failedAccounts.push_back(address);

                            if (i < totalAccounts - 1) {
                                std::cout << "Options:" << std::endl;
                                std::cout << "1. Try again with this account" << std::endl;
                                std::cout << "2. Skip to next account" << std::endl;
                                
                                int choice = 2; 
                                std::cout << "Please enter your choice (1-2, default 2): ";
                                std::string input;
                                std::getline(std::cin, input);
                                if (!input.empty()) {
                                    try {
                                        choice = std::stoi(input);
                                    } catch (...) {
                                        choice = 2;
                                    }
                                }
                                
                                if (choice == 1) {
                                    retryCount = 0; 
                                    continue;
                                }
                            }
                        }
                    }
                }
            }
        }

        std::cout << std::endl << "Account loading summary:" << std::endl;
        std::cout << "- Total accounts: " << totalAccounts << std::endl;
        std::cout << "- Successfully loaded: " << successCount << std::endl;
        std::cout << "- Failed to load: " << failedAccounts.size() << std::endl;

        if (successCount == 0) {
            return accountInitResult::Error(
                accountInitResult::Status::kAccountsNotInitialized,
                "No accounts were successfully unlocked"
            );
        }
        
        return accountInitResult::Success();
    }
    catch (const std::exception& e) {
        return accountInitResult::Error(
            accountInitResult::Status::FATAL_ERROR,
            std::string("Exception during account loading: ") + e.what()
        );
    }
}

// Helper method for batch unlocking accounts with individual passwords
void AccountManager::UnlockAccountsBatchMode(
    const std::vector<std::string>& keystoreFiles, 
    int totalAccounts,
    int maxThreads,
    int& successCount,
    std::vector<std::string>& failedAccounts)
{
    std::cout << "Getting passwords for all accounts..." << std::endl;
    
    struct account_password_entry {
        std::string filePath;
        std::string password;
    };
    
    std::vector<account_password_entry> accountPasswords;
    for (int i = 0; i < totalAccounts; i++) {
        const auto& file = keystoreFiles[i];

        std::string filename;
        size_t lastSlash = file.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = file.substr(lastSlash + 1);
        } else {
            filename = file;
        }
        
        std::string address;
        size_t dotPos = filename.find(".json");
        if (dotPos != std::string::npos) {
            address = filename.substr(0, dotPos);
        } else {
            address = filename;
        }
        
        std::cout << "[" << (i+1) << "/" << totalAccounts << "] Account: " << address << std::endl;
        std::string prompt = "Enter password: ";
        std::string password = GetPasswordFromUser(false, prompt);
        accountPasswords.push_back({file, password});
    }

    std::cout << "\nUnlocking accounts in parallel..." << std::endl;

    std::atomic<int> progress(0);
    std::vector<std::thread> threads;
    int numThreads = std::min(maxThreads, totalAccounts);
    int filesPerThread = totalAccounts / numThreads;
    int remainingFiles = totalAccounts % numThreads;

    std::mutex resultMutex;

    auto processPasswordBatch = [this, &accountPasswords, &resultMutex, &progress, &successCount, &failedAccounts, totalAccounts](int startIdx, int count) {
        for (int i = 0; i < count; i++) {
            int idx = startIdx + i;
            if (idx >= totalAccounts) break;
            
            const auto& entry = accountPasswords[idx];
            const auto& file = entry.filePath;
            const auto& password = entry.password;

            std::string filename;
            size_t lastSlash = file.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                filename = file.substr(lastSlash + 1);
            } else {
                filename = file;
            }
            
            std::string address;
            size_t dotPos = filename.find(".json");
            if (dotPos != std::string::npos) {
                address = filename.substr(0, dotPos);
            } else {
                address = filename;
            }
            
            if (load_account_from_key_store(file, password) == 0) {
                std::lock_guard<std::mutex> lock(resultMutex);
                successCount++;
            } else {
                std::lock_guard<std::mutex> lock(resultMutex);
                failedAccounts.push_back(address);
            }

            int currentProgress = ++progress;
            DisplayProgressBar(currentProgress, totalAccounts, 50);
        }
    };

    int startIdx = 0;
    for (int i = 0; i < numThreads; i++) {
        int count = filesPerThread + (i < remainingFiles ? 1 : 0);
        threads.emplace_back(processPasswordBatch, startIdx, count);
        startIdx += count;
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    std::cout << std::endl; 
}

accountInitResult AccountManager::CreateInitialAccount() {
    return _createInitialAccountImpl();
}

accountInitResult AccountManager::_createInitialAccountImpl() {
    try {
        DEBUGLOG("Creating initial account");
        Account account(true);
        std::string address = account.GetAddr();
        DEBUGLOG("Generated new account with address: {}", address);

        if(AddAccount(account) != 0) {
            return accountInitResult::Error(
                accountInitResult::Status::CREATE_ACCOUNT_FAILED,
                "Failed to add account to account list"
            );
        }

        SetDefaultAccount(address);

        std::string password = GetPasswordFromUser(true, "Please set a password to encrypt your private key:");
        if(password.empty()) {
            auto iter = _accountList.find(address);
            if(iter != _accountList.end()) {
                _accountList.erase(iter);
            }
            
            return accountInitResult::Error(
                accountInitResult::Status::CREATE_ACCOUNT_FAILED,
                "Empty password provided"
            );
        }

        std::string privateKey = account.GetPriStr();
        if(privateKey.empty()) {

            auto iter = _accountList.find(address);
            if(iter != _accountList.end()) {
                _accountList.erase(iter);
            }
            
            return accountInitResult::Error(
                accountInitResult::Status::CREATE_ACCOUNT_FAILED,
                "Empty private key for account: " + address
            );
        }

        DEBUGLOG("Saving private key to keystore file");
        int saveResult = SavePrivateKeyToKeystoreFile(address, privateKey, password);
        if(saveResult != 0) {
            auto iter = _accountList.find(address);
            if(iter != _accountList.end()) {
                _accountList.erase(iter);
            }
            
            return accountInitResult::Error(
                accountInitResult::Status::CREATE_ACCOUNT_FAILED,
                "Failed to save private key to keystore file: " + address + 
                ". Error code: " + std::to_string(saveResult)
            );
        }
        
        DEBUGLOG("Successfully created initial account: {}", address);
        return accountInitResult::Success();
    } 
    catch (const std::exception& e) {
        return accountInitResult::Error(
            accountInitResult::Status::FATAL_ERROR,
            std::string("Exception in _createInitialAccount: ") + e.what()
        );
    }
}

bool isValidAddress(const std::string& address) 
{
    using namespace mm::addr;
    
    if(MagicSingleton<VerifiedAddress>::GetInstance()->isAddressVerified(address))
    {
        return true;
    }
    
    if (address.size() != 40)
    { 
        DEBUGLOG("addresss size is :",address.size());
        return false;
    }

    std::string originalAddress = address;
    std::transform(originalAddress.begin(), originalAddress.end(), originalAddress.begin(), ::tolower);

    std::string hash = Keccak256CrypterOpenSSL(originalAddress);
    std::transform(hash.begin(), hash.end(), hash.begin(), ::tolower);

    for (size_t i = 0; i < originalAddress.size(); ++i) {
        if ((hash[i] >= '8' && originalAddress[i] >= 'a' && originalAddress[i] <= 'f') && address[i] != std::toupper(originalAddress[i]))
        {
            DEBUGLOG("hash[i] >= 8");
            return false;
        }
        else if ((hash[i] < '8' || originalAddress[i] >= '0' && originalAddress[i] <= '9') && address[i] != originalAddress[i])
        {
            DEBUGLOG("hash[i] < 8");
            return false;
        }
            
    }
    MagicSingleton<VerifiedAddress>::GetInstance()->markAddressAsVerified(address);
    return true;
}

std::string GenerateAddr(const std::string& publicKey)
{
    using namespace mm::addr;

    std::string addrCache = MagicSingleton<AddressCache>::GetInstance()->getAddress(publicKey);
    if(addrCache != "")
    {
        return addrCache;
    }
    std::string hash = Keccak256CrypterOpenSSL(publicKey);

    std::string addr = hash.substr(hash.length() - 40);
    std::string checkSumAddr = evm_utils::ToChecksumAddress(addr);
    MagicSingleton<AddressCache>::GetInstance()->addPublicKey(publicKey, checkSumAddr);
    return checkSumAddr;
}

std::string Base64Encode(const std::string & dataSource)
{
    const auto predicted_len = 4 * ((dataSource.length() + 2) / 3);
    const auto output_buffer{std::make_unique<char[]>(predicted_len + 1)};
    const std::vector<unsigned char> vec_chars{dataSource.begin(), dataSource.end()};
    const auto output_len = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(output_buffer.get()), vec_chars.data(), static_cast<int>(vec_chars.size()));

    if (predicted_len != static_cast<unsigned long>(output_len)) 
    {
        ERRORLOG("Base64Encode error");
    }

  return output_buffer.get();
}

std::string Base64Decode(const std::string & dataSource)
{
  const auto predicted_len = 3 * dataSource.length() / 4;
  const auto output_buffer{std::make_unique<char[]>(predicted_len + 1)};
  const std::vector<unsigned char> vec_chars{dataSource.begin(), dataSource.end()};
  const auto output_len = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(output_buffer.get()), vec_chars.data(), static_cast<int>(vec_chars.size()));

  if (predicted_len != static_cast<unsigned long>(output_len)) 
  {
    ERRORLOG("Base64Decode error");
  }
  return output_buffer.get();
}

bool ed25519SignMessage(const std::string &message, EVP_PKEY* pkey, std::string &signature)
{
    EVP_MD_CTX *mdctx = NULL;
    const char * sig_name = message.c_str();

    unsigned char *sigValue = NULL;
    size_t sig_len = strlen(sig_name);

    // Create the Message Digest Context 
    if(!(mdctx = EVP_MD_CTX_new())) 
    {
        return false;
    }

    if(pkey == NULL)
    {
        return false;
    }
    
    // Initialise the DigestSign operation
    if(1 != EVP_DigestSignInit(mdctx, NULL, NULL, NULL, pkey)) 
    {
        return false;
    }

    size_t message_length = 0;
    if( 1 != EVP_DigestSign(mdctx, NULL, &message_length, (const unsigned char *)sig_name, sig_len))
    {
        return false;
    }

    sigValue = (unsigned char *)OPENSSL_malloc(message_length);

    if( 1 != EVP_DigestSign(mdctx, sigValue, &message_length, (const unsigned char *)sig_name, sig_len))
    {
        return false;
    }

    std::string hashString((char*)sigValue, message_length);
    signature = hashString;

    OPENSSL_free(sigValue);
    EVP_MD_CTX_free(mdctx);
    return true;

}

bool ed25519VerificationMessage(const std::string &message, EVP_PKEY* pkey, const std::string &signature)
{
    EVP_MD_CTX *mdctx = NULL;
    const char *msg = message.c_str();
    unsigned char *sig = (unsigned char *)signature.data();
    size_t slen = signature.size();
    size_t msgLen = strlen(msg);

    if(!(mdctx = EVP_MD_CTX_new())) 
    {
        return false;
    }

    /* Initialize `key` with a public key */
    if(1 != EVP_DigestVerifyInit(mdctx, NULL, NULL, NULL, pkey)) 
    {
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    if (1 != EVP_DigestVerify(mdctx, sig, slen ,(const unsigned char *)msg, msgLen)) 
    {
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    EVP_MD_CTX_free(mdctx);
    return true;

}

bool get_ed_pub_key_by_bytes(const std::string &pubStr, EVP_PKEY* &pKey)
{
    //Generate public key from binary string of public key  
    unsigned char* bufPtr = (unsigned char *)pubStr.data();
    const unsigned char *pk_str = bufPtr;
    int lenPtr = pubStr.size();
    
    if(lenPtr == 0)
    {
        ERRORLOG("public key Binary is empty");
        return false;
    }

    EVP_PKEY *peerPubKey = d2i_PUBKEY(NULL, &pk_str, lenPtr);

    if(peerPubKey == nullptr)
    {
        return false;
    }
    pKey = peerPubKey;
    return true;
}

void Sha256Hash(const uint8_t* input, size_t input_len, uint8_t* output) 
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_sha256();

    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestUpdate(ctx, input, input_len);
    EVP_DigestFinal_ex(ctx, output, NULL);
    EVP_MD_CTX_free(ctx);
}

EVP_PKEY *HexPrivateKeyToPkey(const std::string &hexPrivateKey)
{
    std::string primePrivateKeyData = Hex2Str(hexPrivateKey);
    const unsigned char *raw_priv = reinterpret_cast<const unsigned char *>(primePrivateKeyData.data());
    EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, raw_priv, 32); 
    if (!pkey)
    {
        ERRORLOG("EVP_PKEY_new_raw_private_key failed");
    }
    return pkey;
    
}

int ReadPrivateKey(const std::string &accountFileName,std::string &hexPrivateKey)
{

    // Open the hexadecimal private key file
    std::ifstream priFile(accountFileName,std::ios::binary);
    if (!priFile.is_open())
    {
        printf("Error: Failed to open file\n");
        return -1;
    }

    // Read the content of the first line
    if (!std::getline(priFile, hexPrivateKey))
    { 
        printf("Error: Empty file or read failure\n");
        priFile.close();
        return -2;
    }
    priFile.close();
    for (char c : hexPrivateKey)
    {
        if (!std::isxdigit(c))
        {
            printf("Invalid hex character: %c\n", c);
            return -4;
        }
    }

    const size_t expectedLen = PRIME_SEED_NUM * 2;
    if (hexPrivateKey.length() != expectedLen)
    {
        printf("Error: Invalid hex length (expected %zu, got %zu)\n",
               expectedLen, hexPrivateKey.length());
        return -3;
    }

    return 0;
}

int WritePrivateKey(const std::string &accountFileName, const std::string &privateKey)
{
    std::ofstream outFile(accountFileName, std::ios::out | std::ios::binary);
    if (!outFile.is_open())
    {              
        return -1; 
    }

    try
    {
        std::string hexPrivateKey = Str2Hex(privateKey);
        outFile << hexPrivateKey << std::endl; 

        if (outFile.fail())
        {
            outFile.close();
            return -2;
        }
        outFile.close();
    }
    catch (...)
    {
        if (outFile.is_open())
            outFile.close();
        return -3; 
    }

    return 0; 
}

int AccountManager::ChangeAccountPassword(const std::string& address, 
                                       const std::string& oldPassword, 
                                       const std::string& newPassword,
                                       std::string& errorMsg) {
    if(address.empty() || oldPassword.empty() || newPassword.empty()) {
        errorMsg = "Invalid parameters: address or password is empty";
        ERRORLOG("ChangeAccountPassword: Invalid parameters");
        return -1;
    }

    if(!IsExist(address)) {
        errorMsg = "Account not found for address: " + address;
        ERRORLOG("ChangeAccountPassword: Account not found for address: {}", address);
        return -2;
    }

    std::string keystoreFile = GetGlobalFileManager().GetKeystoreFilePath(address);
    std::ifstream inFile(keystoreFile);
    if(!inFile.is_open()) {
        errorMsg = "Keystore file not found for address: " + address;
        ERRORLOG("ChangeAccountPassword: Keystore file not found: {}", keystoreFile);
        return -3;
    }
    
    std::string keystoreJson((std::istreambuf_iterator<char>(inFile)),
                           std::istreambuf_iterator<char>());
    inFile.close();

    KeystoreManager keystoreMgr;
    std::string privateKey;
    
    int decryptResult = keystoreMgr.DecryptPrivateKey(keystoreJson, oldPassword, privateKey);
    if(decryptResult != 0) {
        errorMsg = "Incorrect password";
        ERRORLOG("ChangeAccountPassword: Incorrect password for address: {}", address);
        return -4;
    }

    if(SavePrivateKeyToKeystoreFile(address, privateKey, newPassword) != 0) {
        errorMsg = "Failed to save with new password";
        ERRORLOG("ChangeAccountPassword: Failed to save with new password for address: {}", address);
        return -5;
    }
    
    DEBUGLOG("ChangeAccountPassword: Password changed successfully for address: {}", address);
    return 0;
}

int AccountManager::load_account_from_key_store(const std::string& keystoreFilePath, const std::string& password) {
    std::ifstream keystoreFile(keystoreFilePath);
    if(!keystoreFile.is_open()) {
        ERRORLOG("Cannot open keystore file: {}", keystoreFilePath);
        return -1;
    }

    std::string keystoreJson((std::istreambuf_iterator<char>(keystoreFile)),
                             std::istreambuf_iterator<char>());
    keystoreFile.close();

    nlohmann::json jsonObj;
    try {
        jsonObj = nlohmann::json::parse(keystoreJson);
    } catch(const std::exception& e) {
        ERRORLOG("Failed to parse keystore JSON: {}", e.what());
        return -2;
    }
    
    if(!jsonObj.contains("address") || !jsonObj.contains("crypto")) {
        ERRORLOG("Invalid keystore format");
        return -3;
    }
    
    std::string address = jsonObj["address"];

    KeystoreManager keystoreMgr;
    std::string privateKey;
    
    int decryptResult = keystoreMgr.DecryptPrivateKey(keystoreJson, password, privateKey);
    if(decryptResult != 0) {
        if (decryptResult == -1) {
            ERRORLOG("Unsupported keystore version");
            try {
                int version = jsonObj["version"].get<int>();
                ERRORLOG("Keystore version {} is not supported. Expected version: 1", version);
                std::cout << "\nError: Keystore version " << version << " is not supported. Expected version: 1" << std::endl;
            } catch(...) {
                ERRORLOG("Failed to read keystore version");
                std::cout << "\nError: Invalid keystore version format" << std::endl;
            }
            return -4;
        } else if (decryptResult == -2 || decryptResult == -3) {
            ERRORLOG("Unsupported keystore format (kdf or cipher)");
            std::cout << "\nError: Unsupported keystore format (encryption method)" << std::endl;
            return -5;
        } else if (decryptResult == -4) {
            ERRORLOG("Failed to decrypt keystore with provided password (invalid password)");
            std::cout << "\nError: Incorrect password" << std::endl;
            return -6;
        } else {
            ERRORLOG("Failed to decrypt keystore, error code: {}", decryptResult);
            std::cout << "\nError: Failed to decrypt keystore (error code: " << decryptResult << ")" << std::endl;
            return -7;
        }
    }

    std::string hexPrivateKey = Str2Hex(privateKey);
    EVP_PKEY *pkey = HexPrivateKeyToPkey(hexPrivateKey);
    if(pkey == nullptr) {
        ERRORLOG("Invalid private key from keystore");
        return -8;
    }

    Account account;
    account.SetKey(pkey);

    unsigned char *pkeyDer = NULL;
    int publen = i2d_PUBKEY(pkey, &pkeyDer);
    std::string pubStr;
    for(int i = 0; i < publen; ++i) {
        pubStr += pkeyDer[i];
    }
    OPENSSL_free(pkeyDer);

    size_t len = 80;
    char pkeyData[80] = {0};
    if(EVP_PKEY_get_raw_private_key(pkey, (unsigned char *)pkeyData, &len) == 0) {
        ERRORLOG("Failed to get raw private key");
        return -9;
    }
    std::string priStr(reinterpret_cast<char*>(pkeyData), len);
    
    account.SetPubStr(pubStr);
    account.setPriorityString(priStr);
    account.SetAddr(address);

    if(AddAccount(account) != 0) {
        ERRORLOG("Failed to add account to account list");
        return -10;
    }
    
    DEBUGLOG("Successfully loaded account from keystore: {}", address);
    return 0;
}

int AccountManager::SavePrivateKeyToKeystoreFile(const std::string& address, const std::string& privateKey, const std::string& password) {

    if(address.empty() || privateKey.empty() || password.empty()) {
        ERRORLOG("SavePrivateKeyToKeystoreFile: Invalid parameters");
        return -1;
    }

    KeystoreManager keystoreMgr;
    std::string keystoreJson;

    int64_t processingTime = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    if(keystoreMgr.EncryptPrivateKey(privateKey, address, password, keystoreJson) != 0) {
        ERRORLOG("Failed to encrypt private key for address: {}", address);
        return -2;
    }

    std::string keystoreFile = GetGlobalFileManager().GetKeystoreFilePath(address);
    std::ofstream outFile(keystoreFile, std::ios::out | std::ios::binary);
    if(!outFile.is_open()) {
        ERRORLOG("Failed to create keystore file: {}", keystoreFile);
        return -3;
    }
    
    outFile << keystoreJson;
    if(outFile.fail()) {
        ERRORLOG("Failed to write keystore file: {}", keystoreFile);
        outFile.close();
        return -4;
    }
    
    outFile.close();
    auto end_time = std::chrono::high_resolution_clock::now();
    auto decryption_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    DEBUGLOG("Successfully saved keystore file for address: {}, and time: {}, ms", address, processingTime);
 
    return 0;
}
