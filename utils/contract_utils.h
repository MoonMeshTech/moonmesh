/**
 * *****************************************************************************
 * @file        contract_utils.h
 * @brief       
 * @date        2023-09-28
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef CONTRACT_UTILS_HEADER_GUARD
#define CONTRACT_UTILS_HEADER_GUARD

#include <evmc/evmc.hpp>
#include <future>
#include <chrono>
#include <ostream>
#include <evmc/hex.hpp>
#include <evmone/evmone.h>


namespace evm_utils
{
    using namespace evmc;
    
    /**
     * @brief       
     * 
     * @param       address: 
     * @return      std::string 
     */
    std::string ToChecksumAddress(const std::string& address);

    /**
     * @brief       
     * 
     * @param       addr: 
     * @return      evmc_address 
     */
    evmc_address convertStringToEvmAddress(const std::string& addr);

    /**
     * @brief       
     * 
     * @param       addr: 
     * @return      std::string 
     */
    std::string evm_addr_to_string(const evmc_address& addr);

    /**
     * @brief       
     * 
     * @param       pub: 
     * @return      evmc_address 
     */
    evmc_address convertPubStrToEvmAddr(const std::string& pub);

    /**
     * @brief       
     * 
     * @param       input: 
     * @return      std::string 
     */
    std::string GenerateContractAddr(const std::string& input);

    /**
     * @brief       Get the Evm Addr object
     * 
     * @param       pub: 
     * @return      std::string 
     */
    std::string GetEvmAddr(const std::string& pub);

    evmc_uint256be convertUint32ToEvmcUint256be(uint32_t x);

    uint32_t evmc_uint256be_to_uint32(evmc_uint256be value);

    bytes StringTobytes(const std::string& content);

    std::string BytesToString(const bytes& content);

    int GetContractCode(const std::string& contractAddress, bytes& code);
}


#endif