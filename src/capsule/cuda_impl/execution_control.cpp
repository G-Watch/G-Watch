#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <set>
#include <map>
#include <queue>
#include <atomic>

#include <pthread.h>

#include <cuda.h>
#include <cupti_checkpoint.h>

#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/trace.hpp"
#include "common/utils/cuda.hpp"
#include "common/assemble/kernel.hpp"
#include "common/assemble/kernel_def.hpp"
#include "common/cuda_impl/assemble/kernel_cuda.hpp"
#include "common/cuda_impl/assemble/kernel_def_sass.hpp"
#include "capsule/capsule.hpp"
#include "capsule/hijack/cuda_impl/runtime.hpp"
#include "scheduler/serve/capsule_message.hpp"


gw_retval_t GWCapsule::CUDA_trace_single_kernel(
    CUfunction function,
    std::string function_name,
    uint32_t grid_dim_x,
    uint32_t grid_dim_y,
    uint32_t grid_dim_z,
    uint32_t block_dim_x,
    uint32_t block_dim_y,
    uint32_t block_dim_z,
    uint32_t shared_mem_bytes,
    CUstream stream,
    void** params,
    void** extra,
    CUlaunchAttribute* attrs,
    uint32_t num_attrs
){
    gw_retval_t retval = GW_SUCCESS;
    static thread_local uint64_t trace_id = 0;
    std::string trace_global_id = "";
    CUcontext cu_context = (CUcontext)0;
    CUmodule cu_module = (CUmodule)0;
    GWKernel *kernel = nullptr;
    GWKernelExt_CUDA *kernel_ext_cuda = nullptr;
    GWKernelDef *kernel_def_sass = nullptr;
    GWEvent *trace_event = nullptr;
    uint64_t cpu_thread_id = 0;
    std::map<std::string, GWInstrumentCxt*> map_existing_instrument_ctx;
    std::map<std::string, GWInstrumentCxt*> map_current_instrument_ctx;
    std::map<std::string, GWInstrumentCxt*> map_new_instrument_ctx;
    std::vector<std::string> list_current_instrument_cxt_global_id;

    auto __report_trace_to_scheduler = [&](
        std::string _trace_global_id, std::string _trace_type, nlohmann::json&& _trace_task_serialize, \
        std::vector<std::string> _list_child_trace_global_id = {}
    ){
        gw_retval_t _retval = GW_SUCCESS;
        GWInternalMessage_Capsule *_capsule_message_sql = nullptr, *_capsule_message_kv = nullptr;
        GWInternalMessagePayload_Common_DB_SQL_Write *_sql_write_payload = nullptr;
        GWInternalMessagePayload_Common_DB_KV_Write *_kv_write_payload = nullptr;

        // record trace id to SQL database
        GW_CHECK_POINTER(_capsule_message_sql = new GWInternalMessage_Capsule());
        _sql_write_payload = _capsule_message_sql->get_payload_ptr<GWInternalMessagePayload_Common_DB_SQL_Write>(
            GW_MESSAGE_TYPEID_COMMON_SQL_WRITE_DB
        );
        GW_CHECK_POINTER(_sql_write_payload);
        _capsule_message_sql->type_id = GW_MESSAGE_TYPEID_COMMON_SQL_WRITE_DB;
        _sql_write_payload->table_name = "mgnt_trace";
        _sql_write_payload->insert_data = {
            { "global_id", _trace_global_id },
            { "target", function_name },
            { "type", _trace_type }
        };
        _retval = this->send_to_scheduler(_capsule_message_sql);
        if(_retval == GW_SUCCESS){
            GW_DEBUG("sent trace task (SQL) to scheduler: trace_global_id(%s)", _trace_global_id.c_str());
        }

        // record child trace id to SQL database
        for(auto& _child_trace_global_id : _list_child_trace_global_id){
            _sql_write_payload->table_name = "mgnt_trace_childtrace";
            _sql_write_payload->insert_data = {
                { "global_id", _trace_global_id },
                { "child_global_id", _child_trace_global_id }
            };
            _retval = this->send_to_scheduler(_capsule_message_sql);
            if(_retval == GW_SUCCESS){
                GW_DEBUG("sent parent-child trace task relationship (SQL) to scheduler: trace_global_id(%s)", _trace_global_id.c_str());
            }
        }

        // write trace result to kv database
        GW_CHECK_POINTER(_capsule_message_kv = new GWInternalMessage_Capsule());
        _kv_write_payload
            = _capsule_message_kv->get_payload_ptr<GWInternalMessagePayload_Common_DB_KV_Write>(
                GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB
            );
        GW_CHECK_POINTER(_kv_write_payload);
        _capsule_message_kv->type_id = GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB;
        _kv_write_payload->uri = std::format("/trace/{}", _trace_global_id);
        _kv_write_payload->write_payload = _trace_task_serialize;
        _retval = this->send_to_scheduler(_capsule_message_kv);
        if(_retval == GW_SUCCESS){
            GW_DEBUG("sent trace task (KV) to scheduler: trace_global_id(%s)", _trace_global_id.c_str());
        }

     exit:
        if(_capsule_message_sql != nullptr)
            delete _capsule_message_sql;
        if(_capsule_message_kv != nullptr)
            delete _capsule_message_kv;
        return _retval;
    };

    // get current CPU thread index
    cpu_thread_id = (uint64_t)(pthread_self());

    // obtain current context
    GW_IF_FAILED(
        GWUtilCUDA::get_current_cucontext(cu_context),
        retval,
        goto exit;
    );
    GW_ASSERT(cu_context != (CUcontext)0);

    // find the CUmodule that contain this CUfunction
    this->_mutex_module_management.lock();
    if(unlikely(this->_map_cufunction_cumodule[cu_context].count(function) == 0)){
        GW_WARN_C("failed to trace function, no mapped cumodule found: func(%p)", function);
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }
    cu_module = this->_map_cufunction_cumodule[cu_context][function];

    // parse the cubin of the CUmodule (if needed)
    if(unlikely(this->_map_cumodule_cubin[cu_context].count(cu_module) == 0)){
        this->_mutex_module_management.unlock();
        GW_IF_FAILED(
            this->CUDA_parse_cufunction(function, cu_module, /* do_parse_entire_binary */ false),
            retval,
            {
                GW_WARN_DETAIL("failed to trace function, failed to parse binary: %s", gw_retval_str(retval));
                goto exit;
            }
        );
        this->_mutex_module_management.lock();
        GW_ASSERT(this->_map_cumodule_cubin[cu_context].count(cu_module) > 0);
    }
    this->_mutex_module_management.unlock();

    // obtain kernel definition
    this->_mutex_module_management.lock();
    if(unlikely(this->_map_cufunction_kerneldef[cu_context].count(function) == 0)){
        GW_WARN_C("failed to trace function, no kernel definition found: func(%p)", function);
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }
    GW_CHECK_POINTER(kernel_def_sass = this->_map_cufunction_kerneldef[cu_context][function]);
    this->_mutex_module_management.unlock();

    // instantiate kernel
    GW_CHECK_POINTER(kernel_ext_cuda = GWKernelExt_CUDA::create(kernel_def_sass));
    kernel = kernel_ext_cuda->get_base_ptr();
    kernel->grid_dim_x = grid_dim_x;
    kernel->grid_dim_y = grid_dim_y;
    kernel->grid_dim_z = grid_dim_z;
    kernel->block_dim_x = block_dim_x;
    kernel->block_dim_y = block_dim_y;
    kernel->block_dim_z = block_dim_z;
    kernel->shared_mem_bytes = shared_mem_bytes;
    kernel->stream = (uint64_t)(stream);
    kernel_ext_cuda->params().params = params;
    kernel_ext_cuda->params().extra = extra;
    kernel_ext_cuda->params().attrs = attrs;
    kernel_ext_cuda->params().num_attrs = num_attrs;

    // parse register liveness of the kernel definition
    GW_IF_FAILED(
        kernel_def_sass->parse_register_liveness(),
        retval,
        {
            GW_WARN_C(
                "failed to start tracing due to failed to parse register livesss: error(%s)",
                gw_retval_str(retval)
            );
            goto exit;
        }
    );

    // execute all trace
    for(auto& trace_task : this->_list_trace_task_kernel){
        // check whether need to trace this kernel
        if(!trace_task->do_need_trace(function_name)){
            continue;
        }

        // generate global_id of the trace
        trace_global_id = std::format(
            "{}-thread-{}-kernel-{}-trace-{}",
            this->global_id, cpu_thread_id, (uint64_t)(function), trace_id
        );
        trace_id += 1;

        // create trace event
        GW_CHECK_POINTER(
            trace_event = new GWEvent(std::format("[Trace] {}", trace_task->get_type()))
        );
        trace_event->thread_id = cpu_thread_id;
        trace_event->type_id = GW_EVENT_TYPE_GWATCH;
        trace_event->set_metadata("payload_type", "trace_single_kernel");
        trace_event->set_metadata("Trace UUID", trace_global_id);
        trace_event->set_metadata("Trace Application", trace_task->get_type());

        this->ensure_event_trace();
        GWCapsule::event_trace->push_event(trace_event);
        GWCapsule::event_trace->push_parent_event(trace_event);
        trace_event->record_tick("begin");

        // execute trace task
        map_current_instrument_ctx.clear();
        GW_IF_FAILED(
            trace_task->execute(
                /* capsule */ this,
                /* global_id */ trace_global_id,
                /* kernel */ kernel,
                /* map_existing_instrument_ctx */ map_existing_instrument_ctx,
                /* map_current_instrument_ctx */ map_current_instrument_ctx
            ),
            retval,
            {
                GW_WARN_C(
                    "failed to execute trace task: kernel(%s), error(%s)",
                    function_name.c_str(), gw_retval_str(retval)
                );
            }
        );

        // archive trace event
        trace_event->record_tick("end");
        GWCapsule::event_trace->pop_parent_event(trace_event);
        trace_event->archive();

        // merge new instrument context to avoid duplicate trace
        map_new_instrument_ctx.clear();
        for (auto& [key, ptr] : map_current_instrument_ctx) {
            if(map_existing_instrument_ctx.find(key) == map_existing_instrument_ctx.end()){
                map_existing_instrument_ctx[key] = ptr;
                map_new_instrument_ctx[key] = ptr;
            }
        }

        // report trace task
        for(auto& [key, ptr] : map_current_instrument_ctx){
            list_current_instrument_cxt_global_id.push_back(ptr->global_id);
        }
        GW_IF_FAILED(
            __report_trace_to_scheduler(
                /* trace_global_id */ trace_global_id, 
                /* trace_type */ trace_task->get_type(),
                /* trace_result */ trace_task->serialize(
                    /* global_id */ trace_global_id,
                    /* map_current_instrument_ctx */ map_current_instrument_ctx
                ),
                /* list_child_trace_global_id */ list_current_instrument_cxt_global_id
            ),
            retval,
            {
                GW_WARN_C(
                    "failed to report trace task: trace_global_id(%s), error(%s)",
                    trace_global_id.c_str(), gw_retval_str(retval)
                );
            }
        );

        // report child trace (instrument context)
        for(auto& [key, ptr] : map_new_instrument_ctx){
            GW_IF_FAILED(
                __report_trace_to_scheduler(
                    /* trace_global_id */ ptr->global_id,
                    /* trace_type */ ptr->get_type(),
                    /* trace_result */ ptr->serialize(),
                    /* list_child_trace_global_id */ {}
                ),
                retval,
                {
                    GW_WARN_C(
                        "failed to report child trace (instrument context): trace_global_id(%s), error(%s)",
                        ptr->global_id.c_str(), gw_retval_str(retval)
                    );
                }
            );
        }
    }

exit:
    return retval;
}


gw_retval_t GWCapsule::CUDA_checkpoint(
    gw_capsule_cuda_state_t &state, bool do_push
){
    using namespace NV::Cupti::Checkpoint;
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval = 0;
    CUresult cudv_retval = CUDA_SUCCESS;
    #if GWATCH_PRINT_DEBUG
        uint64_t s_tick = 0, e_tick = 0;
    #endif

    std::lock_guard<std::mutex> lock_guard(this->_mutex_checkpoints);
    
    GW_IF_CUDA_DRIVER_FAILED(
        cuCtxGetDevice(&state.cu_device),
        cudv_retval,
        {
            GW_WARN_C("failed to obtain cuda device before checkpoint: device_id(current)");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

    GW_IF_CUDA_DRIVER_FAILED(
        cuCtxGetCurrent(&state.cu_context),
        cudv_retval,
        {
            GW_WARN_C("failed to obtain cuda context before checkpoint: device_id(current)");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

    GW_CHECK_POINTER(state.cupti_ckpt = new CUpti_Checkpoint);
    memset(state.cupti_ckpt, 0, sizeof(CUpti_Checkpoint));

    state.cupti_ckpt->structSize = CUpti_Checkpoint_STRUCT_SIZE;
    GW_CHECK_POINTER(state.cupti_ckpt->ctx = state.cu_context);
    state.cupti_ckpt->allowOverwrite = 0;
    state.cupti_ckpt->optimizations = CUPTI_CHECKPOINT_OPT_NONE;

    #if GWATCH_PRINT_DEBUG
        s_tick = GWUtilTscTimer::get_tsc();
    #endif
    GW_IF_CUPTI_FAILED(
        cuptiCheckpointSave(state.cupti_ckpt),
        sdk_retval,
        {
            GW_WARN_C("failed to checkpoint CUDA state");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );
    #if GWATCH_PRINT_DEBUG
        e_tick = GWUtilTscTimer::get_tsc();
    #endif

    GW_DEBUG_C(
        "checkpoint CUDA state: "
        "duration(%lf ms), device(%d), context(%p)",
        this->_tsc_timer.tick_to_ms(e_tick - s_tick),
        state.cu_device,
        state.cu_context
    );

    if(do_push){
        if(unlikely(this->_map_cuda_states.count(state.cu_context) == 0)){
            this->_map_cuda_states[state.cu_context] = std::stack<gw_capsule_cuda_state_t>();
        }
        this->_map_cuda_states[state.cu_context].push(state);
    }

 exit:
    return retval;
}


gw_retval_t GWCapsule::CUDA_restore_from_latest(bool do_pop){
    using namespace NV::Cupti::Checkpoint;
    gw_retval_t retval = GW_SUCCESS;
    CUresult cudv_retval = CUDA_SUCCESS;
    gw_capsule_cuda_state_t state;
    int sdk_retval = 0;

    GW_IF_CUDA_DRIVER_FAILED(
        cuCtxGetDevice(&state.cu_device),
        cudv_retval,
        {
            GW_WARN_C("failed to obtain cuda device before restore: device_id(current)");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

    GW_IF_CUDA_DRIVER_FAILED(
        cuCtxGetCurrent(&state.cu_context),
        cudv_retval,
        {
            GW_WARN_C("failed to obtain cuda context before restore: device_id(current)");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

    if(unlikely(
        this->_map_cuda_states.size() == 0 
        || this->_map_cuda_states.count(state.cu_context) == 0
        || this->_map_cuda_states[state.cu_context].size() == 0
    )){
        GW_WARN_C("no cuda state to restore");
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }

    state = this->_map_cuda_states[state.cu_context].top();
    GW_CHECK_POINTER(state.cupti_ckpt);

    GW_IF_FAILED(
        this->CUDA_restore(state),
        retval,
        goto exit;
    );

    if(do_pop){        
        GW_IF_CUPTI_FAILED(
            cuptiCheckpointFree(state.cupti_ckpt),
            sdk_retval, 
            {
                GW_WARN_C("failed to free the checkpoint");
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );
        this->_map_cuda_states[state.cu_context].pop();
    }

exit:
    return retval;
}


gw_retval_t GWCapsule::CUDA_restore(gw_capsule_cuda_state_t &state){
    using namespace NV::Cupti::Checkpoint;
    int sdk_retval = 0;
    gw_retval_t retval = GW_SUCCESS;
    #if GWATCH_PRINT_DEBUG
        uint64_t s_tick = 0, e_tick = 0;
    #endif

    GW_CHECK_POINTER(state.cupti_ckpt);

    #if GWATCH_PRINT_DEBUG
        s_tick = GWUtilTscTimer::get_tsc();
    #endif
    GW_IF_CUPTI_FAILED(
        cuptiCheckpointRestore(state.cupti_ckpt),
        sdk_retval,
        {
            GW_WARN_C("failed to restore CUDA state");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );
    #if GWATCH_PRINT_DEBUG
        e_tick = GWUtilTscTimer::get_tsc();
    #endif

    GW_DEBUG_C(
        "restored CUDA state: "
        "duration(%lf ms), device(%d), context(%p)",
        this->_tsc_timer.tick_to_ms(e_tick - s_tick),
        state.cu_device,
        state.cu_context
    );

exit:
    return retval;
}
