#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <stack>
#include <map>
#include <any>

#include "nlohmann/json.hpp"

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/timer.hpp"
#include "common/utils/lockfree_queue.hpp"


enum gw_event_typeid_t : uint32_t {
    GW_EVENT_TYPE_UNKNOWN = 0,

    GW_EVENT_TYPE_CPU = 100,

    GW_EVENT_TYPE_GPU = 200,

    GW_EVENT_TYPE_APP = 300,

    GW_EVENT_TYPE_GWATCH = 400,
};

constexpr bool gw_event_is_cpu(gw_event_typeid_t id) {
    return id >= GW_EVENT_TYPE_CPU && id < GW_EVENT_TYPE_GPU;
}

constexpr bool gw_event_is_gpu(gw_event_typeid_t id) {
    return id >= GW_EVENT_TYPE_GPU && id < GW_EVENT_TYPE_APP;
}

constexpr bool gw_event_is_app(gw_event_typeid_t id) {
    return id >= GW_EVENT_TYPE_APP && id < GW_EVENT_TYPE_GWATCH;
}

constexpr bool gw_event_is_gwatch(gw_event_typeid_t id) {
    return id >= GW_EVENT_TYPE_GWATCH;
}


/*!
 *  \brief  represent an event occurs during execution
 */
class GWEvent {
    /* ======================== common ======================== */
 public:
    // name of the event
    std::string name = "unknown";

    // id of the event
    uint64_t id = 0;

    // global id of the event (across the world)
    std::string global_id = "";

    // thread id of the event
    uint64_t thread_id = 0;

    // type of the event
    gw_event_typeid_t type_id = GW_EVENT_TYPE_UNKNOWN;


    /*!
     *  \brief  constructor
     */
    GWEvent(std::string name_="unknown") : name(name_){};


    /*!
     *  \brief  deconstructor
     */
    virtual ~GWEvent() = default;


    /*!
     *  \brief  mark the event as archived
     */
    inline void archive() { this->_is_archived = true; }


    /*!
     *  \brief  obtain whether the event is archived
     *  \return whether the event is archived
     */
    inline bool is_archived() const { return this->_is_archived; }


    /*!
     *  \brief  set the parent event of the event
     *  \param  parent parent event of the event
     */
    inline void set_parent(GWEvent* parent){
        GW_CHECK_POINTER(parent);
        this->_has_parent = true;
        this->_parent_id = parent->id;
    }


    /*!
     *  \brief  add a related event
     *  \param  event   related event
     */
    inline void add_related_event(GWEvent* event){
        GW_CHECK_POINTER(event);
        this->_list_related_event_global_idx.push_back(event->global_id);
        event->_list_related_event_global_idx.push_back(this->global_id);
    }


    /*!
     *  \brief  convert event type id to string
     *  \param  id  event type id
     *  \return string representation of the event type id
     */
    static std::string typeid_to_string(gw_event_typeid_t id){
        switch(id){
            case GW_EVENT_TYPE_CPU: return "cpu";
            case GW_EVENT_TYPE_GPU: return "gpu";
            case GW_EVENT_TYPE_APP: return "app";
            case GW_EVENT_TYPE_GWATCH: return "gwatch";
            default: return "unknown";
        }
    }


 protected:
    friend class GWCapsule;

    // related events
    std::vector<std::string> _list_related_event_global_idx;

    // id of the parent event (within the same event trace)
    bool _has_parent = false;
    uint64_t _parent_id = 0;

    // mark whether the event is archived
    volatile bool _is_archived = false;
    /* ======================== common ======================== */


    /* ======================== tick management ======================== */
 public:
    /*!
     *  \brief  record the tick of the event
     *  \param  tick_name name of the tick
     */
    inline void record_tick(std::string tick_name){
        this->_map_ticks.insert({tick_name, GWUtilTscTimer::get_tsc()});
    }


    /*!
     *  \brief  record the tick of the event
     *  \param  tick_name       name of the tick
     *  \param  customized_tick customized tick
     */
    inline void record_tick(std::string tick_name, uint64_t customized_tick){
        this->_map_ticks.insert({tick_name, customized_tick});
    }


    /*!
     *  \brief  get the map of ticks of the event
     *  \return map of ticks of the event
     */
    inline std::map<std::string,uint64_t> get_map_ticks() const { return this->_map_ticks; }


 protected:
    // map of ticks of the event
    std::map<std::string, uint64_t> _map_ticks;
    /* ======================== tick management ======================== */


    /* ======================== metadata management ======================== */
 public:
    /*!
     *  \brief  record the metadata of the event
     *  \param  key   key of the metadata
     *  \param  value value of the metadata
     */
    inline void set_metadata(std::string key, nlohmann::json value){
        this->_list_metadata.push_back({key, value});
    }


    /*!
     *  \brief  get the map of metadata of the event
     *  \return map of metadata of the event
     */
    inline std::vector<std::pair<std::string, nlohmann::json>> get_map_metadata() const { 
        return this->_list_metadata;
    }


 protected:
    // list of metadata of the event
    std::vector<std::pair<std::string, nlohmann::json>> _list_metadata;
    /* ======================== metadata management ======================== */


    /* ======================== JSON serilization ======================== */
 public:
    /*!
     *  \brief  serialize the event
     *  \return serialized string
     */
    nlohmann::json to_json() const;


    /*!
     *  \brief  deserialize the event
     *  \param  json  serialized string
     *  \return GW_SUCCESS if success, otherwise GW_FAILED
     */
    gw_retval_t from_json(const nlohmann::json& json);
    /* ======================== JSON serilization ======================== */
};


/*!
 *  \brief  view of a specific collections of events
 */
class GWEventTraceView {
 public:
    /*!
     *  \brief  constructor
     */
    GWEventTraceView();


    /*!
     *  \brief  deconstructor
     */
    ~GWEventTraceView();


    /*!
     *  \brief  load the event trace view
     *  \param  map_event_trace  map of event trace
     */
    void load(std::map<uint64_t, std::vector<const GWEvent*>> map_event_trace);


    /*!
     *  \brief  serialize the event trace view
     *  \return serialized string
     */
    nlohmann::json to_json() const;


    /*!
     *  \brief  deserialize the event trace view
     *  \param  json  serialized string
     *  \return GW_SUCCESS if success, otherwise GW_FAILED
     */
    gw_retval_t from_json(const nlohmann::json& json);


    /*!
     *  \brief  merge two event trace views
     *  \param  lhs  left hand side operand
     *  \param  rhs  right hand side operand
     *  \return merged event trace view
     */
    friend GWEventTraceView operator+(const GWEventTraceView& lhs, const GWEventTraceView& rhs);


 protected:
    // view of the event trace
    std::map<uint64_t, std::vector<const GWEvent*>> _map_event_trace;


 private:
    /*!
     *  \brief  sort the event trace
     */
    void __sort_event_trace();
};


/*!
 *  \brief  a trace of events, managing event hierarchy and sequence
 */
class GWEventTrace {
 public:
    /*!
     *  \brief  constructor
     *  \param  do_exit  whether to exit the event trace when upstream deconstructor be called
     */
    GWEventTrace(volatile bool do_exit);


    /*!
     *  \brief  deconstructor
     */
    ~GWEventTrace();


    /*!
     *  \brief  set the name of the event trace
     *  \param  name  name of the event trace
     */
    inline void set_name(std::string name){
        this->_name = name;
    }


    /*!
     *  \brief  push an event into the trace
     *  \param  event      the event to be pushed
     *  \param  thread_id  thread id of the event
     *  \return GW_SUCCESS if success, otherwise GW_FAILED
     */
    gw_retval_t push_event(GWEvent* event);
    

    /*!
     *  \brief  pop an event from the trace
     *  \param  event      the event be poped
     *  \param  thread_id  thread id from which the event be poped
     *  \return GW_SUCCESS if success, otherwise GW_FAILED
     */
    gw_retval_t pop_event(GWEvent*& event);


    /*!
     *  \brief  push an parent event into the trace stack
     *  \param  event      the event to be pushed
     *  \param  thread_id  thread id of the event
     *  \return GW_SUCCESS if success, otherwise GW_FAILED
     */
    gw_retval_t push_parent_event(GWEvent* event);


    /*!
     *  \brief  pop an parent event from the trace stack
     *  \param  event       the event be poped
     *  \param  type_id     type id of the event
     *  \param  thread_id   thread id from which the event be poped
     *  \return GW_SUCCESS if success, otherwise GW_FAILED
     */
    gw_retval_t pop_parent_event(GWEvent* event);
    gw_retval_t pop_parent_event(gw_event_typeid_t type_id, uint64_t thread_id);


    /*!
     *  \brief  get the latest parent event from the trace stack
     *  \param  event       the event be poped
     *  \param  type_id     type id of the event
     *  \param  thread_id   thread id from which the event be poped
     *  \return GW_SUCCESS if success, otherwise GW_FAILED
     */
    gw_retval_t get_latest_parent_event(GWEvent*& event, gw_event_typeid_t type_id, uint64_t thread_id=0);


    /*!
     *  \brief  check if the event trace is empty
     *  \return true if empty, otherwise false
     */
    bool is_event_trace_empty();


 private:
    // name of the event trace
    std::string _name = "";

    // whether to exit the event trace when upstream deconstructor be called
    volatile bool _do_exit = false;

    // event trace queue
    GWUtilsLockFreeQueue<GWEvent*>* _event_queue = nullptr;

    // per-type per-thread parent event stack
    std::map<gw_event_typeid_t, std::map<uint64_t, std::stack<GWEvent*>>> _map_parent_events = {};

    // current event id in the trace
    uint64_t _current_event_id = 0;
};
