#pragma once


#define CHECK_ERRORS \
    UERROR(CHECK_ERROR_BASE=0,"CHECK_ERROR_BASE")\
    UERROR(CHECK_ERROR_NOT_VALID_ADDR,"Not a valid address")\
    UERROR(CHECK_ERROR_THE_PUMPING_RATIO_IS_WRONG,"The pumping ratio is wrong")\
    UERROR(CHECK_ERROR_GET_TIME_FAIL,"get time fail")\
    UERROR(CHECK_ERROR_ADDR_ASSET_TYPE_CHECK_FAIL,"addr asset type check fail")

#define UTXO_ERRORS                                                                                                        \
    UERROR(BASE = 2000, "UTXO_ERROR_BASE")                                                                             \
    UERROR(UNKOWN_BAD_ERROR, " bad error")                                                                             \
    UERROR(ASSET_NOT_EXIST, "Asset type does not exist")                                                               \
    UERROR(GET_ASSET_TYPE_INFO_ASSET_TYPE_ERROR, "Get asset type info asset type error")                               \
    UERROR(GET_ASSET_TYPE_INFO_PARSE_ERROR, "Get asset type info parse error")                                         \
    UERROR(GET_ASSET_TYPE_INFO_GET_REVOKE_TX_HASH_ERROR, "Get asset type info get revoke tx hash by asset type error") \
    UERROR(GET_ASSET_TYPE_INFO_GET_REVOKE_PROPOSAL_INFO_ERROR, "Get revoke proposal info by asset type error")         \
    UERROR(GET_ADDR_TYPE_INFO_GET_ADDRESS_STATUS_ERROR, "Get address type info by get address status error")           \
    UERROR(GET_ALL_ASSET_TYPE_GET_ASSET_TYPE_ERROR, "Get all asset type by get asset types error")                     \
    UERROR(GET_BALANCE_GET_ASSET_TYPE_ERROR, "Get balance by get asset types error")                                   \
    UERROR(GET_BALANCE_FIND_NO_ASSET_TYPE_ERROR, "Get balance by find current type error")                             \
    UERROR(GET_BALANCE_FIND_ADDR_ERROR, "Get balance by addr error")                                         \
    UERROR(GET_BLOCK_BY_HASH_GET_BLOCK_HASH_ERROR, "Get block by hash,get block by block hash error")                 \
    UERROR(GET_BLOCK_BY_HEIGHT_HEIGHT_ERROR, "Get block by hash ,height error")                                        \
    UERROR(GET_BLOCK_BY_HEIGHT_TOP_ERROR, "Get block by hash ,top error")                                              \
    UERROR(GET_BLOCK_BY_HEIGHT_GET_BLOCK_HASHES_ERROR, "Get block hashs by block height error")                        \
    UERROR(GET_BLOCK_HASH_BY_HEIGHT_GET_BLOCK_HASHES_ERROR, "Get block hashs by block height error")                   \
    UERROR(GET_BLOCK_BY_BLOCK_HASH_GET_BLOCK_ERROR, "Get block hashs by getting block error")                          \
    UERROR(GET_BLOCK_BY_BLOCK_HASH_PARSE_ERROR, "Get block hashs by getting block hash error")                         \
    UERROR(GET_BLOCK_BY_TRANSACTION_HASH_GET_BLOCK_HASH_ERROR, "Get block hashs by getting block hash error")          \
    UERROR(GET_BLOCK_BY_TRANSACTION_HASH_GET_BLOCK_ERROR, "Get block hashs by getting block error")                    \
    UERROR(GET_BLOCK_BY_TRANSACTION_HASH_PARSE_ERROR, "Get block hashs by getting block error")                        \
    UERROR(GET_BLOCK_BY_HEIGHT_GET_TOP_ERROR, "Get block hashs by getting top error")                                  \
    UERROR(GET_BONUS_INFO_GET_ALL_ADDR_SIGN_COUNT_BY_PERIOD_ERROR, "Get block hashs by getting top error")             \
    UERROR(GET_CHAIN_ID_GET_BLOCK_HASH_ERROR, "Get block hashs by getting block hashs error")                          \
    UERROR(GET_CHAIN_ID_GET_BLOCK_HASH_LENGTH_ERROR, "Get block hash length error")                                    \
    UERROR(GET_TRANSACTION_BY_HASH_GET_TRANSACTION_ERROR, "Get transaction error")                                     \
    UERROR(GET_VOTE_ADDRS_GET_APPROVE_ADDRS_ERROR, "Get approve addresses error")                                      \
    UERROR(GET_VOTE_ADDRS_GET_AGAINST_ADDRS_ERROR, "Get against addresses error")                                      \
    UERROR(GET_YIELD_INFO_GET_ANNUALIZED_RATE_ERROR, "Get annualized rate error")                                      \
    UERROR(GET_VOTE_TX_HASH_GET_VOTE_TX_HASH_ERROR, "Get vote tx hash error")                                      \
    UERROR(END, "utxo_end")

#define RPC_ERRORS \
    UERROR(RPC_ERROR_BASE=3000,"RPC_ERROR_BASE")\
    UERROR(RPC_ERROR_NOT_FOUND_ACOUNT,"not found acount")\
