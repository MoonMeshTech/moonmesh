#pragma once
#include "utils/uerror/ecode.h"
#include "errorn/error_module.h"
#include "errorn/rpc/error.def.h"
#include "errorn/global/error.def.h"
#include "errorn/ca/error.def.h"
#include <vector>
#include <cstdint>



#define EXP_ERROR(a,b) b,
   ERROR_MESSAGE_MACRO_DF(RPC_AE,RPC_PASE_ERROR GOLBA_ERROR);
#undef EXP_ERROR

#define EXP_ERROR(a,b) a,
    ERROR_ENUM_MACRO_DF(RPC_AE,ERROR_MOUDLE,RPC_API,RPC_PASE_ERROR GOLBA_ERROR);
#undef EXP_ERROR


ERROR_SERIALIZER_MACRO_DF(RPC_AE)

