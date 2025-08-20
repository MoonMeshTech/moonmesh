#include "keystore.h"
#include <iomanip>
#include <sstream>
#include <openssl/crypto.h>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <chrono>

nlohmann::json KeystoreFile::ToJson() const {
    nlohmann::json j;
    j["version"] = version;
    j["id"] = id;
    j["address"] = address;
    
    nlohmann::json cryptoJson;
    cryptoJson["cipher"] = crypto.cipher;
    cryptoJson["ciphertext"] = crypto.ciphertext;
    
    nlohmann::json cipherparamsJson;
    cipherparamsJson["iv"] = crypto.cipherparams.iv;
    cryptoJson["cipherparams"] = cipherparamsJson;
    
    cryptoJson["kdf"] = crypto.kdf;
    
    nlohmann::json kdfparamsJson;
    kdfparamsJson["dklen"] = crypto.kdfparams.dklen;
    kdfparamsJson["n"] = crypto.kdfparams.n;
    kdfparamsJson["r"] = crypto.kdfparams.r;
    kdfparamsJson["p"] = crypto.kdfparams.p;
    kdfparamsJson["salt"] = crypto.kdfparams.salt;
    cryptoJson["kdfparams"] = kdfparamsJson;
    
    cryptoJson["mac"] = crypto.mac;
    j["crypto"] = cryptoJson;
    
    return j;
}

KeystoreFile KeystoreFile::FromJson(const nlohmann::json& j) {
    KeystoreFile keystoreFile;
    
    keystoreFile.version = j["version"].get<int>();
    keystoreFile.id = j["id"];
    keystoreFile.address = j["address"];
    
    auto cryptoJson = j["crypto"];
    keystoreFile.crypto.cipher = cryptoJson["cipher"];
    keystoreFile.crypto.ciphertext = cryptoJson["ciphertext"];
    
    auto cipherparamsJson = cryptoJson["cipherparams"];
    keystoreFile.crypto.cipherparams.iv = cipherparamsJson["iv"];
    
    keystoreFile.crypto.kdf = cryptoJson["kdf"];
    
    auto kdfparamsJson = cryptoJson["kdfparams"];
    keystoreFile.crypto.kdfparams.dklen = kdfparamsJson["dklen"];
    keystoreFile.crypto.kdfparams.n = kdfparamsJson["n"];
    keystoreFile.crypto.kdfparams.r = kdfparamsJson["r"];
    keystoreFile.crypto.kdfparams.p = kdfparamsJson["p"];
    keystoreFile.crypto.kdfparams.salt = kdfparamsJson["salt"];
    
    keystoreFile.crypto.mac = cryptoJson["mac"];
    
    return keystoreFile;
}

std::string KeystoreManager::GenerateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    }
    return ss.str();
}

std::string KeystoreManager::GenerateRandomBytes(int length) {
    std::vector<unsigned char> buffer(length);
    if (RAND_bytes(buffer.data(), length) != 1) {
        PrintOpenSSLError();
        throw std::runtime_error("Failed to generate random bytes");
    }
    return std::string(reinterpret_cast<char*>(buffer.data()), length);
}

std::string KeystoreManager::DeriveKeyFromPassword(const std::string& password, const std::string& salt, 
                                                 int n, int r, int p, int dklen) {
    std::vector<unsigned char> key(dklen);
    
    
    uint64_t maxmem = 1024 * 1024 * 1024;
    
    if (EVP_PBE_scrypt(password.c_str(), password.length(),
                      reinterpret_cast<const unsigned char*>(salt.c_str()), salt.length(),
                      n, r, p, maxmem, key.data(), dklen) != 1) {
        PrintOpenSSLError();
        throw std::runtime_error("Failed to derive key from password using scrypt");
    }
    
    return std::string(reinterpret_cast<char*>(key.data()), dklen);
}

std::string KeystoreManager::CalculateMAC(const std::string& derivedKey, const std::string& ciphertext) {
    std::string dataToHash = derivedKey.substr(16, 16) + ciphertext;
    
    extern std::string Keccak256CrypterOpenSSL(const std::string& input);
    std::string mac = Keccak256CrypterOpenSSL(dataToHash);
    
    return mac;
}

int KeystoreManager::AesCtrEncrypt(const std::string& data, const std::string& key, 
                                const std::string& iv, std::string& ciphertext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        PrintOpenSSLError();
        return -1;
    }
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), NULL, 
                        reinterpret_cast<const unsigned char*>(key.c_str()), 
                        reinterpret_cast<const unsigned char*>(iv.c_str())) != 1) {
        PrintOpenSSLError();
        EVP_CIPHER_CTX_free(ctx);
        return -2;
    }
    
    std::vector<unsigned char> ciphertextBuffer(data.length() + EVP_MAX_BLOCK_LENGTH);
    int len = 0, ciphertext_len = 0;
    
    if (EVP_EncryptUpdate(ctx, ciphertextBuffer.data(), &len, 
                        reinterpret_cast<const unsigned char*>(data.c_str()), data.length()) != 1) {
        PrintOpenSSLError();
        EVP_CIPHER_CTX_free(ctx);
        return -3;
    }
    ciphertext_len = len;
    
    if (EVP_EncryptFinal_ex(ctx, ciphertextBuffer.data() + len, &len) != 1) {
        PrintOpenSSLError();
        EVP_CIPHER_CTX_free(ctx);
        return -4;
    }
    ciphertext_len += len;
    
    ciphertext = std::string(reinterpret_cast<char*>(ciphertextBuffer.data()), ciphertext_len);
    
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}

int KeystoreManager::AesCtrDecrypt(const std::string& ciphertext, const std::string& key, 
                                const std::string& iv, std::string& plaintext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        PrintOpenSSLError();
        return -1;
    }
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_ctr(), NULL, 
                        reinterpret_cast<const unsigned char*>(key.c_str()), 
                        reinterpret_cast<const unsigned char*>(iv.c_str())) != 1) {
        PrintOpenSSLError();
        EVP_CIPHER_CTX_free(ctx);
        return -2;
    }
    
    std::vector<unsigned char> plaintextBuffer(ciphertext.length() + EVP_MAX_BLOCK_LENGTH);
    int len = 0, plaintext_len = 0;
    
    if (EVP_DecryptUpdate(ctx, plaintextBuffer.data(), &len, 
                        reinterpret_cast<const unsigned char*>(ciphertext.c_str()), ciphertext.length()) != 1) {
        PrintOpenSSLError();
        EVP_CIPHER_CTX_free(ctx);
        return -3;
    }
    plaintext_len = len;
    
    if (EVP_DecryptFinal_ex(ctx, plaintextBuffer.data() + len, &len) != 1) {
        PrintOpenSSLError();
        EVP_CIPHER_CTX_free(ctx);
        return -4;
    }
    plaintext_len += len;
    
    plaintext = std::string(reinterpret_cast<char*>(plaintextBuffer.data()), plaintext_len);
    
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}

int KeystoreManager::EncryptPrivateKey(const std::string& privateKey, const std::string& address,
                                    const std::string& password, std::string& keystoreJson) {
    auto start_time = std::chrono::high_resolution_clock::now();
    auto kdf_start_time = start_time;
    auto encrypt_start_time = start_time;
    auto mac_start_time = start_time;
    auto json_start_time = start_time;
    
    int64_t kdf_time_ms = 0;
    int64_t encrypt_time_ms = 0;
    int64_t mac_time_ms = 0;
    int64_t json_time_ms = 0;
    try {
        KeystoreFile keystoreFile;

        keystoreFile.version = 1;
        keystoreFile.id = GenerateUUID();
        keystoreFile.address = address;

        keystoreFile.crypto.cipher = "aes-128-ctr";

        std::string salt = GenerateRandomBytes(32);
        keystoreFile.crypto.kdfparams.salt = Str2Hex(salt);

        keystoreFile.crypto.kdf = "scrypt";
        keystoreFile.crypto.kdfparams.dklen = 32;

        keystoreFile.crypto.kdfparams.n = 262144;  
        keystoreFile.crypto.kdfparams.r = 8;      
        keystoreFile.crypto.kdfparams.p = 1;      

        kdf_start_time = std::chrono::high_resolution_clock::now();
        std::string derivedKey = DeriveKeyFromPassword(password, salt, 
                                                     keystoreFile.crypto.kdfparams.n,
                                                     keystoreFile.crypto.kdfparams.r,
                                                     keystoreFile.crypto.kdfparams.p,
                                                     keystoreFile.crypto.kdfparams.dklen);
        
        auto kdf_end_time = std::chrono::high_resolution_clock::now();
        kdf_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(kdf_end_time - kdf_start_time).count();
        std::string encryptKey = derivedKey.substr(0, 16);

        std::string iv = GenerateRandomBytes(16);
        keystoreFile.crypto.cipherparams.iv = Str2Hex(iv);

        encrypt_start_time = std::chrono::high_resolution_clock::now();

        std::string ciphertext;
        if (AesCtrEncrypt(privateKey, encryptKey, iv, ciphertext) != 0) {
            return -1;
        }
        keystoreFile.crypto.ciphertext = Str2Hex(ciphertext);

        auto encrypt_end_time = std::chrono::high_resolution_clock::now();
        encrypt_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(encrypt_end_time - encrypt_start_time).count();

        mac_start_time = std::chrono::high_resolution_clock::now();

        keystoreFile.crypto.mac = CalculateMAC(derivedKey, ciphertext);

        auto mac_end_time = std::chrono::high_resolution_clock::now();
        mac_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(mac_end_time - mac_start_time).count();

        nlohmann::json jsonObj = keystoreFile.ToJson();
        keystoreJson = jsonObj.dump(4);

        auto json_end_time = std::chrono::high_resolution_clock::now();
        json_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(json_end_time - json_start_time).count();

        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        DEBUGLOG("Keystore generation success timing (ms): Total={}, KDF={}, Encrypt={}, MAC={}, JSON={}",
                total_time, kdf_time_ms, encrypt_time_ms, mac_time_ms, json_time_ms);
        return 0;
    } catch(const std::exception& e) {
        ERRORLOG("Failed to encrypt private key: {}", e.what());

        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        DEBUGLOG("Keystore generation failed timing (ms): Total={}, KDF={}, Encrypt={}, MAC={}, JSON={}",
                total_time, kdf_time_ms, encrypt_time_ms, mac_time_ms, json_time_ms);
        
        return -2;
    }
}

int KeystoreManager::DecryptPrivateKey(const std::string& keystoreJson, const std::string& password, 
                                    std::string& privateKey) {
    try {
        nlohmann::json jsonObj = nlohmann::json::parse(keystoreJson);
        KeystoreFile keystoreFile = KeystoreFile::FromJson(jsonObj);
        
        if (keystoreFile.version != 1) {
            ERRORLOG("Unsupported keystore version: {}", keystoreFile.version);
            return -1;
        }

        if (keystoreFile.crypto.kdf != "scrypt") {
            ERRORLOG("Unsupported key derivation function: {}", keystoreFile.crypto.kdf);
            return -2;
        }

        if (keystoreFile.crypto.cipher != "aes-128-ctr") {
            ERRORLOG("Unsupported cipher: {}", keystoreFile.crypto.cipher);
            return -3;
        }
        
        std::string salt = Hex2Str(keystoreFile.crypto.kdfparams.salt);
        std::string ciphertext = Hex2Str(keystoreFile.crypto.ciphertext);
        std::string iv = Hex2Str(keystoreFile.crypto.cipherparams.iv);
        
        std::string derivedKey = DeriveKeyFromPassword(password, salt, 
                                                     keystoreFile.crypto.kdfparams.n,
                                                     keystoreFile.crypto.kdfparams.r,
                                                     keystoreFile.crypto.kdfparams.p,
                                                     keystoreFile.crypto.kdfparams.dklen);
        
        std::string mac = CalculateMAC(derivedKey, ciphertext);
        if (mac != keystoreFile.crypto.mac) {
            ERRORLOG("Invalid password or corrupted keystore file");
            return -4;
        }

        std::string encryptKey = derivedKey.substr(0, 16);
        if (AesCtrDecrypt(ciphertext, encryptKey, iv, privateKey) != 0) {
            return -5;
        }
        
        return 0;
    } catch(const std::exception& e) {
        ERRORLOG("Failed to decrypt private key: {}", e.what());
        return -6;
    }
}

void KeystoreManager::PrintOpenSSLError() {
    unsigned long err = ERR_get_error();
    char err_buf[256];
    ERR_error_string_n(err, err_buf, sizeof(err_buf));
    ERRORLOG("OpenSSL error: {}", err_buf);
}

static struct termios g_oldTerminalSettings;
static bool g_terminalSettingsChanged = false;

void RestoreTerminalSettings() {
    if (g_terminalSettingsChanged) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_oldTerminalSettings);
        g_terminalSettingsChanged = false;
    }
}

/**
* Function for securely retrieving a password from the console
* Does not rely on std::cin, but reads directly from the terminal
* @param isConfirm indicates whether password confirmation is required
* @param prompt indicates a custom prompt message
*/
std::string GetPasswordFromUser(bool isConfirm, const std::string& prompt) {
    struct termios newTermios;
    std::string password;
    
    tcgetattr(STDIN_FILENO, &g_oldTerminalSettings);
    newTermios = g_oldTerminalSettings;
    g_terminalSettingsChanged = true; 

    newTermios.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);

    std::cout << prompt << std::flush;

    char c;
    while (true) {
        if (read(STDIN_FILENO, &c, 1) != 1) {
            break;
        }

        if (c == '\n' || c == '\r') {
            std::cout << std::endl;
            break;
        }

        if (c == 127 || c == 8) {  // Backspace or Delete
            if (!password.empty()) {
                password.pop_back();
            }
            continue;
        }

        password.push_back(c);
    }

    while (password.empty()) {
        std::cout << "Password cannot be empty. " << prompt << std::flush;

        while (true) {
            if (read(STDIN_FILENO, &c, 1) != 1) {
                break;
            }
            
            if (c == '\n' || c == '\r') {
                std::cout << std::endl;
                break;
            }
            
            if (c == 127 || c == 8) {
                if (!password.empty()) {
                    password.pop_back();
                }
                continue;
            }
            
            password.push_back(c);
        }
    }

    if (isConfirm) {
        std::string confirmPassword;
        std::cout << "Confirm password: " << std::flush;

        while (true) {
            if (read(STDIN_FILENO, &c, 1) != 1) {
                break;
            }
            
            if (c == '\n' || c == '\r') {
                std::cout << std::endl;
                break;
            }
            
            if (c == 127 || c == 8) {
                if (!confirmPassword.empty()) {
                    confirmPassword.pop_back();
                }
                continue;
            }
            
            confirmPassword.push_back(c);
        }

        if (password != confirmPassword) {
            std::cout << "Passwords do not match. Please try again." << std::endl;

            RestoreTerminalSettings();
            return GetPasswordFromUser(isConfirm, prompt);
        }
    }

    RestoreTerminalSettings();
    
    return password;
}