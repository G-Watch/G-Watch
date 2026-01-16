#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "capsule/hijack/cuda_impl/runtime.hpp"
#include "common/cuda_impl/real_apis.hpp"


extern "C" {

CUresult cuLaunchHostFunc_ptsz(
    CUstream hStream,
    CUhostFn fn,
    void* userData
){
    gw_retval_t gw_retval = GW_SUCCESS;
    CUresult cudv_retval = CUDA_SUCCESS;

    GW_CHECK_POINTER(capsule);
    GW_CHECK_POINTER((void*)(fn));

    // obtain the real cuda apis
    __init_real_cuda_apis();

    // execute the real cuda api
    cudv_retval = real_cuLaunchHostFunc_ptsz(hStream, fn, userData);
    GW_DEBUG("called cuLaunchHostFunc_ptsz");

    // if operation isn't success, we sync all previous messages to scheduler to be finished
    if(unlikely(cudv_retval != CUDA_SUCCESS)){
        capsule->sync_send_to_scheduler();
    }

    return cudv_retval;
}

} // extern "C"
