#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "capsule/hijack/cuda_impl/runtime.hpp"
#include "common/cuda_impl/real_apis.hpp"


CUresult cuModuleLoad (
    CUmodule* module,
    const char* fname
){
    using this_func_t = CUresult(CUmodule*, const char*);
    CUresult cudv_retval = CUDA_SUCCESS;
    gw_retval_t gw_retval = GW_SUCCESS;

    GW_CHECK_POINTER(capsule);
    
    // obtain the real cuda apis
    __init_real_cuda_apis();

    /* ================ call actual function ================ */
    cudv_retval = real_cuModuleLoad(module, fname);
    GW_DEBUG("called cuModuleLoad");
    /* ================ call actual function ================ */

    /* ================ cache the fatbin ================ */
    if(likely(cudv_retval == CUDA_SUCCESS)){
        GW_IF_FAILED(
            capsule->CUDA_cache_cumodule(*module, std::string(fname)),
            gw_retval,
            {
                GW_WARN_DETAIL("failed to cache CUDA module");
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
