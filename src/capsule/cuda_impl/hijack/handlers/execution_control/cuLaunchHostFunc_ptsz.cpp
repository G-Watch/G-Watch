#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "capsule/cuda_impl/hijack/runtime.hpp"
#include "capsule/cuda_impl/hijack/real_apis.hpp"


CUresult cuLaunchHostFunc_ptsz(
    CUstream hStream,
    CUhostFn fn,
    void* userData
){
    gw_retval_t gw_retval = GW_SUCCESS;
    CUresult cudv_retval = CUDA_SUCCESS, tmp_cudv_retval;
    GWEvent *cpu_event = nullptr, *gpu_event = nullptr;

    auto __record_kernel_begin_tick = [](void *user_data){
        GWEvent *event = (GWEvent*)user_data;
        GW_CHECK_POINTER(event);
        event->record_tick("begin");
    };

    auto __record_kernel_end_tick = [](void *user_data){
        GWEvent *event = (GWEvent*)user_data;
        GW_CHECK_POINTER(event);
        event->record_tick("end");
        event->archive();
    };

    GW_CHECK_POINTER(capsule);
    GW_CHECK_POINTER((void*)(fn));

    // obtain the real cuda apis
    __init_real_cuda_apis();

    // capture the kernel launch behaviour
    if(capsule->is_capturing()){
        capsule->ensure_event_trace();

        GW_CHECK_POINTER(cpu_event = new GWEvent("cuLaunchHostFunc_ptsz"));
        cpu_event->type_id = GW_EVENT_TYPE_CPU;
        cpu_event->thread_id = (uint64_t)(pthread_self());
        cpu_event->set_metadata("function", (uint64_t)fn);
        GW_IF_FAILED(
            GWCapsule::event_trace->push_event(cpu_event),
            gw_retval,
            {
                GW_WARN(
                    "failed to push cpu event to capsule: api(cuLaunchHostFunc_ptsz), err(%s)",
                    gw_retval_str(gw_retval)
                );
                delete cpu_event;
                cpu_event = nullptr;
            }
        );

        GW_CHECK_POINTER(gpu_event = new GWEvent("host_function"));
        gpu_event->type_id = GW_EVENT_TYPE_GPU;
        gpu_event->thread_id = (uint64_t)(hStream);
        gpu_event->set_metadata("function", (uint64_t)fn);
        gpu_event->set_metadata("stream", (uint64_t)hStream);
        GW_IF_FAILED(
            GWCapsule::event_trace->push_event(gpu_event),
            gw_retval,
            {
                GW_WARN(
                    "failed to push gpu event to capsule: api(cuLaunchHostFunc_ptsz), err(%s)",
                    gw_retval_str(gw_retval)
                );
                delete gpu_event;
                gpu_event = nullptr;
            }
        );
    }

    if(cpu_event != nullptr){ 
        cpu_event->record_tick("begin");
    }
    if(gpu_event != nullptr){ 
        tmp_cudv_retval = real_cuLaunchHostFunc(hStream, __record_kernel_begin_tick, (void*)(gpu_event));
        /* CUDA could have sticky error, so we manully set the end timing of GPU event at CPU if it fails */
        if(tmp_cudv_retval != CUDA_SUCCESS){
            gpu_event->record_tick("begin");
        }
    }

    cudv_retval = real_cuLaunchHostFunc_ptsz(hStream, fn, userData);
    GW_DEBUG("called cuLaunchHostFunc_ptsz");

    if(cpu_event != nullptr){ 
        cpu_event->record_tick("end");
        cpu_event->set_metadata("return code", cudv_retval);
        cpu_event->archive();
    }
    if(gpu_event != nullptr){ 
        tmp_cudv_retval = real_cuLaunchHostFunc(hStream, __record_kernel_end_tick, (void*)(gpu_event));
        /* CUDA could have sticky error, so we manully set the end timing of GPU event at CPU if it fails */
        if(tmp_cudv_retval != CUDA_SUCCESS){
            gpu_event->record_tick("end");
            gpu_event->set_metadata("return code", cudv_retval);
            gpu_event->archive();
        }
    }

    // if operation isn't success, we sync all previous messages to scheduler to be finished
    if(cudv_retval != CUDA_SUCCESS){
        capsule->sync_send_to_scheduler();
    }

    return cudv_retval;
}
