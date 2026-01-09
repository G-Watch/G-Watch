#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <set>
#include <map>
#include <queue>
#include <atomic>
#include <format>
#include <string>

#include <libwebsockets.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/binary.hpp"
#include "common/assemble/kernel_def.hpp"
#include "common/utils/timer.hpp"
#include "common/utils/socket.hpp"
#include "common/utils/queue.hpp"
#include "common/cuda_impl/binary/utils.hpp"
#include "capsule/event.hpp"
#include "capsule/metric.hpp"
#include "capsule/trace.hpp"
#include "profiler/context.hpp"
#include "scheduler/serve/capsule_message.hpp"


#ifdef GW_BACKEND_CUDA
    #include <cuda.h>
    #include <cupti_checkpoint.h>
    #include "profiler/cuda_impl/context.hpp"
    #include "profiler/cuda_impl/profiler.hpp"
#endif


/*!
 *  \brief  capsule for executing profiling according to a specific plan
 */
class GWCapsule {
    /* ==================== Common ==================== */
 public:
    /*!
    *  \brief  constructor
    */
    GWCapsule();


    /*!
    *  \brief  destructor
    */
    ~GWCapsule();


    /*!
     *  \brief  state of the capsule
     */
    enum gw_capsule_state_t : uint8_t {
        GW_CAPSULE_STATE_INIT = 0,
        GW_CAPSULE_STATE_RUNNING,
        GW_CAPSULE_STATE_BLOCKED
    };


    /*!
     *  \brief  convert state to string
     *  \param  state   state to be converted
     *  \return string representation of the state
     */
    static std::string state_to_string(gw_capsule_state_t state){
        switch (state)
        {
        case GW_CAPSULE_STATE_INIT:
            return "init";
        case GW_CAPSULE_STATE_RUNNING:
            return "running";
        case GW_CAPSULE_STATE_BLOCKED:
            return "blocked";
        default:
            return "unknown";
        }
    }


    /*!
     *  \brief  convert state from string
     *  \param  state_str   state string to be converted
     *  \return state in enum type
     */
    static gw_capsule_state_t state_from_string(std::string state_str){
        if (state_str == "init") {
            return GW_CAPSULE_STATE_INIT;
        } else if (state_str == "running") {
            return GW_CAPSULE_STATE_RUNNING;
        } else if (state_str == "blocked") {
            return GW_CAPSULE_STATE_BLOCKED;
        } else {
            return GW_CAPSULE_STATE_INIT;
        }
    }


    /*!
     *  \brief  get the path of the log file / dir
     *  \return path of the log file / dir
     */
    std::string get_log_file_path() const { return this->_log_file_path; }
    std::string get_log_file_dir() const { return this->_log_file_dir; }

    // global index of the capsule
    std::string global_id = "";

    // state
    gw_capsule_state_t state = GW_CAPSULE_STATE_INIT;


 private:
    // path of the log file
    std::string _log_file_dir = "";
    std::string _log_file_path = "";

    // TSC timer
    GWUtilTscTimer _tsc_timer;

    // library handles
    void *_lib_gwatch_instrunmentation = nullptr;
    /* ==================== Common ==================== */


    /* ======================== Topology Detection ======================== */
    /*!
     *  \brief  detect the OS info of the node
     *  \param  capsule_info   the obtained os info of the node
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t __detect_capsule(gw_capsule_info_t &capsule_info);


    /*!
     *  \brief  detect the host topology of the node
     *  \param  cpu_info   the obtained host info of the node
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t __detect_cpu_topology(gw_capsule_cpu_info_t &cpu_info);


    /*!
     *  \brief  detect the gpu topology of the node
     *  \param  list_gpu_info   the obtained gpu info of the node
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t __detect_gpu_topology(std::vector<gw_capsule_gpu_info_t> &list_gpu_info);
    /* ======================== Topology Detection ======================== */


    /* ======================== Trace Management ======================== */
 public:
    /*!
     *  \brief  register a kernel trace task
     *  \param  trace_task_def  definition of the trace task
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t register_trace_task(GWTraceTask* trace_task);


    /*!
     *  \brief  unregister a kernel trace task
     *  \param  trace_task_def  definition of the trace task
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t unregister_trace_task(GWTraceTask* trace_task);


    /*!
     *  \brief  check whether the capsule need to trace a kernel with specific kernel name
     *  \param  kernel_name  kernel name
     *  \return true if has, false otherwise
     */
    bool do_need_trace_kernel(std::string kernel_name);


 private:
    // list of trace task (per thread)
    static thread_local std::vector<GWTraceTask*>   _list_trace_task;
    static thread_local std::vector<GWTraceTask*>   _list_trace_task_kernel;
    /* ======================== Trace Management ======================== */


    /* ======================== Event Management ======================== */
 public:
    /*!
     *  \brief  obtain the status of whether the capsule is capturing trace
     *  \return the status of whether the capsule is capturing trace
     */
    bool is_capturing(){
        return this->_trace_counter.load(std::memory_order_relaxed) > 0;
    }


    /*!
     *  \brief  start to capture a metric trace
     *  \param  name            name of the metric
     *  \param  begin_hash      hash value of the start of the metric
     *  \param  line_position   line position of the metric start position
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t start_trace(std::string name, uint64_t begin_hash, std::string line_position="");


    /*!
     *  \brief  stop to capture a metric trace
     *  \param  begin_hash      begin hash to find the metric trace
     *  \param  end_hash        hash value of the end of the metric
     *  \param  line_position   line position of the metric end position
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t stop_trace(uint64_t begin_hash, uint64_t end_hash, std::string line_position="");


    /*!
     *  \brief  obtain metric trace by given begin hash
     *  \param  begin_hash      begin hash to find the metric trace
     *  \param  metric_trace    the obtained metric trace
     *  \return GW_SUCCESS if success
     */
    inline gw_retval_t get_metric_trace_by_begin_hash(uint64_t begin_hash, const GWAppMetricTrace*& metric_trace){
        gw_retval_t retval = GW_SUCCESS;

        if(unlikely(this->_map_begin_hash_to_app_trace.count(begin_hash) == 0)){
            retval = GW_FAILED_NOT_EXIST;
            goto exit;
        }
        metric_trace = this->_map_begin_hash_to_app_trace[begin_hash];

    exit:
        return retval;
    }


    /*!
     *  \brief  obtain metric trace by given end hash
     *  \param  end_hash        end hash to find the metric trace
     *  \param  metric_trace    the obtained metric trace
     *  \return GW_SUCCESS if success
     */
    inline gw_retval_t get_metric_trace_by_end_hash(uint64_t end_hash, const GWAppMetricTrace*& metric_trace){
        gw_retval_t retval = GW_SUCCESS;

        if(unlikely(this->_map_end_hash_to_metric_trace.count(end_hash) == 0)){
            retval = GW_FAILED_NOT_EXIST;
            goto exit;
        }
        metric_trace = this->_map_end_hash_to_metric_trace[end_hash];

    exit:
        return retval;
    }


    /*!
     *  \brief  ensure the event trace and the thread for report to scheduler is created
     */
    void ensure_event_trace();


    // trace of events (cpu/gpu/app)
    static thread_local GWEventTrace *event_trace;

 private:
    /*!
     *  \brief  thread function to report to scheduler
     *  \param  _this           the capsule instance
     *  \param  event_trace     the event trace to report
     *  \param  linux_thread_id the linux thread id of the report thread
     */
    static void __event_report_func(
        GWCapsule* _this, GWEventTrace *event_trace, uint64_t linux_thread_id
    );

    // thread to report trace event to scheduler
    static thread_local std::thread* _event_report_thread;

    // map of event report thread: <thread_id, thread_handle>
    std::mutex _mutex_map_event_report_thread;
    std::map<uint64_t, std::thread*> _map_event_report_thread;

    // map of metric trace
    std::map<uint64_t, GWAppMetricTrace*> _map_begin_hash_to_app_trace;
    std::map<uint64_t, GWAppMetricTrace*> _map_end_hash_to_metric_trace;

    // capture ref counter (only during capture phase we will record cpu/gpu events)
    std::atomic<uint64_t> _trace_counter = 1;
    /* ======================== Event Management ======================== */


    /* ======================== Communication w/ Scheduler ======================== */
 public:
    /*!
     *  \brief  send message to the scheduler
     *  \return GW_SUCCESS if success,
     *          GW_FAILED_NOT_READY for no websocket connection is built
     */
    gw_retval_t send_to_scheduler(GWInternalMessage_Capsule *message);


    /*!
     *  \brief  sync send message to the scheduler
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t sync_send_to_scheduler();


 private:
    /*!
     *  \brief  process the capsule message: REPORT_TOPO
     *  \param  message     the received message
     */
    void __process_capsule_resp_REGISTER(GWInternalMessage_Capsule *message);


    /*!
     *  \brief  start the websocket daemon
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t __start_websocket_daemon();


    /*!
     *  \brief  shutdown the websocket daemon
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t __shutdown_websocket_daemon();


    /*!
     *  \brief  daemon thread function of the capsule
     */
    void __websocket_serve_func();


    /*!
     *  \brief  heartbeat thread function of the capsule
     */
    void __heartbeat_func();


    /*!
     *  \brief  callback function for websocket
     *  \param  wsi     websocket instance
     *  \param  reason  reason for the callback
     *  \param  user    user data
     *  \param  in      input data
     *  \param  len     length of the input data
     */
    static int __websocket_callback(
        struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len
    );

    // utility for sending message to scheduler
    GWUtilWebSocketInstance *_ws_intance = nullptr;

    // websocket context
    gw_websocket_desp_t _ws_desp;

    // capsule daemon thread handle
    std::thread *_daemon_thread = nullptr;

    // capsule heartbeat thread handle
    std::thread *_heartbeat_thread = nullptr;

    // signal for stopping the event report thread
    volatile bool _is_event_report_stop = false;

    // signal for stopping the daemon thread
    volatile bool _is_daemon_stop = false;
    /* ======================== Communication w/ Scheduler ======================== */


    /* ======================== CUDA Specific ======================== */
    #ifdef GW_BACKEND_CUDA
    /* ============ CUDA - Checkpoint/Restore ============ */
 public:
    typedef struct gw_capsule_cuda_state {
    #if GW_BACKEND_CUDA
        NV::Cupti::Checkpoint::CUpti_Checkpoint* cupti_ckpt = nullptr;
        CUcontext cu_context = (CUcontext)0;
        CUdevice cu_device = (CUdevice)0;
        uint64_t saved_memory_size = 0;
    #endif
    } gw_capsule_cuda_state_t;


    /*!
     *  \brief  checkpoint CUDA state
     *  \param  cuda_state  the checkpointed state
     *  \param  do_push     mark whether to push the checkpoint to the queue
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t CUDA_checkpoint(gw_capsule_cuda_state_t &cuda_state, bool do_push=true);


    /*!
     *  \brief  restore CUDA state
     *  \param  cuda_state  the restored state, restore from the latest checkpoint if not specified
     *  \param  do_pop      mark whether to pop the checkpoint from the queue
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t CUDA_restore_from_latest(bool do_pop=true);
    gw_retval_t CUDA_restore(gw_capsule_cuda_state_t &cuda_state);


 private:
    // mutex for manage checkpoints
    std::mutex _mutex_checkpoints;

    // map of checkpointed CUDA state
    std::map<CUcontext, std::stack<gw_capsule_cuda_state_t>> _map_cuda_states;


    /* ============ CUDA - Trace Management ============ */
 public:
    /*!
     *  \brief  parse cufunction
     *  \param  cu_function         cufunction to be parsed
     *  \param  function_name       name of the function to be traced
     *  \param  grid_dim_x          grid dimension x
     *  \param  grid_dim_y          grid dimension y
     *  \param  grid_dim_z          grid dimension z
     *  \param  block_dim_x         block dimension x
     *  \param  block_dim_y         block dimension y
     *  \param  block_dim_z         block dimension z
     *  \param  shared_mem_bytes    shared memory bytes
     *  \param  stream              stream
     *  \param  params              origin parameters to call the kernel
     *  \param  extra               extra parameters to call the kernel
     *  \param  attrs               launch attributes (optional)
     *  \param  num_attrs           number of launch attributes
     *  \return GW_SUCCESS if success, GW_FAILED otherwise
     */
    gw_retval_t CUDA_trace_single_kernel(
        CUfunction function,
        std::string function_name,
        uint32_t grid_dim_x, uint32_t grid_dim_y, uint32_t grid_dim_z,
        uint32_t block_dim_x, uint32_t block_dim_y, uint32_t block_dim_z,
        uint32_t shared_mem_bytes,
        CUstream stream,
        void** params,
        void** extra,
        CUlaunchAttribute* attrs = nullptr,
        uint32_t num_attrs = 0
    );


    /* ============ CUDA - Module Management ============ */
 public:
    /*!
     *  \brief  cache the CUlibrary along with its binary data from specified file
     *  \note   this function should be thread safe
     *  \param  library         CUDA library handle
     *  \param  file_path_str   file path of the binary data
     *  \return GW_SUCCESS  for successful caching
     */
    gw_retval_t CUDA_cache_culibrary(CUlibrary library, std::string file_path_str);


    /*!
     *  \brief  cache the CUlibrary along with its binary data
     *  \note   this function should be thread safe
     *  \param  library CUDA library handle
     *  \param  data    binary data
     *  \return GW_SUCCESS  for successful caching
     */
    gw_retval_t CUDA_cache_culibrary(CUlibrary library, const void *data);


    /*!
     *  \brief  cache the CUmodule along with its binary data from specified file
     *  \note   this function should be thread safe
     *  \param  module          CUDA module handle
     *  \param  file_path_str   file path of the binary data
     *  \return GW_SUCCESS  for successful caching
     */
    gw_retval_t CUDA_cache_cumodule(CUmodule module, std::string file_path_str);


    /*!
     *  \brief  cache the CUmodule along with its binary data
     *  \note   this function should be thread safe
     *  \param  module  CUDA module handle
     *  \param  data    binary data
     *  \return GW_SUCCESS  for successful caching
     */
    gw_retval_t CUDA_cache_cumodule(CUmodule module, const void *data);


    /*!
     *  \brief  record the mapping of CUmodule to its parent CUlibary
     *  \note   this function should be thread safe
     *  \param  library CUDA library handle
     *  \param  data    binary data
     *  \return GW_SUCCESS  for successful recording
     */
    gw_retval_t CUDA_record_mapping_cumodule_culibrary(CUmodule module, CUlibrary library);


    /*!
     *  \brief  record the mapping of CUfunction to its parent CUmodule
     *  \note   this function should be thread safe
     *  \param  function CUDA function handle
     *  \param  module   CUDA module handle
     *  \return GW_SUCCESS  for successful recording
     */
    gw_retval_t CUDA_record_mapping_cufunction_cumodule(CUfunction function, CUmodule module);


    /*!
     *  \brief  parse CUfunction
     *  \note   this function should be thread safe
     *  \param  function                the CUfunction to be parsed
     *  \param  module                  the CUmodule that contain this function
     *  \param  do_parse_entire_binary  mark whether to parse the entire binary
     *  \return GW_SUCCESS  for successful parsing
     */
    gw_retval_t CUDA_parse_cufunction(CUfunction function, CUmodule module, bool do_parse_entire_binary=true);


    /*!
     *  \brief  record the function
     *  \note   this function is thread safe 
     *  \param  function    the function to be recorded
     *  \return GW_SUCCESS  for successful recording
     */
    gw_retval_t CUDA_report_function(CUfunction function);


 private:
    // mutex for manage modules
    std::mutex _mutex_module_management;

    // map of CUlibary to its contained binary data
    std::map<CUlibrary, std::vector<uint8_t>> _map_culibrary_data;

    // map of CUmodule to its contained binary data: <cucontext, <module, data>>
    std::map<CUcontext, std::map<CUmodule, std::vector<uint8_t>>> _map_cumodule_data;

    // map from CUmodule to its parent CUlibary: <cucontext, <module, culibrary>>
    std::map<CUcontext, std::map<CUmodule, CUlibrary>> _map_cumodule_culibrary;

    // map from CUfunction to its parent CUmodule: <cucontext, <function, module>>
    std::map<CUcontext, std::map<CUfunction, CUmodule>> _map_cufunction_cumodule;

    // map of CUfunction with its corresponding kernel definition: <cucontext, <cufunction, kerneldef>>
    std::map<CUcontext, std::map<CUfunction, GWKernelDef*>> _map_cufunction_kerneldef;

    // map of CUmodule with its contained fatbin: <cucontext, <module, fatbin>>
    std::map<CUcontext, std::map<CUmodule, GWBinaryImage*>> _map_cumodule_fatbin;

    // map of CUmodule with its contained cubin: <cucontext, <module, cubin>>
    std::map<CUcontext, std::multimap<CUmodule, GWBinaryImage*>> _map_cumodule_cubin;

    // map of CUmodule with its contained ptx: <cucontext, <module, ptx>>
    std::map<CUcontext, std::multimap<CUmodule, GWBinaryImage*>> _map_cumodule_ptx;

    // libraries registered by user
    std::map<CUfunction, gw_cuda_function_attribute_t*> _map_trace_function;

    // binary utilities
    GWBinaryUtility_CUDA _binary_utility_cuda;


    /* ============ CUDA - CUPTI Profiling ============ */
 public:
    // profiler context
    // NOTE(zhuobin):   we need to carefully coordinate profile context between
    //                  capsule and profiler process, as CUDA doesn't initiate 
    //                  multiple profiling context on the same device
    GWProfileContext_CUDA* profile_context_cuda = nullptr;

    // mutex for running profiler
    std::mutex _mutex_profiler;


    /* ============ CUDA - Coredump ============ */
 public:
    /*!
     *  \brief  get the path of the coredump file
     *  \return path of the coredump file
     */
    std::string get_coredump_file_path() const { return this->_coredump_file_path; }


 private:
    // path of the coredump file
    std::string _coredump_file_path = "";

    #endif // GW_BACKEND_CUDA
    /* ==================== CUDA Specific ==================== */
};
