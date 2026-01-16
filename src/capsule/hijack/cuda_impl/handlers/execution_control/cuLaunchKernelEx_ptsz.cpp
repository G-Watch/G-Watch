#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "common/assemble/kernel.hpp"
#include "common/cuda_impl/assemble/kernel_cuda.hpp"
#include "capsule/hijack/cuda_impl/runtime.hpp"
#include "common/cuda_impl/real_apis.hpp"


extern "C" {

static thread_local std::map<CUfunction, std::string> _map_function_name;
static thread_local std::map<CUfunction, GWKernelDef*> _map_function_kerneldef;
extern thread_local bool global_cuLaunchKernel_at_first_level;

CUresult cuLaunchKernelEx_ptsz (
    const CUlaunchConfig* config,
    CUfunction f,
    void** kernelParams,
    void** extra
){
    gw_retval_t gw_retval = GW_SUCCESS;
    CUresult cudv_retval = CUDA_SUCCESS;
    const char* func_name = nullptr;
    bool cuLaunchKernel_at_first_level = false;
    GWKernel *kernel = nullptr;
    GWKernelDef *kernel_def = nullptr;
    GWKernelExt_CUDA *kernel_ext = nullptr;

    GW_CHECK_POINTER(capsule);
    GW_CHECK_POINTER((void*)(f));

    // obtain the real cuda apis
    __init_real_cuda_apis();

    // avoid do recursive tracing
    cuLaunchKernel_at_first_level = global_cuLaunchKernel_at_first_level;
    if(global_cuLaunchKernel_at_first_level == true){
        global_cuLaunchKernel_at_first_level = false;
    }

    // obtain kernel def
    if(unlikely(_map_function_kerneldef.count(f) == 0)){
        GW_IF_FAILED(
            capsule->CUDA_get_kerneldef_by_cufunction(f, kernel_def),
            gw_retval,
            {
                GW_WARN(
                    "failed to obtain kernel def from capsule: func(%p), err(%s)",
                    f,
                    gw_retval_str(gw_retval)
                );
                goto execute_real_apis;
            }
        );
        _map_function_kerneldef[f] = kernel_def;
    } else {
        kernel_def = _map_function_kerneldef[f];
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

    // trace kernel launch
    if(cuLaunchKernel_at_first_level && capsule->is_tracing_kernel_launch()){
        GW_CHECK_POINTER(kernel_ext = GWKernelExt_CUDA::create(kernel_def));
        GW_CHECK_POINTER(kernel = kernel_ext->get_base_ptr());
        kernel->block_dim_x = config->blockDimX;
        kernel->block_dim_y = config->blockDimY;
        kernel->block_dim_z = config->blockDimZ;
        kernel->grid_dim_x = config->gridDimX;
        kernel->grid_dim_y = config->gridDimY;
        kernel->grid_dim_z = config->gridDimZ;
        kernel->shared_mem_bytes = config->sharedMemBytes;
        kernel->stream = (uint64_t)config->hStream;
        kernel_ext->params().params = kernelParams;
        kernel_ext->params().attrs = config->attrs;
        kernel_ext->params().num_attrs = config->numAttrs;
        kernel_ext->params().extra = extra;
        capsule->append_kernel_launch(kernel);
    }

    // trace the function
    if(unlikely(cuLaunchKernel_at_first_level && capsule->do_need_trace_kernel(func_name))){
        GW_IF_FAILED(
            capsule->CUDA_trace_single_kernel(
                f, func_name,
                config->gridDimX, config->gridDimY, config->gridDimZ,
                config->blockDimX, config->blockDimY, config->blockDimZ,
                config->sharedMemBytes,
                config->hStream,
                kernelParams,
                extra,
                config->attrs,
                config->numAttrs
            ),
            gw_retval,
            GW_WARN(
                "failed to trace function: api(cuLaunchKernelEx_ptsz), err(%s)",
                gw_retval_str(gw_retval)
            );
        );
    }

 execute_real_apis:
    // execute the real cuda api
    cudv_retval = real_cuLaunchKernelEx_ptsz(config, f, kernelParams, extra);
    GW_DEBUG("called cuLaunchKernelEx_ptsz");

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
