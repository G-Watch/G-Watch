#pragma once

#include <iostream>
#include <string>

#include <stdint.h>

#include "gwatch/common.hpp"


namespace gwatch {


// forward declaration
class TraceContext;


/*!
 *  \brief  trace event
 */
class TraceEvent {
 public:
    TraceEvent(std::string name);
    ~TraceEvent();


    /*!
     *  \brief  record a tick in the trace event
     *  \param  tick_name  the name of the tick
     *  \return  the error code of the operation
     */
    gwError RecordTick(std::string tick_name);


    /*!
     *  \brief  set the metadata of the trace event
     *  \param  key  the key of the metadata
     *  \param  value  the value of the metadata
     *  \return  the error code of the operation
     */
    gwError SetMetadata(std::string key, std::string value);


    /*!
     *  \brief  archive the trace event
     */
    void Archive();


    /*!
     *  \brief  check if the trace event is archived
     *  \return  true if the trace event is archived, false otherwise
     */
    bool IsArchived();


 private:
    friend class TraceContext;
    void *_gw_event_handle = nullptr;
};


/*!
 *  \brief  trace task descriptor
 */
class TraceTask {
 public:
    TraceTask(std::string type);
    ~TraceTask();

    gwError GetMetadata(std::string key, std::string &value);
    gwError SetMetadata(std::string key, std::string value);
    bool HasMetadata(std::string key);

 private:
    friend class TraceContext;
    void *_gw_trace_task_handle = nullptr;
};


/*!
 *  \brief  the trace context
 */
class TraceContext {
 public:
    /*!
     *  \brief  get the instance of the trace context
     *  \return  the instance of the trace context
     */
    static TraceContext& GetInstance() {
        static TraceContext instance;
        return instance;
    }


    /*!
     *  \brief  delete the copy constructor of the trace context
     */
    TraceContext(const TraceContext&) = delete;
    TraceContext& operator=(const TraceContext&) = delete;


    /*!
     *  \brief  start tracing a metric trace capture
     *  \param  name  the name of the metric trace capture
     *  \param  hash  the hash of the metric trace capture
     *  \param  line_position  the line position of the metric trace capture
     *  \return  the error code of the operation
     */
    gwError StartTracing(std::string name, uint64_t hash, std::string line_position);


    /*!
     *  \brief  stop tracing a metric trace capture
     *  \param  begin_hash  the begin hash of the metric trace capture
     *  \param  end_hash  the end hash of the metric trace capture
     *  \param  line_position  the line position of the metric trace capture
     *  \return  the error code of the operation
     */
    gwError StopTracing(uint64_t begin_hash, uint64_t end_hash, std::string line_position="");


    /*!
     *  \brief  push an event to the trace context
     *  \param  event  the event to be pushed
     *  \return  the error code of the operation
     */
    gwError PushEvent(TraceEvent *event);


    /*!
     *  \brief  push a parent event to the trace context
     *  \param  event  the parent event to be pushed
     *  \return  the error code of the operation
     */
    gwError PushParentEvent(TraceEvent *event);


    /*!
     *  \brief  pop the last parent event from the trace context
     *  \return  the error code of the operation
     */
    gwError PopParentEvent();


    /*!
     *  \brief  register a trace task to the trace context
     *  \param  task  the trace task to be registered
     *  \return  the error code of the operation
     */
    gwError RegisterTraceTask(TraceTask *task);
    
    
    /*!
     *  \brief  unregister a trace task from the trace context
     *  \param  task  the trace task to be unregistered
     *  \return  the error code of the operation
     */
    gwError UnregisterTraceTask(TraceTask *task);

 private:
    TraceContext();
    ~TraceContext();
};


} // namespace gwatch
