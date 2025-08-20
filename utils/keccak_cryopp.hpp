#ifndef __KECCAK256CRY_H__
#define __KECCAK256CRY_H__
#include <stdio.h>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/err.h>
std::string Keccak256CrypterOpenSSL(const std::string& input);
#endif