/**
 * *****************************************************************************
 * @file        block_compare.h
 * @brief       
 * @date        2023-09-25
 * @copyright   mm
 * *****************************************************************************
 */

#ifndef CA_BLOCK_COMPARE_HEADER
#define CA_BLOCK_COMPARE_HEADER

#include "block.pb.h"

struct BlockComparator
{
    bool operator()(const CBlock &a, const CBlock &b) const
    {
        if (a.height() > b.height())
        {
            return true;
        }
        else if (a.height() == b.height())
        {
            if (a.time() > b.time())
            {
                return true;
            }
        }
        return false;
    }
};

#endif 
