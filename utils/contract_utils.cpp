#include "contract_utils.h"
#include "account_manager.h"

#include <evmc/hex.hpp>
#include <evmone/evmone.h>
#include "db/db_api.h"
#include "../utils/keccak_cryopp.hpp"
evmc_address evm_utils::convertStringToEvmAddress(const std::string& addr)
{
    const char *s = addr.data();
    evmc_address evmAddr = evmc::from_hex<evmc::address>(s).value_or(evmc_address{});
    return evmAddr;
}

std::string evm_utils::evm_addr_to_string(const evmc_address& addr)
{
    return evm_utils::ToChecksumAddress(evmc::hex({addr.bytes,sizeof(addr.bytes)}));
}

evmc_address evm_utils::convertPubStrToEvmAddr(const std::string& pub)
{
    std::string evmAddress = GenerateAddr(pub);
    const char *s = evmAddress.data();
    evmc_address evmaddr = evmc::from_hex<evmc::address>(s).value_or(evmc_address{});;
    return evmaddr;
}

std::string evm_utils::ToChecksumAddress(const std::string& address) {
    std::string addressNoPrefix = address.substr(0, 2) == "0x" ? address.substr(2) : address;
    std::transform(addressNoPrefix.begin(), addressNoPrefix.end(), addressNoPrefix.begin(), ::tolower);

    std::string hash = Keccak256CrypterOpenSSL(addressNoPrefix);
    std::transform(hash.begin(), hash.end(), hash.begin(), ::tolower);

    for (size_t i = 0; i < addressNoPrefix.size(); ++i) {
        if (hash[i] >= '8' && addressNoPrefix[i] >= 'a' && addressNoPrefix[i] <= 'f') {
            addressNoPrefix[i] = toupper(addressNoPrefix[i]);
        }
    }

    return addressNoPrefix;
}

std::string evm_utils::GenerateContractAddr(const std::string& input)
{
    return GenerateAddr(input);
}

std::string evm_utils::GetEvmAddr(const std::string& pub)
{
    std::string evmAddress = GenerateAddr(pub);
    return "0x" + evmAddress;
}

evmc::bytes evm_utils::StringTobytes(const std::string& content)
{

    auto convertResult = evmc::from_hex(content);
    if(convertResult.has_value())
    {
        return convertResult.value();
    }
    else
    {
        ERRORLOG("fail to convert content to hex , content: {}", content);
        return {};
    }

}

std::string evm_utils::BytesToString(const bytes& content)
{
    return evmc::hex(content);
}

evmc_uint256be evm_utils::convertUint32ToEvmcUint256be(uint32_t x)
{
    evmc_uint256be value = {};
    value.bytes[31] = static_cast<uint8_t>(x);
    value.bytes[30] = static_cast<uint8_t>(x >> 8);
    value.bytes[29] = static_cast<uint8_t>(x >> 16);
    value.bytes[28] = static_cast<uint8_t>(x >> 24);
    return value;
}

uint32_t evm_utils::evmc_uint256be_to_uint32(evmc_uint256be value)
{
    return (uint32_t{value.bytes[28]} << 24) | (uint32_t{value.bytes[29]} << 16) |
           (uint32_t{value.bytes[30]} << 8) | (uint32_t{value.bytes[31]});
}

int evm_utils::GetContractCode(const std::string& contractAddress, bytes& code)
{
    DBReader dbReader;
    std::string strCode;
    if(dbReader.getContractCodeByContractAddr(contractAddress, strCode) != DB_SUCCESS)
    {
        ERRORLOG("can't get deploy hash of contract {}", contractAddress)
        return 1;
    }

    code = StringTobytes(strCode);
    if (code.empty())
    {
        ERRORLOG("fail to convert contract code to hex format");
        return -4;
    }

    return 0;
}