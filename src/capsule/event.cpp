#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <thread>

#include <pthread.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/event.hpp"
#include "common/utils/lockfree_queue.hpp"


nlohmann::json GWEvent::to_json() const {
    nlohmann::json object;

    object["name"] = this->name;
    object["id"] = this->id;
    object["global_id"] = this->global_id;
    object["list_related_event_global_idx"] = this->_list_related_event_global_idx;
    object["has_parent"] = this->_has_parent;
    object["parent_id"] = this->_parent_id;
    object["type"] = this->type_id;
    object["thread_id"] = this->thread_id;
    object["ticks"] = this->_map_ticks;
    object["metadata"] = this->_list_metadata;

    return object;
}


gw_retval_t GWEvent::from_json(const nlohmann::json& json){
    gw_retval_t retval = GW_SUCCESS;

    try {
        if(json.contains("name"))
            this->name = json["name"];
        
        if(json.contains("id"))
            this->id = json["id"];

        if(json.contains("global_id"))
            this->global_id = json["global_id"];

        if(json.contains("thread_id"))
            this->thread_id = json["thread_id"];

        if(json.contains("type")){
            this->type_id = json["type"];
        }

        if(json.contains("list_related_event_global_idx"))
            this->_list_related_event_global_idx = json["list_related_event_global_idx"];

        if(json.contains("has_parent"))
            this->_has_parent = json["has_parent"];

        if(json.contains("parent_id"))
            this->_parent_id = json["parent_id"];

        if(json.contains("ticks")){
            for(auto &it : json["ticks"].items()){
                this->_map_ticks.insert({it.key(), it.value()});
            }
        }

        if(json.contains("metadata")){
            this->_list_metadata = json["metadata"];
        }
    } catch (const std::exception& e) {
        GW_WARN_C("failed to deserialize the event: %s", e.what());
        retval = GW_FAILED;
        goto exit;
    }

 exit:
    return retval;
}


GWEventTraceView::GWEventTraceView(){
}


GWEventTraceView::~GWEventTraceView(){
}


void GWEventTraceView::load(std::map<uint64_t, std::vector<const GWEvent*>> map_event_trace){
    this->_map_event_trace = map_event_trace;
    this->__sort_event_trace();
}


nlohmann::json GWEventTraceView::to_json() const {
    std::map<uint64_t, std::vector<nlohmann::json>> tmp_map_event_trace;
    for(auto &it : this->_map_event_trace){
        for(auto &event : it.second){
            tmp_map_event_trace[it.first].push_back(event->to_json());
        }
    }
    return tmp_map_event_trace;
}


gw_retval_t GWEventTraceView::from_json(const nlohmann::json& json){
    gw_retval_t retval = GW_SUCCESS;
    GWEvent* event;
    std::map<uint64_t, std::vector<nlohmann::json>> tmp_map_event_trace;

    tmp_map_event_trace = json.get<std::map<uint64_t, std::vector<nlohmann::json>>>();
    for(auto &it : tmp_map_event_trace){
        for(auto &event_json : it.second){
            GW_CHECK_POINTER(event = new GWEvent());
            event->from_json(event_json);
            this->_map_event_trace[it.first].push_back(event);
        }
    }

    return retval;
}


GWEventTraceView operator+(const GWEventTraceView& lhs, const GWEventTraceView& rhs) {
    GWEventTraceView result;

    // merge left
    for (const auto& [key, value] : lhs._map_event_trace) {
        result._map_event_trace[key] = value;
    }

    // merge right
    for (const auto& [key, value] : rhs._map_event_trace) {
        if (result._map_event_trace.find(key) != result._map_event_trace.end()) {
            result._map_event_trace[key].insert(result._map_event_trace[key].end(), value.begin(), value.end());
        } else {
            result._map_event_trace[key] = value;
        }
    }

    // sort the result
    result.__sort_event_trace();

    return result;
}


void GWEventTraceView::__sort_event_trace(){
    auto __sort_event = [](const GWEvent* a, const GWEvent* b) -> bool {
        std::map<std::string,uint64_t> map_ticks_a, map_ticks_b;
        if(a == nullptr || b == nullptr) return false;
        return  a->get_map_ticks()["begin"] < b->get_map_ticks()["begin"];
    };
    for(auto &it : this->_map_event_trace){
        std::sort(it.second.begin(), it.second.end(), __sort_event);
    }
}


GWEventTrace::GWEventTrace(volatile bool do_exit) : _do_exit(do_exit) {
    this->_event_queue = new GWUtilsLockFreeQueue<GWEvent*>(
        [](GWEvent *event) -> gw_retval_t {
            if(event != nullptr) delete event;
            return GW_SUCCESS;
        }
    );
    GW_CHECK_POINTER(this->_event_queue);
}


GWEventTrace::~GWEventTrace()
{}


gw_retval_t GWEventTrace::push_event(GWEvent* event){
    gw_retval_t retval = GW_SUCCESS, tmp_retval = GW_SUCCESS;
    GWUtilsLockFreeQueue<GWEvent*> *new_queue;
    GWEvent *parent_event = nullptr;

    GW_CHECK_POINTER(event);

    // make sure the queue exist
    GW_CHECK_POINTER(this->_event_queue);

    // set id and global id of the event
    event->id = this->_current_event_id;
    this->_current_event_id += 1;

    // event global index: <trace_name>-<type_name>-<thread_id>--<id>
    event->global_id =  this->_name 
                        + "-" + GWEvent::typeid_to_string(event->type_id)
                        + "-" + std::to_string(event->thread_id)
                        + "-" + std::to_string(event->id);

    // push to event trace
    this->_event_queue->push(event);

    // try set parent event (if exist)
    tmp_retval = this->get_latest_parent_event(parent_event, event->type_id, event->thread_id);
    if(tmp_retval == GW_SUCCESS){
        GW_CHECK_POINTER(parent_event);
        event->set_parent(parent_event);
    }

    return retval;
}


gw_retval_t GWEventTrace::pop_event(GWEvent*& event){
    return this->_event_queue->dequeue(event);
}


gw_retval_t GWEventTrace::push_parent_event(GWEvent* event){
    gw_retval_t retval = GW_SUCCESS;
    
    GW_CHECK_POINTER(event);
    this->_map_parent_events[event->type_id][event->thread_id].push(event);

    return retval;
}


gw_retval_t GWEventTrace::pop_parent_event(GWEvent* event){
    return this->pop_parent_event(event->type_id, event->thread_id);
}


gw_retval_t GWEventTrace::pop_parent_event(gw_event_typeid_t type_id, uint64_t thread_id){
    gw_retval_t retval = GW_SUCCESS;

    if(unlikely(this->_map_parent_events.count(type_id) == 0)){
        GW_WARN_C("failed to pop parent event, no parent event for type_id: type_id(%d)", type_id);
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }

    if(unlikely(this->_map_parent_events[type_id].count(thread_id) == 0)){
        GW_WARN_C(
            "failed to pop parent event, no parent event for thread: type_id(%d), thread_id(%lu)",
            type_id, thread_id
        );
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }

    if(!this->_map_parent_events[type_id][thread_id].empty()){
        this->_map_parent_events[type_id][thread_id].pop();
    } else {
        GW_WARN_C(
            "failed to pop parent event, stack is empty for thread: type_id(%d), thread_id(%lu)",
            type_id, thread_id
        );
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }

exit:
    return retval;
}


gw_retval_t GWEventTrace::get_latest_parent_event(GWEvent*& event, gw_event_typeid_t type_id, uint64_t thread_id){
    gw_retval_t retval = GW_SUCCESS;

    if(unlikely(this->_map_parent_events.count(type_id) == 0)){
        event = nullptr;
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }

    if(unlikely(this->_map_parent_events[type_id].count(thread_id) == 0)){
        event = nullptr;
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }

    if(unlikely(this->_map_parent_events[type_id][thread_id].empty())){
        event = nullptr;
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }

    event = this->_map_parent_events[type_id][thread_id].top();

exit:
    return retval;
}


bool GWEventTrace::is_event_trace_empty(){
    return this->_event_queue->len() == 0;
}
