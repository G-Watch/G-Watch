#include <iostream>
#include <string>

#include <dlfcn.h>
#include <pthread.h>

#include "nlohmann/json.hpp"

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"
#include "scheduler/serve/capsule_message.hpp"
#include "capsule/capsule.hpp"
#include "capsule/trace.hpp"
#include "binding/runtime_control.hpp"


GWTraceTask* gw_rt_control_create_trace_task(std::string type){
    GWTraceTask* trace_task = nullptr;
    static bool is_hijacklib_loaded = __init_capsule();

    if(is_hijacklib_loaded){
        trace_task = GWTraceTaskFactory::instance().create(type);
    }

    return trace_task;
}


void gw_rt_control_destory_trace_task(GWTraceTask* trace_task){
    GWTraceTask* trace_task_ = nullptr;
    static bool is_hijacklib_loaded = __init_capsule();

    if(is_hijacklib_loaded){
        GW_CHECK_POINTER(trace_task_ = (GWTraceTask*)trace_task);
        delete trace_task_;
    }
}


void gw_rt_control_register_trace_task(GWTraceTask* trace_task){
    gw_retval_t retval = GW_SUCCESS;
    static bool is_hijacklib_loaded = __init_capsule();
    GWTraceTask* trace_task_ = nullptr;

    GW_CHECK_POINTER(trace_task_ = (GWTraceTask*)trace_task);

    if(is_hijacklib_loaded){
        GW_CHECK_POINTER(capsule != nullptr);

        GW_IF_FAILED(
            capsule->register_trace_task(trace_task_),
            retval,
            {
                throw GWException("failed to register trace task");
            }
        );
    }
}


void gw_rt_control_unregister_trace_task(GWTraceTask* trace_task){
    gw_retval_t retval;
    static bool is_hijacklib_loaded = __init_capsule();
    GWTraceTask* trace_task_ = nullptr;

    GW_CHECK_POINTER(trace_task_ = (GWTraceTask*)trace_task);

    if(is_hijacklib_loaded){
        GW_CHECK_POINTER(capsule != nullptr);

        GW_IF_FAILED(
            capsule->unregister_trace_task(trace_task_),
            retval,
            {
                throw GWException("failed to unregister trace task definition");
            }
        );
    }
}
