#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/cuda_impl/real_apis.hpp"
#include "capsule/capsule.hpp"
#include "capsule/hijack/cuda_impl/runtime.hpp"


extern "C"
{

/*!
 *  \brief  hijack the dlsym function for intercepting attempt to
 *          load the cuGetProcAddress symbol
 *  \param  handle  handle of the library
 *  \param  symbol  symbol to load
 *  \return pointer to the symbol
 */
void *dlsym(void *handle, const char *symbol){
    void *ptr;

    // GW_DEBUG("dlsym symbol: %s", symbol);

    // early out if not a CUDA driver symbol
    if (strncmp(symbol, "cu", 2) != 0) {
        goto normal_dlsym;
    }

    if(unlikely(strcmp(symbol, "cuGetProcAddress") == 0)){
        return (void*)(cuGetProcAddress);
    }
    #if CUDA_VERSION >= 12030
        if(unlikely(strcmp(symbol, "cuGetProcAddress_v2") == 0)){
            return (void*)(cuGetProcAddress);
        }
    #endif

    // hook CUDA driver APIs
    if((ptr = __hook_cuda_driver_api(symbol)) != nullptr){
        return ptr;
    }

 normal_dlsym:
    __init_real_dlsym();
    return real_dlsym(handle, symbol);
}

} // extern "C"
