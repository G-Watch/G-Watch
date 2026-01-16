#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "capsule/hijack/cuda_impl/runtime.hpp"
#include "common/cuda_impl/real_apis.hpp"


CUresult cuLibraryGetModule(
    CUmodule* module,
    CUlibrary library
){
    gw_retval_t gw_retval = GW_SUCCESS;
    CUresult cudv_retval = CUDA_SUCCESS;

    GW_CHECK_POINTER(capsule);

    // obtain the real cuda apis
    __init_real_cuda_apis();

    cudv_retval = real_cuLibraryGetModule(module, library);
    GW_DEBUG("called cuLibraryGetModule");

    if(cudv_retval == CUDA_SUCCESS){
        GW_IF_FAILED(
            capsule->CUDA_record_mapping_cumodule_culibrary(*module, library),
            gw_retval,
            {
                GW_WARN("failed to record mapping between CUmodule and CUlibrary");
            }
        );
    }

    // if operation isn't success, we sync all previous messages to scheduler to be finished
    if(cudv_retval != CUDA_SUCCESS){
        capsule->sync_send_to_scheduler();
    }

    return cudv_retval;
}
