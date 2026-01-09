#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "capsule/cuda_impl/hijack/runtime.hpp"
#include "capsule/cuda_impl/hijack/real_apis.hpp"


extern "C" {


CUresult cuGetProcAddress(
    const char *symbol,
    void **pfn,
    int driverVersion,
    cuuint64_t flags,
    CUdriverProcAddressQueryResult *symbolStatus
){
    __init_real_cuda_apis();

    // GW_DEBUG("cuGetProcAddress symbol: %s", symbol);

    // nVIDIA would use cuGetProcAddress to find cuGetProcAddress again,
    // don't be fooled by it, we still return our hooked cuGetProcAddress
    if(unlikely(strcmp(symbol, "cuGetProcAddress") == 0)){
        *pfn = (void*)(cuGetProcAddress);
        //! \note   never deref symbolStatus, sometimes it passes a empty pointer
        // *symbolStatus = CU_GET_PROC_ADDRESS_SUCCESS;
        return CUDA_SUCCESS;
    }

    *pfn = __hook_cuda_driver_api(symbol);
    if(*pfn != nullptr){
        //! \note   never deref symbolStatus, sometimes it passes a empty pointer
        // *symbolStatus = CU_GET_PROC_ADDRESS_SUCCESS;
        return CUDA_SUCCESS;
    }

    // call real cuGetProcAddress
    return real_cuGetProcAddress(symbol, pfn, driverVersion, flags, symbolStatus);
}


} // extern "C"
