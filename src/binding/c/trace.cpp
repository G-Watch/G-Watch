#include <iostream>
#include <string>

#include <pthread.h>
#include <stdint.h>
#include <dlfcn.h>

#include "nlohmann/json.hpp"

#include "common/common.hpp"
#include "common/utils/exception.hpp"
#include "capsule/event.hpp"
#include "capsule/trace.hpp"
#include "binding/runtime_control.hpp"

// public header
#include "gwatch/capsule.hpp"


namespace gwatch {


TraceEvent::TraceEvent(std::string name) {
    GWEvent *app_event = nullptr;

    app_event = new GWEvent(name);
    app_event->thread_id = (uint64_t)(pthread_self());
    app_event->type_id = GW_EVENT_TYPE_APP;

    this->_gw_event_handle = (void*)app_event;
}


TraceEvent::~TraceEvent() {
    // NOTE(zhuobin):
    // this->_gw_event_handle should be deleted after sending to scheduler
}


gwError TraceEvent::RecordTick(std::string tick_name){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWEvent *app_event = nullptr;

    if(this->_gw_event_handle){
        GW_CHECK_POINTER(app_event = (GWEvent*)this->_gw_event_handle);
        app_event->record_tick(tick_name);
    } else {
        throw std::runtime_error("TraceEvent::RecordTick: event handle is null");
    }

exit:
    return retval;
}


gwError TraceEvent::SetMetadata(std::string key, std::string value){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWEvent *app_event = nullptr;

    if(this->_gw_event_handle){
        GW_CHECK_POINTER(app_event = (GWEvent*)this->_gw_event_handle);
        app_event->set_metadata(key, value);
    } else {
        throw std::runtime_error("TraceEvent::SetMetadata: event handle is null");
    }

exit:
    return retval;
}


void TraceEvent::Archive(){
    GWEvent *app_event = nullptr;

    if(this->_gw_event_handle){
        GW_CHECK_POINTER(app_event = (GWEvent*)this->_gw_event_handle);
        app_event->archive();
    } else {
        throw std::runtime_error("TraceEvent::Archive: event handle is null");
    }
}


bool TraceEvent::IsArchived(){
    bool retval = false;
    GWEvent *app_event = nullptr;

    if(this->_gw_event_handle){
        GW_CHECK_POINTER(app_event = (GWEvent*)this->_gw_event_handle);
        retval = app_event->is_archived();
    } else {
        throw std::runtime_error("TraceEvent::IsArchived: event handle is null");
    }

    return retval;
}


TraceTask::TraceTask(std::string type){
    this->_gw_trace_task_handle = (void*)gw_rt_control_create_trace_task(type);
    GW_CHECK_POINTER(this->_gw_trace_task_handle);
}


TraceTask::~TraceTask(){
    if(this->_gw_trace_task_handle)
        gw_rt_control_destory_trace_task((GWTraceTask*)this->_gw_trace_task_handle);
}


gwError TraceTask::GetMetadata(std::string key, std::string &value){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWTraceTask *trace_task = nullptr;
    nlohmann::json json_value;

    if(this->_gw_trace_task_handle){
        GW_CHECK_POINTER(trace_task = (GWTraceTask*)this->_gw_trace_task_handle);
        GW_IF_FAILED(
            trace_task->get_metadata(key, json_value),
            gw_retval,
            {
                retval = gwFailed;
                goto exit;
            }
        );
    } else {
        throw std::runtime_error("TraceTask::GetMetadata: trace task handle is null");
    }

    value = json_value.dump();

exit:
    return retval;
}


gwError TraceTask::SetMetadata(std::string key, std::string value){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWTraceTask *trace_task = nullptr;
    nlohmann::json json_value = nlohmann::json::parse(value);

    if(this->_gw_trace_task_handle){
        GW_CHECK_POINTER(trace_task = (GWTraceTask*)this->_gw_trace_task_handle);
        trace_task->set_metadata(key, json_value);
    } else {
        throw std::runtime_error("TraceTask::SetMetadata: trace task handle is null");
    }

exit:
    return retval;
}


bool TraceTask::HasMetadata(std::string key){
    bool retval = false;
    GWTraceTask *trace_task = nullptr;

    if(this->_gw_trace_task_handle){
        GW_CHECK_POINTER(trace_task = (GWTraceTask*)this->_gw_trace_task_handle);
        retval = trace_task->has_metadata(key);
    } else {
        throw std::runtime_error("TraceTask::HasMetadata: trace task handle is null");
    }

    return retval;
}


TraceContext::TraceContext()
{}


TraceContext::~TraceContext()
{}


gwError TraceContext::StartTracing(std::string name, uint64_t hash, std::string line_position){
    gwError ret = gwSuccess;
    gw_rt_control_start_app_trace(name, hash, line_position);
    return ret;
}


gwError TraceContext::StopTracing(uint64_t begin_hash, uint64_t end_hash, std::string line_position){
    gwError ret = gwSuccess;
    gw_rt_control_stop_app_trace(begin_hash, end_hash, line_position);
    return ret;
}


gwError TraceContext::PushEvent(TraceEvent *event){
    gwError ret = gwSuccess;
    gw_rt_control_push_app_event((GWEvent*)event->_gw_event_handle);
    return ret;
}


gwError TraceContext::PushParentEvent(TraceEvent *event){
    gwError ret = gwSuccess;
    gw_rt_control_push_app_parent_event((GWEvent*)event->_gw_event_handle);
    return ret;
}


gwError TraceContext::PopParentEvent(){
    gwError ret = gwSuccess;
    gw_rt_control_pop_app_parent_event();
    return ret;
}


gwError TraceContext::RegisterTraceTask(TraceTask *task){
    gwError ret = gwSuccess;
    gw_rt_control_register_trace_task((GWTraceTask*)task->_gw_trace_task_handle);
    return ret;
}


gwError TraceContext::UnregisterTraceTask(TraceTask *task){
    gwError ret = gwSuccess;
    gw_rt_control_unregister_trace_task((GWTraceTask*)task->_gw_trace_task_handle);
    return ret;
}


} // namespace gwatch
