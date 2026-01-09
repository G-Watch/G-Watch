#include <iostream>
#include <string>

#include "common/common.hpp"
#include "common/log.hpp"

const char* gw_retval_str(gw_retval_t retval){
    switch (retval)
    {
    case GW_SUCCESS:
        return "no error";

    case GW_FAILED:
        return "unknown reason";

    case GW_FAILED_HARDWARE:
        return "hardware failure";

    case GW_FAILED_SDK:
        return "underlying sdk failure";
    
    case GW_FAILED_NOT_READY:
        return "resource not ready";

    case GW_FAILED_NOT_EXIST:
        return "resource not exist";

    case GW_FAILED_ALREADY_EXIST:
        return "resource already exist";

    case GW_FAILED_NOT_IMPLEMENTAED:
        return "not implemented";

    case GW_FAILED_INVALID_INPUT:
        return "invalid input";

    default:
        GW_ERROR_DETAIL("shouldn't be here");
    }
}
