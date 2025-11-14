#pragma once


#define CHECK_ERRORS \
    UERROR(CHECK_ERROR_BASE=0,"CHECK_ERROR_BASE")\
    UERROR(CHECK_ERROR_NOT_VALID_ADDR,"Not a valid address")\
    UERROR(CHECK_ERROR_THE_PUMPING_RATIO_IS_WRONG,"The pumping ratio is wrong")\
    UERROR(CHECK_ERROR_GET_TIME_FAIL,"get time fail")\
    UERROR(CHECK_ERROR_ADDR_ASSET_TYPE_CHECK_FAIL,"addr asset type check fail")


#define UTXO_ERRORS                                                   \
  UERROR(UE_BASE = 2000, "UTXO_ERROR_BASE")                            \
  UERROR(UE_UNKOWN_BAD_ERROR, " bad error")                            \
  UERROR(UE_ASSET_NOT_EXIST, "Asset type does not exist")              \
  UERROR(UE_INSUFFICIENT_ASSET_TYPE, "Insufficient asset types")       \
  UERROR(UE_NOT_FIND_UTXO_BY_HASH, "not found utxo by hash")           \
  UERROR(UE_BALANCE_IS_INSUFFICIENT, "The balance is insufficient")    \
  UERROR(UE_ACOUNT_HAS_STAKED, "acount has staked")                    \
  UERROR(UE_THE_STAKE_AMOUNT_IS_INSUFFICIENT,"The stake amount is insufficient")                                   \
  UERROR(UE_SIGN_VIN_FAIL, "sign vin fail")                            \
  UERROR(UE_SING_UTXO_FAIL, "sign utxo fail")                          \
  UERROR(UE_GAS_IS_ZERO, "gas is zero")                                \
  UERROR(UE_FAILED_TO_PICK_A_PACKAGER, "Failed to pick a packager")    \
  UERROR(UE_FAILED_TO_GET_HEIGHT, "Failed to get height")              \
  UERROR(UE_FAILED_TO_GET_UTXO_HEIGHT, "faild get utxo height")        \
  UERROR(UE_FIND_UTXO_FAIL, "Find utxo fail")                          \
  UERROR(UE_NOT_A_VALID_CONTRACT, "Not a valid contract")              \
  UERROR(UE_GET_TOP_FAIL, "get top fail")                              \
  UERROR(UE_PRE_RANDAO, "pre randao fail ")                            \
  UERROR(UE_CREATE_CONTRACT_ADDR_FAIL, "create contract addr fail")    \
  UERROR(UE_GET_CHAINID_FAIL, "get chainid fail")                      \
  UERROR(UE_CONTRACT_NOCE_FAIL, "contract noce fail")                  \
  UERROR(UE_CREATE_EVMONE_FAIL, "create evmone fail")                  \
  UERROR(UE_EVM_TIMEOUT, "evm timeout")                                \
  UERROR(UE_EVMONE_EXEC_FAIL, "evmone exec fail")                      \
  UERROR(UE_GEN_VIN_FAIL, "gen vin fail")                              \
  UERROR(UE_GAS_ASSET_TYPE_NOT_MATCH, "gas asset type not match")      \
  UERROR(UE_CHECK_ADDR_ASSTYPE_HEIGHT_FAIL, "check addr asset type height fail")                                  \
  UERROR(UE_TO_ADDR_EMPTY, "to addr empty")                            \
  UERROR(UE_UNVALID_ADDR, "unvalid addr")                              \
  UERROR(UE_NOT_QUALIFIED_FOR_UNDELEGATION, "FromAddr is not qualified to Undelegating!")                         \
  UERROR(UE_DELEGATING_AMOUNT_EXCEED_THE_LIMIT,"Delegating amount exceeds the limit!")                               \
  UERROR(UE_UNKOWN_DELEGATE_TYPE, "Unknown delegate type!")            \
  UERROR(UE_NOT_QUALIFIED_FOR_BONUS, "Not allowed to Bonus!")          \
  UERROR(UE_NOT_QUALIFIED_TO_UNSTAKE, "not qualified to unstake") \
  UERROR(UE_NOT_QUALIFIED_TO_UNLOCK, "not qualified to unlock") \
  UERROR(UE_GET_CLAIMED_AMOUNT_BY_THE_DELEGATINGTOR,"Obtain the amount claimed by the delegatingor")                      \
  UERROR(UE_GET_UNAVAILABLE_ASSET, "Get available asset fail")         \
  UERROR(UE_GET_GAS_AMOUNT_BY_TIME_TYPE_FAILED,"Get gas amount by time type")                                        \
  UERROR(UE_NO_FUND_REWARD_THE_DAY_BEFORE,"There was no fund reward the day before!")                           \
  UERROR(UE_HAVE_CLAIMED_THE_FUND_REWARD,  "Have already claimed the fund reward!")                              \
  UERROR(UE_CHECK_INELIGIBILITY_FOR_RECEIVING_FUND,"Checking eligibility for receiving fund")                            \
  UERROR(UE_VOTE_INFO_CHECK_FAIL, "vote info check fail")              \
  UERROR(UE_LESS_THAN_MIN_LOCK_AMOUNT, " less than min lock amount")            \
  UERROR(UE_CHECK_GAS_ASSET_FAILED, "check gas asset isn't qualified")                                           \
  UERROR(UE_END,"utxo_end")\


#define RPC_ERRORS \
    UERROR(RPC_ERROR_BASE=3000,"RPC_ERROR_BASE")\
    UERROR(RPC_ERROR_NOT_FOUND_ACOUNT,"not found acount")\
