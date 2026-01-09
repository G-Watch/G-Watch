#include <iostream>
#include <string>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"
#include "capsule/capsule.hpp"
#include "binding/internal_interface.hpp"
#include "scheduler/serve/capsule_message.hpp"


void GW_INTERNAL_start_app_trace(std::string name, uint64_t hash, std::string line_position){
    gw_retval_t retval = GW_SUCCESS;
    static bool is_hijacklib_loaded = __init_capsule();

    if(is_hijacklib_loaded){
        GW_CHECK_POINTER(capsule);
        GW_IF_FAILED(
            capsule->start_trace(name, hash, line_position),
            retval,
            {
                throw GWException(
                    "failed to start metric trace capture: name(%s), hash(%lu), line_position(%s)",
                    name.c_str(), hash, line_position.c_str()
                );
            }
        );
        GW_DEBUG(
            "started metric trace capture: name(%s), hash(%lu), line_position(%s)",
            name.c_str(), hash, line_position.c_str()
        );
    }
}


void GW_INTERNAL_stop_app_trace(uint64_t begin_hash, uint64_t end_hash, std::string line_position){
    gw_retval_t retval = GW_SUCCESS;
    static bool is_hijacklib_loaded = __init_capsule();

    if(is_hijacklib_loaded){
        GW_CHECK_POINTER(capsule!= nullptr);
        GW_IF_FAILED(
            capsule->stop_trace(begin_hash, end_hash, line_position),
            retval,
            {
                throw GWException(
                    "failed to stop metric trace capture: begin_hash(%lu), end_hash(%lu), line_position(%s)",
                    begin_hash, end_hash, line_position.c_str()
                );
            }
        );
        GW_DEBUG(
            "stopped metric trace capture: begin_hash(%lu), end_hash(%lu), line_position(%s)",
            begin_hash, end_hash, line_position.c_str()
        );
    }
}


void GW_INTERNAL_push_app_event(GWEvent* app_event){
    static bool is_hijacklib_loaded = __init_capsule();

    if(is_hijacklib_loaded){
        GW_CHECK_POINTER(capsule);
        GW_CHECK_POINTER(app_event);

        capsule->ensure_event_trace();
        GWCapsule::event_trace->push_event((GWEvent*)(app_event));
    }
}


void GW_INTERNAL_push_app_parent_event(GWEvent* app_event){
    static bool is_hijacklib_loaded = __init_capsule();

    if(is_hijacklib_loaded){
        GW_CHECK_POINTER(capsule);
        GW_CHECK_POINTER(app_event);

        capsule->ensure_event_trace();
        GWCapsule::event_trace->push_parent_event((GWEvent*)(app_event));
    }    
}


void GW_INTERNAL_pop_app_parent_event(){
    static bool is_hijacklib_loaded = __init_capsule();
    
    if(is_hijacklib_loaded){
        GW_CHECK_POINTER(capsule);

        capsule->ensure_event_trace();
        GWCapsule::event_trace->pop_parent_event(GW_EVENT_TYPE_APP, (uint64_t)(pthread_self()));
    }
}
