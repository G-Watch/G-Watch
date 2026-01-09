#include <iostream>

#include <dlfcn.h>
#include <string.h>
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

CUresult cuLaunchKernelEx (
    const CUlaunchConfig* config,
    CUfunction f,
    void** kernelParams,
    void** extra
){
    using this_func_t = CUresult(const CUlaunchConfig*, CUfunction, void**, void**);
    using cu_launch_host_func_t = CUresult(CUstream, CUhostFn, void*);

    gw_retval_t gw_retval = GW_SUCCESS;
    CUresult cudv_retval = CUDA_SUCCESS, tmp_cudv_retval;
    GWEvent *cpu_event = nullptr, *gpu_event = nullptr;
    const char* func_name = nullptr;
    bool cuLaunchKernel_at_first_level = false;

    auto __record_kernel_begin_tick = [](void *user_data){
        GWEvent *event = (GWEvent*)user_data;
        GW_CHECK_POINTER(event);
        event->record_tick("begin");
    };

    auto __record_kernel_end_tick = [](void *user_data){
        GWEvent *event = (GWEvent*)user_data;
        GW_CHECK_POINTER(event);
        event->record_tick("end");
        event->archive();;
    };

    GW_CHECK_POINTER(capsule);
    GW_CHECK_POINTER((void*)(f));

    // obtain the real cuda apis
    __init_real_cuda_apis();

    // avoid do recursive tracing
    cuLaunchKernel_at_first_level = global_cuLaunchKernel_at_first_level;
    if(global_cuLaunchKernel_at_first_level == true){
        global_cuLaunchKernel_at_first_level = false;
    }

    // obtain function name
    if(unlikely(cuLaunchKernel_at_first_level && _map_function_name.count(f) == 0)){
        GW_IF_CUDA_DRIVER_FAILED(
            cuFuncGetName(&func_name, f),
            tmp_cudv_retval,
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
                "failed to trace function: api(cuLaunchKernelEx), err(%s)",
                gw_retval_str(gw_retval)
            );
        );
    }

    // capture the kernel launch behaviour
    if(capsule->is_capturing()){
        capsule->ensure_event_trace();

        GW_CHECK_POINTER(cpu_event = new GWEvent("cuLaunchKernelEx"));
        cpu_event->type_id = GW_EVENT_TYPE_CPU;
        cpu_event->thread_id = (uint64_t)(pthread_self());
        cpu_event->set_metadata("function", (uint64_t)f);
        cpu_event->set_metadata("function_name", func_name ? func_name : "unknown kernel");
        GW_IF_FAILED(
            GWCapsule::event_trace->push_event(cpu_event),
            gw_retval,
            {
                GW_WARN(
                    "failed to push cpu event to capsule: api(cuLaunchKernelEx), err(%s)",
                    gw_retval_str(gw_retval)
                );
                delete cpu_event;
                cpu_event = nullptr;
            }
        );

        GW_CHECK_POINTER(gpu_event = new GWEvent(func_name ? func_name : "unknown kernel"));
        gpu_event->type_id = GW_EVENT_TYPE_GPU;
        gpu_event->thread_id = (uint64_t)(config->hStream);
        gpu_event->set_metadata("function", (uint64_t)f);
        gpu_event->set_metadata("grid_dim_x", config->gridDimX);
        gpu_event->set_metadata("grid_dim_y", config->gridDimY);
        gpu_event->set_metadata("grid_dim_z", config->gridDimZ);
        gpu_event->set_metadata("block_dim_x", config->blockDimX);
        gpu_event->set_metadata("block_dim_y", config->blockDimY);
        gpu_event->set_metadata("block_dim_z", config->blockDimZ);
        gpu_event->set_metadata("shared_mem_bytes", config->sharedMemBytes);
        gpu_event->set_metadata("stream", (uint64_t)config->hStream);
        GW_IF_FAILED(
            GWCapsule::event_trace->push_event(gpu_event),
            gw_retval,
            {
                GW_WARN(
                    "failed to push gpu event to capsule: api(cuLaunchKernelEx), err(%s)",
                    gw_retval_str(gw_retval)
                );
                delete gpu_event;
                gpu_event = nullptr;
            }
        );
    }

    // add connections
    if(cpu_event != nullptr && gpu_event != nullptr){
        cpu_event->add_related_event(gpu_event);
    }

    if(cpu_event != nullptr){ 
        cpu_event->record_tick("begin");
    }
    if(gpu_event != nullptr){ 
        tmp_cudv_retval = real_cuLaunchHostFunc(config->hStream, __record_kernel_begin_tick, (void*)(gpu_event));
        /* CUDA could have sticky error, so we manully set the end timing of GPU event at CPU if it fails */
        if(tmp_cudv_retval != CUDA_SUCCESS){
            gpu_event->record_tick("begin");
        }
    }

    cudv_retval = real_cuLaunchKernelEx(config, f, kernelParams, extra);
    GW_DEBUG("called cuLaunchKernelEx");

    if(cpu_event != nullptr){ 
        cpu_event->record_tick("end");
        cpu_event->set_metadata("return code", cudv_retval);
        cpu_event->archive();
    }
    if(gpu_event != nullptr){ 
        tmp_cudv_retval = real_cuLaunchHostFunc(config->hStream, __record_kernel_end_tick, (void*)(gpu_event));
        /* CUDA could have sticky error, so we manully set the end timing of GPU event at CPU if it fails */
        if(tmp_cudv_retval != CUDA_SUCCESS){
            gpu_event->record_tick("end");
            gpu_event->set_metadata("return code", cudv_retval);
            gpu_event->archive();
        }
    }

    if(cuLaunchKernel_at_first_level == true){
        GW_ASSERT(global_cuLaunchKernel_at_first_level == false);
        global_cuLaunchKernel_at_first_level = true;
    }

    // if operation isn't success, we sync all previous messages to scheduler to be finished
    if(cudv_retval != CUDA_SUCCESS){
        capsule->sync_send_to_scheduler();
    }

    return cudv_retval;
}


} // extern "C"
