#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <filesystem>

#include "common/common.hpp"
#include "common/log.hpp"
#include "profiler/context.hpp"
#include "scheduler/serve/profiler_message.hpp"
#include "common/utils/socket.hpp"
#include "common/utils/system.hpp"
#include "common/utils/string.hpp"
#include "common/utils/exception.hpp"


GWProfileContext::GWProfileContext(bool lazy_init_device) 
    :   _num_device(0),
        global_id(""),
        _log_file_path(""),
        _daemon_thread(nullptr),
        _heartbeat_thread(nullptr),
        _is_serving(false)
{
    // init TSC timer
    GW_CHECK_POINTER(this->_tsc_timer = new GWUtilTscTimer());
}


GWProfileContext::~GWProfileContext(){
    gw_retval_t retval;
    GW_CHECK_POINTER(this->_tsc_timer);
    delete this->_tsc_timer;

    if(this->_is_serving){
        GW_IF_FAILED(
            this->__shutdown_websocket_daemon(),
            retval,
            GW_ERROR_C("failed to shutdown websocket daemon");
        );

        if(likely(this->_ws_intance != nullptr)){
            delete this->_ws_intance;
        }

        this->_is_serving = false;
    }
}


gw_retval_t GWProfileContext::serve(){
    gw_retval_t retval = GW_SUCCESS;
    std::string env_value;
    std::string log_file_dir;
    bool do_redirect = true;
    int fd;
    std::error_code ec;

    // set global id
    this->global_id = GWUtilSocket::get_local_ip();

    // redirect log to file
    retval = GWUtilSystem::get_env_variable("GW_LOG_PATH", env_value);
    if(likely(retval == GW_SUCCESS)){
        log_file_dir = env_value;
    } else {
        log_file_dir = GW_DEFAULT_LOG_PATH;
    }
    this->_log_file_path = log_file_dir + std::string("/profiler_") + this->global_id + std::string(".log");

    if (unlikely(!std::filesystem::exists(log_file_dir))) {
        if (unlikely(!std::filesystem::create_directories(log_file_dir, ec))) {
            GW_WARN_C(
                "failed to create log file directory: dir(%s), err(%s)",
                log_file_dir.c_str(), ec.message().c_str()
            );
            do_redirect = false;
        } else {
            GW_DEBUG_C("log file directory created: dir(%s)", log_file_dir.c_str());
        }
    } else {
        GW_DEBUG_C("reused log file directory: dir(%s)", log_file_dir.c_str());
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

    GW_IF_FAILED(
        this->__start_websocket_daemon(),
        retval,
        {
            GW_WARN_C("failed to start websocket daemon");
            goto exit;
        }
    )
    this->_is_serving = true;

 exit:
    return retval;
}


gw_retval_t GWProfileContext::__start_websocket_daemon(){
    gw_retval_t retval = GW_SUCCESS, tmp_retval;
    struct lws_context_creation_info ws_context_create_info;
    struct lws_client_connect_info ws_ccinfo;
    std::string env_value;
    std::string scheduler_ip_addr;
    struct lws *wsi;

    struct lws_protocols __protocols[2] = {
        { 
            .name = "gwatch-websocket-protocol",
            .callback = &GWProfileContext::__websocket_callback,
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
    ws_ccinfo.path = "/profiler/main";
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

    this->_daemon_thread = new std::thread(&GWProfileContext::__websocket_serve_func, this);
    GW_CHECK_POINTER(this->_daemon_thread);

 exit:
    return retval;
}


gw_retval_t GWProfileContext::__shutdown_websocket_daemon(){
    gw_retval_t retval = GW_SUCCESS;

    GW_CHECK_POINTER(this->_daemon_thread);

    // we need to wait all message be sent out
    if(this->_ws_intance != nullptr){
        while(!this->_ws_intance->send_queue.empty()){}
    }
    
    if(this->_daemon_thread != nullptr){
        this->_is_daemon_stop = true;
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


void GWProfileContext::__websocket_serve_func(){
    GW_DEBUG_C("websocket serve daemon started");
    while(likely(!this->_is_daemon_stop)){
        lws_service(this->_ws_desp.wb_context, -1);
    }
    GW_DEBUG_C("websocket serve daemon stopped");
}


int GWProfileContext::__websocket_callback(
    struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len
){
    int retval = 0, sdk_retval = 0;
    gw_retval_t gw_retval = GW_SUCCESS;
    struct sockaddr_in scheduler_addr;
    socklen_t scheduler_addr_len = sizeof(scheduler_addr);
    char scheduler_ip[INET_ADDRSTRLEN] = { 0 };
    uint16_t scheduler_port;
    GWProfileContext *context;
    char *recv_buf;
    GWInternalMessage_Profiler message;

    GW_CHECK_POINTER(wsi);
    GW_CHECK_POINTER(in);
    GW_CHECK_POINTER(
        context = static_cast<GWProfileContext*>(lws_context_user(lws_get_context(wsi)))
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
        GW_CHECK_POINTER(context->_ws_intance = new GWUtilWebSocketInstance(wsi, &(context->_is_daemon_stop)));
        context->_heartbeat_thread = new std::thread(&GWProfileContext::__heartbeat_func, context);
        GW_CHECK_POINTER(context->_heartbeat_thread);
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        GW_DEBUG("disconnected of scheduler, shutdown the profiler websocket daemon");
        context->_is_daemon_stop = true;
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        GW_DEBUG("websocket received: ip(%s), port(%u), length(%lu)", scheduler_ip, scheduler_port, len);
        GW_IF_FAILED(
            context->_ws_intance->recv_to_buf(in, len),
            sdk_retval,
            {
                GW_WARN("failed to record received message to context instance");
                retval = -1;
                goto exit;
            }
        );
        if(lws_is_final_fragment(wsi)){
            recv_buf = context->_ws_intance->export_recv_buf();
            GW_DEBUG("received message from scheduler: ip(%s), port(%u), message(%s)", scheduler_ip, scheduler_port, recv_buf);

            message.deserialize(recv_buf);
            context->_ws_intance->reset_recv_buf();

            switch (message.type_id)
            {
            case GW_MESSAGE_TYPEID_PROFILER_SET_METRICS:
                context->__process_SET_METRICS(&message);
                break;
            case GW_MESSAGE_TYPEID_PROFILER_BEGIN:
                context->__process_BEGIN(&message);
                break;
            case GW_MESSAGE_TYPEID_PROFILER_END:
                context->__process_END(&message);
                break;
            default:
                GW_WARN(
                    "received unknown scheduler message type: message_type(%d)",
                    message.type_id
                );
            }
        }
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        GW_IF_FAILED(
            GWUtilWebSocketInstance::wsi_send_chunk(context->_ws_intance),
            gw_retval,
            {
                retval = -1;
                GW_WARN("failed to send out websocket to scheduler");
            }
        );
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        GW_WARN("websocket connection error: %s", in ? (const char *)in : "(null)")
        retval = -1;
        break;

    default:
        // GW_DEBUG("websocket callback: ip(%s), port(%u), reason(%d)", scheduler_ip, scheduler_port, reason);
        break;
    }

exit:
    return retval;
}


void GWProfileContext::__heartbeat_func(){
    gw_retval_t retval;
    GW_DEBUG_C("heartbeat thread started");
    GWInternalMessage_Profiler message;

    message.type_id = GW_MESSAGE_TYPEID_COMMON_HEARTBEAT;

    static constexpr int64_t GW_PROFILER_HEARTBEAT_INTERVAL = 5;

    while(likely(!this->_is_daemon_stop)){
        GW_IF_FAILED(
            this->send_to_scheduler(&message),
            retval,
            {
                GW_WARN_C("failed to send heartbeat message to scheduler: %s", gw_retval_str(retval));
                break;
            }
        );
        std::this_thread::sleep_for(std::chrono::seconds(GW_PROFILER_HEARTBEAT_INTERVAL));
    }
    GW_DEBUG_C("heartbeat thread stopped");
}


gw_retval_t GWProfileContext::send_to_scheduler(GWInternalMessage_Profiler *message){
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


void GWProfileContext::__process_SET_METRICS(GWInternalMessage_Profiler *message){
    gw_retval_t tmp_retval = GW_SUCCESS, dirty_retval = GW_SUCCESS;
    gw_image_sign_t sign;
    GWProfileDevice *gw_device = nullptr;
    GWProfiler *gw_profiler = nullptr;
    GWInternalMessagePayload_Profiler_SetMetrics *payload;
    int i;
    bool is_profiler_exist = false;

    payload = message->get_payload_ptr<GWInternalMessagePayload_Profiler_SetMetrics>(
        GW_MESSAGE_TYPEID_PROFILER_SET_METRICS
    );

    for(i=0; i<this->_num_device; i++){
        // init device (if needed) and obtain chip name
        GW_IF_FAILED(
            this->__init_gw_device(/* device_id */ i),
            tmp_retval,
            {
                GW_WARN("failed to init device: device_id(%d)", i);
                dirty_retval = tmp_retval;
                continue;
            }
        );

        GW_CHECK_POINTER(gw_device = this->_gw_devices[i]);

        // check whether the profiler with same sign and device_id has beed created
        sign = GWProfileImage::sign(payload->metric_names, gw_device->get_chip_name());
        if(this->_map_profiler.count(i) > 0){
            if(this->_map_profiler[i].count(sign) > 0){
                is_profiler_exist = true;
                GW_CHECK_POINTER(gw_profiler = this->_map_profiler[i][sign]);
            }
        }

        // create profiler if not exist
        if(!is_profiler_exist){
            #ifdef GW_BACKEND_CUDA
                GW_IF_FAILED(
                    this->create_profiler(
                        /* device_id */ i,
                        payload->metric_names,
                        GWProfiler_Mode_CUDA_PM_Sampling,
                        &gw_profiler
                    ),
                    tmp_retval,
                    {
                        GW_WARN("failed to create profiler: device_id(%d)", i);
                        dirty_retval = tmp_retval;
                        continue;
                    }
                );
            #else
                GW_ERROR("NOT IMPLEMENT")
            #endif
            this->_map_profiler[i][sign] = gw_profiler;
        }
        GW_CHECK_POINTER(gw_profiler);
    }

    // set payload
    payload->sign = gw_profiler->get_sign();
    if(dirty_retval != GW_SUCCESS)
        payload->success = false;
    else
        payload->success = true;

    // send back the response of SET_METRICS request
    GW_IF_FAILED(
        this->send_to_scheduler(message),
        tmp_retval,
        {
            GW_WARN_C("failed to send back the response of SET_METRICS request");
        }
    );
}


void GWProfileContext::__process_BEGIN(GWInternalMessage_Profiler *message){

}


void GWProfileContext::__process_END(GWInternalMessage_Profiler *message){
 
}
