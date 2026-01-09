#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "capsule/cuda_impl/hijack/runtime.hpp"
#include "capsule/cuda_impl/hijack/real_apis.hpp"


CUresult cuLibraryLoadFromFile (
    CUlibrary* library,
    const char* fileName,
    CUjit_option* jitOptions,
    void** jitOptionsValues,
    unsigned int  numJitOptions,
    CUlibraryOption* libraryOptions,
    void** libraryOptionValues,
    unsigned int  numLibraryOptions
){
    using this_func_t = CUresult(
        CUlibrary*, const char*, CUjit_option*,
        void**, unsigned int, CUlibraryOption*,
        void**, unsigned int
    );
    CUresult cudv_retval = CUDA_SUCCESS;
    gw_retval_t gw_retval = GW_SUCCESS;

    GW_CHECK_POINTER(capsule);

    // obtain the real cuda apis
    __init_real_cuda_apis();


    /* ================ call actual function ================ */
    cudv_retval = real_cuLibraryLoadFromFile(
        library, fileName, jitOptions, jitOptionsValues,
        numJitOptions, libraryOptions, libraryOptionValues,
        numLibraryOptions
    );
    GW_DEBUG("called cuLibraryLoadFromFile");
    /* ================ call actual function ================ */


    /* ================ cache the fatbin ================ */
    if(likely(cudv_retval == CUDA_SUCCESS)){
        GW_IF_FAILED(
            capsule->CUDA_cache_culibrary(*library, std::string(fileName)),
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
