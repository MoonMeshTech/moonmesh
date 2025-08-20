#include "../utils/keccak_cryopp.hpp"
#include <stdio.h>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/err.h>

std::string Keccak256CrypterOpenSSL(const std::string& input) 
{
    EVP_MD_CTX *mdctx = nullptr;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    std::string result;

    if ((mdctx = EVP_MD_CTX_new()) == nullptr) {
        return "";
    }

    EVP_MD *keccak256_md = EVP_MD_fetch(NULL, "KECCAK-256", NULL);
    if (keccak256_md == NULL) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    if (EVP_DigestInit_ex(mdctx, keccak256_md, nullptr) != 1) {
        EVP_MD_free(keccak256_md);
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    if (EVP_DigestUpdate(mdctx, input.c_str(), input.length()) != 1) {
        EVP_MD_free(keccak256_md);
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    if (EVP_DigestFinal_ex(mdctx, digest, &digest_len) != 1) {
        EVP_MD_free(keccak256_md);
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    EVP_MD_free(keccak256_md);
    EVP_MD_CTX_free(mdctx);

    std::stringstream ss;
    ss << std::hex << std::uppercase; 
    for (unsigned int i = 0; i < digest_len; i++) {
        ss << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }

    return ss.str();
}

