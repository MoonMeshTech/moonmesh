#ifndef _KEYSTORE_H_
#define _KEYSTORE_H_

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <nlohmann/json.hpp>
#include "hex_code.h"
#include "../include/logging.h"

struct KeystoreFile {
    int version;                    
    std::string id;                 
    std::string address;            
    
    struct CryptoInfo {
        std::string cipher;         
        std::string ciphertext;     
        
        struct CipherParams {
            std::string iv;         
        } cipherparams;
        
        std::string kdf;            
        
        struct KDFParams {
            int dklen;              
            int n;                  
            int r;                  
            int p;                  
            std::string salt;       
        } kdfparams;
        
        std::string mac;            
    } crypto;

    nlohmann::json ToJson() const;
    
    static KeystoreFile FromJson(const nlohmann::json& json);
};

class KeystoreManager {
public:
    int EncryptPrivateKey(const std::string& privateKey, const std::string& address, const std::string& password, std::string& keystoreJson);
    
    int DecryptPrivateKey(const std::string& keystoreJson, const std::string& password, std::string& privateKey);
    
private:

    std::string GenerateUUID();
    
    std::string GenerateRandomBytes(int length);
    
    std::string DeriveKeyFromPassword(const std::string& password, const std::string& salt, 
                                     int n, int r, int p, int dklen);

    std::string CalculateMAC(const std::string& derivedKey, const std::string& ciphertext);
    
    int AesCtrEncrypt(const std::string& data, const std::string& key, const std::string& iv, 
                     std::string& ciphertext);
    
    int AesCtrDecrypt(const std::string& ciphertext, const std::string& key, const std::string& iv, 
                     std::string& plaintext);

    void PrintOpenSSLError();
};

std::string GetPasswordFromUser(bool isConfirm = false, const std::string& prompt = "Enter password: ");

#endif // _KEYSTORE_H_