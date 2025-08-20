/**
 * *****************************************************************************
 * @file        account_manager.h
 * @brief       
 * @date        2023-09-28
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef EdManager
#define EdManager

#include <iostream>
#include <string>
#include <filesystem>
#include <dirent.h>

#include "hex_code.h"
#include "utils/time_util.h"
#include "magic_singleton.h"
#include "../ca/global.h"
#include "../include/logging.h"
#include <nlohmann/json.hpp>
#include "include/scope_guard.h"
#include "utils/file_manager.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/core_names.h>

#include "keystore.h"

class accountInitResult {
public:
    enum class Status {
        SUCCESS,
        EMPTY_KEYSTORE,
        kAccountsNotInitialized,
        CREATE_ACCOUNT_FAILED,
        USER_REQUESTED_EXIT,
        FATAL_ERROR
    };
    
    Status status;
    std::string message;
    int errorCode;
    
    static accountInitResult Success() {
        return {Status::SUCCESS, "Initialization completed successfully", 0};
    }
    
    static accountInitResult Error(Status status, const std::string& message, int errorCode = -1) {
        return {status, message, errorCode};
    }
};

/**
 * @brief  hexPrivateKey length is equal to seed length
 * 
 */
static const int PRIME_SEED_NUM = 32;
class Account
{
    public:
        Account(const bool isNeedInitialize) :_pkey(nullptr, &EVP_PKEY_free){
            generatePkey();
        }
        Account():_pkey(nullptr, &EVP_PKEY_free),_pubStr(), _priStr(), _Addr(){}

        /**
         * @brief       Construct a new Account object
         * 
         * @param       bs58Addr: 
         */
        Account(const std::string &addressFileName):_pkey(nullptr, &EVP_PKEY_free)
        {
            generateFilePkey(addressFileName);
        }

        /**
         * @brief       Construct a new Account object
         * 
         * @param       other: 
         */
        Account(const Account& other);

        /**
         * @brief       Construct a new Account object
         * 
         * @param       other: 
         */
        Account(Account&& other) noexcept;

        /**
         * @brief       
         * 
         * @param       other: 
         * @return      Account& 
         */
        Account& operator=(const Account& other);

        /**
         * @brief       
         * 
         * @param       other: 
         * @return      Account& 
         */
        Account& operator=(Account&& other) noexcept;

        /**
         * @brief       Destroy the Account object
         * 
         */
        ~Account(){ _pkey.reset(); };

        /**
         * @brief       
         * 
         * @param       message: 
         * @param       signature: 
         * @return      true 
         * @return      false 
         */
        bool Sign(const std::string &message, std::string &signature);

        /**
         * @brief       
         * 
         * @param       message: 
         * @param       signature: 
         * @return      true 
         * @return      false 
         */
        bool Verify(const std::string &message, std::string &signature);
        
        /**
         * @brief       Get the Key object
         * 
         * @return      EVP_PKEY* 
         */
        EVP_PKEY * GetKey () const
        {
            return _pkey.get();
        }

        /**
         * @brief       Set the Key object
         * 
         * @param       key: 
         */
        void SetKey(EVP_PKEY * key)
        {
            _pkey.reset(key);
        }
        
        /**
         * @brief       Get the Pub Str object
         * 
         * @return      std::string 
         */
        std::string GetPubStr() const
        {
            return _pubStr;
        }
        
        /**
         * @brief       Set the Pub Str object
         * 
         * @param       str: 
         */
        void SetPubStr(std::string &str)
        {
            _pubStr = str;
        }

        /**
         * @brief       Get the Pri Str object
         * 
         * @return      std::string 
         */
        std::string GetPriStr() const
        {
            return _priStr;
        }

        /**
         * @brief       Set the Pri Str object
         * 
         * @param       str: 
         */
        void setPriorityString(std::string &str)
        {
            _priStr = str;
        }

        /**
         * @brief       Get the Base 5 8 object
         * 
         * @return      std::string 
         */
        std::string GetAddr() const
        {
            return _Addr;
        }

        /**
         * @brief       Set the Base 5 8 object
         * 
         * @param       addr: 
         */
        void SetAddr(std::string & addr)
        {
            _Addr = addr;
        }
    private:
        /**
         * @brief  Generate a public key through a pointer
         * 
         * 
         */
        void generatePublicString(EVP_PKEY* pkeyPtr);

        /**
         * @brief       generatePrivateString
         * 
         */
        void generatePrivateString(EVP_PKEY* pkeyPtr);

        /**
         * @brief      _GenerateAddr 
         * 
         */
        void _GenerateAddr();
        
        /**
         * @brief     generatePkey  
         * 
         */
        void generatePkey();

        /**
         * @brief       generateFilePkey
         * 
         * @param       address 
         */
        void generateFilePkey(const std::string &address);

        /**
         * @brief       _GenerateRandomSeed
         * 
         */
        void _GenerateRandomSeed(uint8_t _seed[PRIME_SEED_NUM]);

    private:
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> _pkey;
        std::string _pubStr;
        std::string _priStr;
        std::string _Addr;
};

/**
 * @brief       
 * 
 */
class AccountManager
{
    public:
        AccountManager() {}
        ~AccountManager() = default;

        /**
         * @brief       
         * 
         * @param       account: 
         * @return      int 
         */
        int AddAccount(Account &account);

        /**
         * @brief       
         * 
         */
        void PrintAllAccount() const;

        /**
         * @brief       delete account
         * 
         * @param       addr: address
         * @return      int: 0 success
         */
        int DeleteAccount(const std::string& addr);

        /**
         * @brief       Set the Default Base 5 8 Addr object
         * 
         * @param       address: 
         */
        void SetDefaultAddr(const std::string & address);

        /**
         * @brief       Get the Default Base 5 8 Addr object
         * 
         * @return      std::string 
         */
        std::string GetDefaultAddr() const;

        /**
         * @brief       Set the Default Account object
         * 
         * @param       address: 
         * @return      int 
         */
        int SetDefaultAccount(const std::string & address);

        /**
         * @brief       
         * 
         * @param       address: 
         * @return      true 
         * @return      false 
         */
        bool IsExist(const std::string & address);

        /**
         * @brief       
         * 
         * @param       address: 
         * @param       account: 
         * @return      int 
         */
        int FindAccount(const std::string & address, Account & account);

        /**
         * @brief       Get the Default Account object
         * 
         * @param       account: 
         * @return      int 
         */
        int GetDefaultAccount(Account & account);

        /**
         * @brief       Get the Account List object
         * 
         * @param       _list: 
         */
        void GetAccountList(std::vector<std::string> & _list);

        /**
         * @brief       The public key is obtained by public key binary
         * 
         * @param       pubStr: 
         * @param       account: 
         * @return      true 
         * @return      false 
         */
        bool getAccountPublicKeyByBytes(const std::string &pubStr, Account &account);
        
        /**
        * @brief
        *
        * @param privateKeyHex: Private key in hexadecimal format
        * @param address: Output parameter, generated account address
        * @return int: 0 if successful, otherwise failed
        */
        int ImportHexPrivateKey(const std::string & hexPrivateKey, std::string &address);

        /**
        * @brief Save the key to the keystore file using a password
        *
        * @param address: Account address
        * @param privateKey: Private key details
        * @param password: User password
        * @return int: 0 if successful, otherwise failed
        */
        int SavePrivateKeyToKeystoreFile(const std::string& address, const std::string& privateKey, const std::string& password);

        /**
        * @brief Loads an account from a keystore file
        *
        * @param keystoreFilePath: keystore file path
        * @param password: user password
        * @return int: 0 if successful, otherwise failed
        */
        int load_account_from_key_store(const std::string& keystoreFilePath, const std::string& password);

        /**
        * @brief Imports a private key and stores it encrypted with a password
        *
        * @param hexPrivateKey: Private key in hexadecimal format
        * @param password: User password
        * @return int: 0 if successful, otherwise failed
        */
        int ImportEncryptedPrivateKey(const std::string& hexPrivateKey, const std::string& password);

        /**
        * @brief Change account password
        *
        * @param address: The account address to change the password
        * @param oldPassword: The old password
        * @param newPassword: The new password
        * @param errorMsg: The error message output parameter
        * @return int: 0 if successful, otherwise (error code)
        */
        int ChangeAccountPassword(const std::string& address, 
                                const std::string& oldPassword, 
                                const std::string& newPassword,
                                std::string& errorMsg);

        /**
        * @brief Helper method for batch unlocking accounts using a single password
        *
        * @param keystoreFiles List of keystore files to unlock
        * @param totalAccounts Total number of accounts to process
        * @param maxThreads Maximum number of threads to use
        * @param successCount Number of accounts successfully unlocked
        * @param failedAccounts List of accounts that failed to unlock
        */
        void UnlockAccountsBatchMode(
            const std::vector<std::string>& keystoreFiles,
            int totalAccounts,
            int maxThreads,
            int& successCount,
            std::vector<std::string>& failedAccounts);

        accountInitResult Initialize();
        accountInitResult CreateInitialAccount();

    private:
        std::string _defaultAddress;
        std::map<std::string /*addr*/, std::shared_ptr<Account>> _accountList;
        
        accountInitResult _scanKeystoreDirectory(std::vector<std::string>& keystoreFiles);
        accountInitResult loadExistingAccounts(const std::vector<std::string>& keystoreFiles);
        accountInitResult _createInitialAccountImpl();
};


bool isValidAddress(const std::string& address);

/**
 * @brief       
 * 
 * @param       publicKey: 
 * @return      std::string 
 */
std::string GenerateAddr(const std::string& publicKey);

/**
 * @brief       
 * 
 * @param       dataSource: 
 * @return      std::string 
 */
std::string Base64Encode(const std::string & dataSource);

/**
 * @brief       
 * 
 * @param       dataSource: 
 * @return      std::string 
 */
std::string Base64Decode(const std::string & dataSource);

/**
 * @brief       
 * 
 * @param       text: 
 * @return      std::string 
 */
std::string Getsha256hash(const std::string & text);

/**
 * @brief       
 * 
 * @param       message: 
 * @param       pkey: 
 * @param       signature: 
 * @return      true 
 * @return      false 
 */
bool ed25519SignMessage(const std::string &message, EVP_PKEY* pkey, std::string &signature);

/**
 * @brief       
 * 
 * @param       message: 
 * @param       pkey: 
 * @param       signature: 
 * @return      true 
 * @return      false 
 */
bool ed25519VerificationMessage(const std::string &message, EVP_PKEY* pkey, const std::string &signature);

/**
 * @brief       
 * 
 * @param       pubStr: 
 * @param       pKey: 
 * @return      true 
 * @return      false 
 */
bool get_ed_pub_key_by_bytes(const std::string &pubStr, EVP_PKEY* &pKey);

/**
 * @brief       generater random seed
 * 
 * @return      std::vector<unsigned char> 
 */

void Sha256Hash(const uint8_t* input, size_t input_len, uint8_t* output);

EVP_PKEY *HexPrivateKeyToPkey(const std::string &hexPrivateKey);

int ReadPrivateKey(const std::string &accountFileName, std::string &hexPrivateKey);

int WritePrivateKey(const std::string &accountFileName, const std::string &privateKey);

void DisplayProgressBar(int current, int total, int barWidth);

#endif
