#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <any>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/assemble/kernel.hpp"
#include "capsule/capsule.hpp"
#include "capsule/event.hpp"


// global capsule instance within binding lib
extern GWCapsule *capsule;


/*!
 *  \brief  initialize capsule point to point to the instance in
 *          libgwatch_capsule_hijack.so
 *  \return true if libgwatch_capsule_hijack.so is loaded
 */
bool __init_capsule();


/*!
 *  \brief  start capture kernel launch
 */
void GW_INTERNAL_start_capture_kernel_launch();


/*!
 *  \brief  stop capture kernel launch
 *  \return list of captured kernels
 */
std::vector<GWKernel> GW_INTERNAL_stop_capture_kernel_launch();


// NOTE(zhuobin): below are legacy APIs, should be deprecated in the future
/*!
 *  \brief  create trace task
 *  \param  type    trace application of the trace task
 */
GWTraceTask* GW_INTERNAL_create_trace_task(std::string type);


/*!
 *  \brief  destory trace task
 *  \param  trace_task  trace task to be destoryed
 */
void GW_INTERNAL_destory_trace_task(GWTraceTask* trace_task);


/*!
 *  \brief  register kernel trace task
 *  \param  trace_task  trace task to be registered
 */
void GW_INTERNAL_register_trace_task(GWTraceTask* trace_task);


/*!
 *  \brief  unregister kernel trace task
 *  \param  trace_task  trace task to be unregistered
 */
void GW_INTERNAL_unregister_trace_task(GWTraceTask* trace_task);


/*!
 *  \brief  start app metric trace capture
 *  \param  name            name of the metric
 *  \param  hash            hash of the metric
 *  \param  line_position   line position of the metric
 */
void GW_INTERNAL_start_app_trace(
    std::string name, uint64_t hash, std::string line_position=""
);


/*!
 *  \brief  end app metric latency
 *  \param  begin_hash      begin hash of the metric to be capture stopped
 *  \param  end_hash        end hash of the metric
 *  \param  line_position   line position of the metric
 */
void GW_INTERNAL_stop_app_trace(
    uint64_t begin_hash, uint64_t end_hash, std::string line_position=""
);


/*!
 *  \brief  push app range event
 *  \param  app_event       pointer to the app range event
 */
void GW_INTERNAL_push_app_event(GWEvent* app_event);


/*!
 *  \brief  push app range parent event
 *  \param  app_event       pointer to the app range event
 */
void GW_INTERNAL_push_app_parent_event(GWEvent* app_event);


/*!
 *  \brief  pop app range parent event
 */
void GW_INTERNAL_pop_app_parent_event();
