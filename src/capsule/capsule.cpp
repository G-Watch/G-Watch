#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <string>
#include <cstring>
#include <filesystem>
#include <queue>
#include <atomic>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <dlfcn.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <numa.h>
#include <libwebsockets.h>


#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/event.hpp"
#include "capsule/capsule.hpp"
#include "capsule/metric.hpp"
#include "capsule/trace.hpp"
#include "scheduler/serve/capsule_message.hpp"
#include "common/utils/system.hpp"
#include "common/utils/string.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/socket.hpp"
#include "common/utils/queue.hpp"


#ifdef GW_BACKEND_CUDA
    #include <cuda.h>
    #include <cuda_runtime_api.h>
#endif


thread_local std::thread* GWCapsule::_event_report_thread = nullptr;
thread_local GWEventTrace *GWCapsule::event_trace = nullptr;
thread_local std::vector<GWTraceTask*> GWCapsule::_list_trace_task;
thread_local std::vector<GWTraceTask*> GWCapsule::_list_trace_task_kernel;


GWCapsule::GWCapsule()
    #ifdef GW_BACKEND_CUDA
        :
        _binary_utility_cuda()
    #endif
{
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval = 0;
    std::string log_file_dir;
    std::string env_value;
    std::error_code ec;
    bool do_redirect = true;
    int fd = 0;

    // set global id
    this->global_id = "capsule-" + GWUtilSocket::get_local_ip() + "-" + std::to_string(getpid());

    // load instrumentation library
    this->_lib_gwatch_instrunmentation = dlopen("libgwatch_instrumentation.so", RTLD_NOW | RTLD_GLOBAL);
    if(unlikely(this->_lib_gwatch_instrunmentation == nullptr)){
        GW_ERROR_C("failed to load instrumentation library: libname(libgwatch_instrumentation.so), err(%s)", dlerror());
    }

    // redirect log to file
    retval = GWUtilSystem::get_env_variable("GW_LOG_PATH", env_value);
    if(likely(retval == GW_SUCCESS)){
        log_file_dir = env_value;
    } else {
        log_file_dir = GW_DEFAULT_LOG_PATH;
    }
    this->_log_file_dir = log_file_dir + std::string("/") + this->global_id;
    this->_log_file_path = this->_log_file_dir + std::string("/main.log");

    if (unlikely(!std::filesystem::exists(this->_log_file_dir))) {
        if (unlikely(!std::filesystem::create_directories(this->_log_file_dir, ec))) {
            GW_WARN_C(
                "failed to create log file directory: dir(%s), err(%s)",
                this->_log_file_dir.c_str(), ec.message().c_str()
            );
            do_redirect = false;
        } else {
            GW_DEBUG_C("log file directory created: dir(%s)", this->_log_file_dir.c_str());
        }
    } else {
        GW_DEBUG_C("reused log file directory: dir(%s)", this->_log_file_dir.c_str());
    }

    if(do_redirect){
        fd = open(this->_log_file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(fd < 0){
            GW_WARN_C(
                "failed to open log file: file_path(%s), err(%s)",
                this->_log_file_path.c_str(),
                strerror(errno)
            );
        } else {
            GW_DEBUG_C("redirect stdout and stderr to log file: file_path(%s)", this->_log_file_path.c_str());
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
    }

    // initailize profiler
    #if GW_BACKEND_CUDA
        retval = GWUtilSystem::get_env_variable("GW_ENABLE_COREDUMP", env_value);
        if(likely(retval == GW_SUCCESS)){
            GW_DEBUG_C("coredump enabled, profiler is disabled")
            this->_coredump_file_path = this->_log_file_dir + "/core.%t.%h.%p.nvcudmp";
        } else {
            GW_CHECK_POINTER(this->profile_context_cuda = new GWProfileContext_CUDA());
        }
    #endif

    GW_IF_FAILED(
        this->__start_websocket_daemon(),
        retval,
        throw GWException("failed to start websocket daemon");
    );
}


GWCapsule::~GWCapsule() {
    gw_retval_t retval = GW_SUCCESS;
    std::lock_guard lock(this->_mutex_map_event_report_thread);

    this->_is_event_report_stop = true;

    // join all event report threads
    for(auto& thread_pair : this->_map_event_report_thread){
        GW_CHECK_POINTER(thread_pair.second);
        if(thread_pair.second->joinable()){
            thread_pair.second->join();
        }
        delete thread_pair.second;
        thread_pair.second = nullptr;
    }

    GW_IF_FAILED(
        this->__shutdown_websocket_daemon(),
        retval,
        GW_ERROR_C("failed to shutdown websocket daemon");
    );

    if(likely(this->_ws_intance != nullptr))
        delete this->_ws_intance;

    // close instrumentation library
    if(likely(this->_lib_gwatch_instrunmentation != nullptr)){
        dlclose(this->_lib_gwatch_instrunmentation);
        this->_lib_gwatch_instrunmentation = nullptr;
    }
}


gw_retval_t GWCapsule::register_trace_task(GWTraceTask* trace_task){
    gw_retval_t retval = GW_SUCCESS;

    GW_CHECK_POINTER(trace_task);
    this->_list_trace_task.push_back(trace_task);

    if(trace_task->get_type().starts_with("kernel:")){
        this->_list_trace_task_kernel.push_back(trace_task);
    } else {
        GW_ERROR_C_DETAIL("shouldn't be here");
    }

exit:
    return retval;
}


gw_retval_t GWCapsule::unregister_trace_task(GWTraceTask* trace_task){
    gw_retval_t retval = GW_SUCCESS;

    GW_CHECK_POINTER(trace_task);
    this->_list_trace_task.erase(
        std::remove(this->_list_trace_task.begin(), this->_list_trace_task.end(), trace_task),
        this->_list_trace_task.end()
    );

    if(trace_task->get_type().starts_with("kernel:")){
        this->_list_trace_task_kernel.erase(
            std::remove(this->_list_trace_task_kernel.begin(), this->_list_trace_task_kernel.end(), trace_task),
            this->_list_trace_task_kernel.end()
        );
    } else {
        GW_ERROR_C_DETAIL("shouldn't be here");
    }

exit:
    return retval;
}


bool GWCapsule::do_need_trace_kernel(std::string kernel_name){
    bool retval = false;

    for(auto trace_task : this->_list_trace_task_kernel){
        if(trace_task->do_need_trace(kernel_name)){
            retval = true;
            break;
        }
    }

exit:
    return retval;
}


gw_retval_t GWCapsule::start_trace(std::string name, uint64_t begin_hash, std::string line_position){
    gw_retval_t retval = GW_SUCCESS;
    GWAppMetricTrace *metric_trace = nullptr;

    if((this->_map_begin_hash_to_app_trace.count(begin_hash) > 0)){
        GW_CHECK_POINTER(metric_trace = this->_map_begin_hash_to_app_trace[begin_hash]);
    } else {
        GW_CHECK_POINTER(metric_trace = new GWAppMetricTrace(name, begin_hash, line_position));
        this->_map_begin_hash_to_app_trace[begin_hash]= metric_trace;
    }

    metric_trace->start_capture();
    this->_trace_counter.fetch_add(1, std::memory_order_relaxed);

    return retval;
}


gw_retval_t GWCapsule::stop_trace(uint64_t begin_hash, uint64_t end_hash, std::string line_position){
    gw_retval_t retval = GW_SUCCESS;
    GWAppMetricTrace *metric_trace;

    if(unlikely(this->_map_begin_hash_to_app_trace.find(begin_hash) == this->_map_begin_hash_to_app_trace.end())){
        GW_WARN_C("failed to find metric trace with begin hash: hash(%lu)", begin_hash);
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }
    GW_CHECK_POINTER(metric_trace = this->_map_begin_hash_to_app_trace[begin_hash]);

    metric_trace->mark_end_hash(end_hash);
    metric_trace->stop_capture();
    this->_map_end_hash_to_metric_trace[end_hash]= metric_trace;

    this->_trace_counter.fetch_sub(1, std::memory_order_relaxed);

 exit:
    return retval;
}


void GWCapsule::ensure_event_trace(){
    uint64_t linux_thread_id = 0;

    linux_thread_id = (uint64_t)(pthread_self());

    // make sure event trace exist
    if(unlikely(GWCapsule::event_trace == nullptr)){
        GW_CHECK_POINTER(GWCapsule::event_trace = new GWEventTrace(this->_is_event_report_stop));
        GWCapsule::event_trace->set_name("capsule_event_trace-" + this->global_id);
        GW_DEBUG_C("create event trace: linux_thread_id(%lu)", linux_thread_id);
    }

    // make sure event trace report thread exist
    if(unlikely(GWCapsule::_event_report_thread == nullptr)){
        GW_CHECK_POINTER(GWCapsule::_event_report_thread = new std::thread(
            GWCapsule::__event_report_func, this, GWCapsule::event_trace, linux_thread_id
        ));
        this->_mutex_map_event_report_thread.lock();
        this->_map_event_report_thread[linux_thread_id] = GWCapsule::_event_report_thread;
        this->_mutex_map_event_report_thread.unlock();
        GW_DEBUG_C("create event trace report thread: linux_thread_id(%lu)", linux_thread_id);
    }
}


void GWCapsule::__event_report_func(GWCapsule* _this, GWEventTrace *event_trace, uint64_t linux_thread_id){
    gw_retval_t retval = GW_SUCCESS;
    GWEvent *event = nullptr;
    GWInternalMessage_Capsule *capsule_message = nullptr;
    GWInternalMessagePayload_Common_DB_TS_Write *payload = nullptr;
    std::queue<GWEvent*> queue_non_ready_events;
    #if GW_BACKEND_CUDA
        CUresult cudv_retval = CUDA_SUCCESS;
    #endif
    
    GW_CHECK_POINTER(_this);
    GW_CHECK_POINTER(event_trace);

    GW_DEBUG("start to report event stream to scheduler: linux_thread_id(%lu)", linux_thread_id);

    while(
        likely(!_this->_is_event_report_stop)
        or !queue_non_ready_events.empty()
        or !event_trace->is_event_trace_empty()
    ){
        event = nullptr;

        // first we try dequeue from the cache queue
        if(!queue_non_ready_events.empty()){
            event = queue_non_ready_events.front();
            GW_CHECK_POINTER(event);
            queue_non_ready_events.pop();

            // if the event isn't ready, we push back to the queue
            if(event->is_archived() == false){
                queue_non_ready_events.push(event);
                event = nullptr;
            }
        }

        // if the cache is empty or nothing in cache is ready, we try dequeue from the event trace
        if(event == nullptr){
            if(event_trace->pop_event(event) == GW_SUCCESS){
                GW_CHECK_POINTER(event);

                // if the event isn't ready, we push back to the queue
                if(event->is_archived() == false){
                    queue_non_ready_events.push(event);
                    event = nullptr;
                }
            }
        }

        /*!
         *  \note(zhuobin): if the report thread is about to close,
         *                  and there still unfinished GPU event,
         *                  we need to check whether there's sticky
         *                  error on current context; if it's, we
         *                  need to manully end these events    
         */
        #if GW_BACKEND_CUDA
            if(
                unlikely(_this->_is_event_report_stop)
                and event == nullptr
                and event->type_id == GW_EVENT_TYPE_GPU
                and !queue_non_ready_events.empty()
            ){
                cudv_retval = cuCtxSynchronize();
                if(unlikely(cudv_retval != CUDA_SUCCESS)){
                    event = queue_non_ready_events.front();
                    GW_CHECK_POINTER(event);
                    queue_non_ready_events.pop();
                    event->record_tick("end");
                    event->set_metadata("return code", cudv_retval);
                    event->archive();
                }
            }
        #endif

        if(event != nullptr){
            GW_ASSERT(event->is_archived());

            GW_CHECK_POINTER(capsule_message = new GWInternalMessage_Capsule());
            payload = capsule_message->get_payload_ptr<GWInternalMessagePayload_Common_DB_TS_Write>(GW_MESSAGE_TYPEID_COMMON_TS_WRITE_DB);
            GW_CHECK_POINTER(payload);

            // make payload
            capsule_message->type_id = GW_MESSAGE_TYPEID_COMMON_TS_WRITE_DB;
            payload->uri = std::format(
                "/capsule/{}/{}event",
                _this->global_id,
                GWEvent::typeid_to_string(event->type_id)
            );
            payload->index = event->id;
            payload->timestamp = GWUtilTscTimer::get_tsc();
            payload->payload = event->to_json();

            // send to scheduler
            GW_IF_FAILED(
                _this->send_to_scheduler(capsule_message),
                retval,
                {
                    GW_WARN(
                        "failed to write_kvdb for event report: %s",
                        capsule_message->serialize().c_str()
                    );
                    goto _exit;
                }
            );
            GW_DEBUG("reported event: linux_thread_id(%lu), event_id(%lu)", linux_thread_id, event->id);

        _exit:
            // should be save to delete the event here, as
            // all parent / relative connection should be form
            // before the event is archived
            // delete event;

            if(capsule_message != nullptr){
                delete capsule_message;
                capsule_message = nullptr;
            }
        }
    }

    GW_DEBUG("stop to report event stream to scheduler: linux_thread_id(%lu)", linux_thread_id);
}


gw_retval_t GWCapsule::send_to_scheduler(GWInternalMessage_Capsule *message){
    gw_retval_t retval = GW_SUCCESS;

    if(this->_ws_intance == nullptr){
        GW_WARN_C("failed to send message to scheduler, websocket not ready");
        retval = GW_FAILED_NOT_READY;
        goto exit;
    }

    GW_IF_FAILED(
        this->_ws_intance->send(message->serialize()),
        retval,
        {
            GW_WARN_C("failed to send message to scheduler: %s", gw_retval_str(retval));
            goto exit;
        }
    );

exit:
    return retval;
}


gw_retval_t GWCapsule::sync_send_to_scheduler(){
    gw_retval_t retval = GW_SUCCESS;
    
    if(this->_ws_intance == nullptr){
        GW_WARN_C("failed to sync send message to scheduler, websocket not ready");
        retval = GW_FAILED_NOT_READY;
        goto exit;
    }

    GW_IF_FAILED(
        this->_ws_intance->sync_send(),
        retval,
        {
            GW_WARN_C("failed to sync send message to scheduler: %s", gw_retval_str(retval));
            goto exit;
        }
    );

exit:
    return retval;
}


gw_retval_t GWCapsule::__start_websocket_daemon(){
    gw_retval_t retval = GW_SUCCESS, tmp_retval;
    struct lws_context_creation_info ws_context_create_info;
    struct lws_client_connect_info ws_ccinfo;
    std::string env_value;
    std::string scheduler_ip_addr;
    struct lws *wsi;

    struct lws_protocols __protocols[2] = {
        { 
            .name = "gwatch-websocket-protocol",
            .callback = &GWCapsule::__websocket_callback,
            .per_session_data_size = sizeof(void*),
            .rx_buffer_size = GW_WEBSOCKET_MAX_MESSAGE_LENGTH,
        },
        { NULL, NULL, 0, 0 }
    };
    
    this->_ws_desp.list_wb_protocols.push_back(__protocols[0]);
    this->_ws_desp.list_wb_protocols.push_back(__protocols[1]);

    memset(&ws_context_create_info, 0, sizeof(ws_context_create_info));
    ws_context_create_info.port = CONTEXT_PORT_NO_LISTEN;
    ws_context_create_info.protocols = this->_ws_desp.list_wb_protocols.data();
    ws_context_create_info.user = this;

    if(unlikely(nullptr == (
        this->_ws_desp.wb_context = lws_create_context(&ws_context_create_info)
    ))){
        GW_WARN_C("failed to create websocket context");
        retval = GW_FAILED;
        goto exit;
    }

    tmp_retval = GWUtilSystem::get_env_variable("GW_SCHEDULER_IPV4", env_value);
    if(likely(tmp_retval == GW_SUCCESS)){
        scheduler_ip_addr = env_value;
    } else {
        scheduler_ip_addr = GW_SCHEDULER_IPV4;
    }
    tmp_retval = GWUtilSystem::get_env_variable("GW_SCHEDULER_SERVE_WS_PORT", env_value);
    if(likely(tmp_retval == GW_SUCCESS)){
        this->_ws_desp.port = GWUtilString::string_to_uint16(env_value);
    } else {
        this->_ws_desp.port = GW_SCHEDULER_SERVE_WS_PORT;
    }

    memset(&ws_ccinfo, 0, sizeof(ws_ccinfo));
    ws_ccinfo.context = this->_ws_desp.wb_context;
    ws_ccinfo.address = scheduler_ip_addr.c_str();
    ws_ccinfo.port = this->_ws_desp.port;
    ws_ccinfo.path = "/capsule/main";
    ws_ccinfo.host = lws_canonical_hostname(this->_ws_desp.wb_context);
    ws_ccinfo.origin = "origin";
    ws_ccinfo.protocol = "gwatch-websocket-protocol";

    wsi = lws_client_connect_via_info(&ws_ccinfo);
    if(unlikely(nullptr == wsi)){
        GW_WARN_C(
            "failed to connect to scheduler: ip_addr(%s), port(%u)",
            scheduler_ip_addr.c_str(), this->_ws_desp.port
        );
        retval = GW_FAILED;
        goto exit;
    }
    GW_DEBUG_C("connecting to scheduler...: ip_addr(%s), port(%u)", scheduler_ip_addr.c_str(), this->_ws_desp.port);

    this->_daemon_thread = new std::thread(&GWCapsule::__websocket_serve_func, this);
    GW_CHECK_POINTER(this->_daemon_thread);

 exit:
    return retval;
}


gw_retval_t GWCapsule::__shutdown_websocket_daemon(){
    gw_retval_t retval = GW_SUCCESS;

    // TODO(zhuobin): we need to wait until all send messages are done

    this->_is_daemon_stop = true;

    if(this->_daemon_thread != nullptr){
        if(this->_daemon_thread->joinable())
            this->_daemon_thread->join();
        delete this->_daemon_thread;
    }

    if(this->_heartbeat_thread != nullptr){
        if(this->_heartbeat_thread->joinable())
            this->_heartbeat_thread->join();
        delete this->_heartbeat_thread;
    }

    lws_context_destroy(this->_ws_desp.wb_context);

    return retval;
}


void GWCapsule::__websocket_serve_func(){
    GW_DEBUG_C("websocket serve daemon started");
    while(likely(!this->_is_daemon_stop)){
        lws_service(this->_ws_desp.wb_context, -1);
    }
    GW_DEBUG_C("websocket serve daemon stopped");
}


void GWCapsule::__heartbeat_func(){
    gw_retval_t retval;
    GW_DEBUG_C("heartbeat thread started");
    GWInternalMessage_Capsule message;

    message.type_id = GW_MESSAGE_TYPEID_CAPSULE_HEARTBEAT;

    static constexpr int64_t GW_CAPSULE_HEARTBEAT_INTERVAL = 5;

    while(likely(!this->_is_daemon_stop)){
        // GW_IF_FAILED(
        //     this->send_to_scheduler(&message),
        //     retval,
        //     {
        //         GW_WARN_C("failed to send heartbeat message to scheduler: %s", gw_retval_str(retval));
        //         break;
        //     }
        // );
        // std::this_thread::sleep_for(std::chrono::seconds(GW_CAPSULE_HEARTBEAT_INTERVAL));
    }
    GW_DEBUG_C("heartbeat thread stopped");
}


int GWCapsule::__websocket_callback(
    struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len
){
    int retval = 0, sdk_retval = 0;
    gw_retval_t gw_retval = GW_SUCCESS;
    struct sockaddr_in scheduler_addr;
    socklen_t scheduler_addr_len = sizeof(scheduler_addr);
    char scheduler_ip[INET_ADDRSTRLEN] = { 0 };
    uint16_t scheduler_port;
    GWCapsule *capsule;
    char *recv_buf;
    GWInternalMessage_Capsule message;

    GW_CHECK_POINTER(wsi);
    GW_CHECK_POINTER(in);
    GW_CHECK_POINTER(
        capsule = static_cast<GWCapsule*>(lws_context_user(lws_get_context(wsi)))
    );

    // obtain scheduler address
    getpeername(lws_get_socket_fd(wsi), (struct sockaddr*)&scheduler_addr, &scheduler_addr_len);
    inet_ntop(AF_INET, &(scheduler_addr.sin_addr), scheduler_ip, INET_ADDRSTRLEN);
    scheduler_port = ntohs(scheduler_addr.sin_port);

    gw_socket_addr_t scheduler_socket_addr(scheduler_addr);

    // check reason at https://libwebsockets.org/lws-api-doc-main/html/lws-callbacks_8h_source.html
    switch (reason)
    {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        GW_DEBUG("connected to scheduler: ip(%s), port(%u)", scheduler_ip, scheduler_port);
        GW_CHECK_POINTER(capsule->_ws_intance = new GWUtilWebSocketInstance(wsi, &(capsule->_is_daemon_stop)));
        capsule->_heartbeat_thread = new std::thread(&GWCapsule::__heartbeat_func, capsule);
        GW_CHECK_POINTER(capsule->_heartbeat_thread);
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        GW_DEBUG("disconnected of scheduler, shutdown the capsule websocket daemon");
        capsule->_is_daemon_stop = true;
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        GW_DEBUG("websocket received: ip(%s), port(%u), length(%lu)", scheduler_ip, scheduler_port, len);
        GW_IF_FAILED(
            capsule->_ws_intance->recv_to_buf(in, len),
            sdk_retval,
            {
                GW_WARN("failed to record received message to capsule instance");
                retval = -1;
                goto exit;
            }
        );
        if(lws_is_final_fragment(wsi)){
            recv_buf = capsule->_ws_intance->export_recv_buf();
            GW_DEBUG("received message from scheduler: ip(%s), port(%u), message(%s)", scheduler_ip, scheduler_port, recv_buf);

            message.deserialize(recv_buf);
            capsule->_ws_intance->reset_recv_buf();

            switch (message.type_id)
            {
            case GW_MESSAGE_TYPEID_CAPSULE_REGISTER:
                capsule->__process_capsule_resp_REGISTER(&message);
                break;
            default:
                GW_WARN("received unknown scheduler message type: message_type(%d)", message.type_id);
            }
        }
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        GW_IF_FAILED(
            GWUtilWebSocketInstance::wsi_send_chunk(capsule->_ws_intance),
            gw_retval,
            {
                retval = -1;
                GW_WARN("failed to send out websocket to capsule");
            }
        );
        break;

    default:
        // GW_DEBUG("websocket callback: ip(%s), port(%u), reason(%d)", scheduler_ip, scheduler_port, reason);
        break;
    }

exit:
    return retval;
}
