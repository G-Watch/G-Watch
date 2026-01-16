#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "capsule/hijack/cuda_impl/runtime.hpp"
#include "common/cuda_impl/real_apis.hpp"


CUresult cuModuleLoadFatBinary(
    CUmodule* module,
    const void* fatCubin
){
    using this_func_t = CUresult(CUmodule*, const void*);
    CUresult cudv_retval = CUDA_SUCCESS;

    GW_CHECK_POINTER(capsule);
    
    // obtain the real cuda apis
    __init_real_cuda_apis();
    
    cudv_retval = real_cuModuleLoadFatBinary(module, fatCubin);
    GW_DEBUG("called cuModuleLoadFatBinary");
    
    // if operation isn't success, we sync all previous messages to scheduler to be finished
    if(cudv_retval != CUDA_SUCCESS){
        capsule->sync_send_to_scheduler();
    }

exit:
    return cudv_retval;
}
