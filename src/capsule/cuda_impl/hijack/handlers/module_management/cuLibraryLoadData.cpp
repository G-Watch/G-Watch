#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/system.hpp"
#include "capsule/capsule.hpp"
#include "capsule/cuda_impl/hijack/runtime.hpp"
#include "capsule/cuda_impl/hijack/real_apis.hpp"


CUresult cuLibraryLoadData (
    CUlibrary* library,
    const void* code,
    CUjit_option* jitOptions,
    void** jitOptionsValues,
    unsigned int numJitOptions,
    CUlibraryOption* libraryOptions,
    void** libraryOptionValues,
    unsigned int numLibraryOptions
){
    CUresult cudv_retval = CUDA_SUCCESS;
    gw_retval_t gw_retval = GW_SUCCESS;

    GW_CHECK_POINTER(capsule);
    
    // obtain the real cuda apis
    __init_real_cuda_apis();


    /* ================ call actual function ================ */
    cudv_retval = real_cuLibraryLoadData(
        library, code, jitOptions, jitOptionsValues,
        numJitOptions, libraryOptions, libraryOptionValues, numLibraryOptions
    );
    GW_DEBUG("called cuLibraryLoadData");
    /* ================ call actual function ================ */


    /* ================ cache the fatbin ================ */
    // NOTE(zhuobin):   we cache the library instead of directly parse the fatbinary,
    //                  to save initialization time
    if(likely(cudv_retval == CUDA_SUCCESS)){
        GW_IF_FAILED(
            capsule->CUDA_cache_culibrary(*library, (const uint8_t*)code),
            gw_retval,
            {
                GW_WARN_DETAIL("failed to cache CUDA library");
            }
            goto exit;
        );
    }
    /* ================ cache the fatbin ================ */

    // if operation isn't success, we sync all previous messages to scheduler to be finished
    if(cudv_retval != CUDA_SUCCESS){
        capsule->sync_send_to_scheduler();
    }

exit:
    return cudv_retval;
}
