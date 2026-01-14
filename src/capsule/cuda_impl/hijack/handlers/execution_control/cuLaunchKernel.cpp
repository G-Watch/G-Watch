#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <pthread.h>

#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "capsule/event.hpp"
#include "capsule/cuda_impl/hijack/runtime.hpp"
#include "capsule/cuda_impl/hijack/real_apis.hpp"


extern "C" {

static thread_local std::map<CUfunction, std::string> _map_function_name;
extern thread_local bool global_cuLaunchKernel_at_first_level;

CUresult cuLaunchKernel (
    CUfunction f,
    unsigned int gridDimX,
    unsigned int gridDimY,
    unsigned int gridDimZ,
    unsigned int blockDimX,
    unsigned int blockDimY,
    unsigned int blockDimZ,
    unsigned int sharedMemBytes,
    CUstream hStream,
    void** kernelParams,
    void** extra
){
    gw_retval_t gw_retval = GW_SUCCESS;
    CUresult cudv_retval = CUDA_SUCCESS;
    const char* func_name = nullptr;
    bool cuLaunchKernel_at_first_level = false;

    GW_CHECK_POINTER(capsule);
    GW_CHECK_POINTER((void*)(f));

    // avoid do recursive tracing
    cuLaunchKernel_at_first_level = global_cuLaunchKernel_at_first_level;
    if(global_cuLaunchKernel_at_first_level == true){
        global_cuLaunchKernel_at_first_level = false;
    }

    // obtain function name
    if(unlikely(cuLaunchKernel_at_first_level && _map_function_name.count(f) == 0)){
        GW_IF_CUDA_DRIVER_FAILED(
            cuFuncGetName(&func_name, f),
            cudv_retval,
            {
                func_name = nullptr;
                GW_WARN("failed to obtain function name: func(%p)", f);
            }
        );
        _map_function_name[f] = func_name;
    } else {
        func_name = _map_function_name[f].c_str();
    }

    // trace the function
    if(cuLaunchKernel_at_first_level && capsule->do_need_trace_kernel(func_name)){
        GW_IF_FAILED(
            capsule->CUDA_trace_single_kernel(
                f, func_name, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ,
                sharedMemBytes, hStream, kernelParams, extra
            ),
            gw_retval,
            GW_WARN(
                "failed to trace function: api(cuLaunchKernel), err(%s)",
                gw_retval_str(gw_retval)
            );
        );
    }

    // execute the real cuda api
    cudv_retval = real_cuLaunchKernel(
        f, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY,
        blockDimZ, sharedMemBytes, hStream, kernelParams, extra
    );
    GW_DEBUG("called cuLaunchKernel");

    if(cuLaunchKernel_at_first_level == true){
        GW_ASSERT(global_cuLaunchKernel_at_first_level == false);
        global_cuLaunchKernel_at_first_level = true;
    }

    // if operation isn't success, we sync all previous messages to scheduler to be finished
    if(unlikely(cudv_retval != CUDA_SUCCESS)){
        capsule->sync_send_to_scheduler();
    }

    return cudv_retval;
}


} // extern "C"
