#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "capsule/cuda_impl/hijack/runtime.hpp"
#include "capsule/cuda_impl/hijack/real_apis.hpp"


CUresult cuModuleLoadDataEx (
    CUmodule* module,
    const void* image,
    unsigned int numOptions,
    CUjit_option* options,
    void** optionValues
){
    using this_func_t = CUresult(
        CUmodule* module, const void*, unsigned int, CUjit_option*, void**);

    CUresult cudv_retval = CUDA_SUCCESS;

    GW_CHECK_POINTER(capsule);

    // obtain the real cuda apis
    __init_real_cuda_apis();
    
    cudv_retval = real_cuModuleLoadDataEx(module, image, numOptions, options, optionValues);
    GW_DEBUG("called cuModuleLoadDataEx");

    // if operation isn't success, we sync all previous messages to scheduler to be finished
    if(cudv_retval != CUDA_SUCCESS){
        capsule->sync_send_to_scheduler();
    }

exit:
    return cudv_retval;
}
