#include <iostream>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <libwebsockets.h>
#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/system.hpp"
#include "common/utils/string.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/socket.hpp"
#include "common/utils/command_caller.hpp"
#include "scheduler/serve/capsule_message.hpp"
#include "scheduler/scheduler.hpp"
#include "scheduler/serve/capsule_instance.hpp"


GWScheduler::GWScheduler(gw_scheduler_config_t config) 
    : _config(config), _is_daemon_stop(false)
{
    gw_retval_t retval = GW_SUCCESS;
    GW_IF_FAILED(
        this->__init_db(),
        retval,
        GW_WARN_C("failed to initialize scheduler database");
    );
}


GWScheduler::~GWScheduler() {
    std::string cmd = "", cmd_output = "";
    pid_t pgid = 0;
    int sdk_retval = 0;
    gw_process_launch_meta_t *profiler_launch_meta = nullptr;

    // kill main capsule
    if(this->_main_capsule_launch_meta != nullptr){
        if(this->_main_capsule_launch_meta->pipe != nullptr){
            pgid = getpgid(this->_main_capsule_launch_meta->pid);
            if(pgid == -1){
                GW_WARN_C(
                    "failed to get pgid of %d: %s",
                    this->_main_capsule_launch_meta->pid,
                    strerror(errno)
                );
                goto exit;
            }

            GW_IF_LIBC_FAILED(
                kill(-pgid, SIGTERM),
                sdk_retval,
                {
                    GW_WARN_C("failed to kill main capsule: output(%s)", strerror(errno));
                    goto exit;
                }
            );
            GW_DEBUG_C("kill main capsule process: pid(%d)", this->_main_capsule_launch_meta->pid);

            GWUtilCommandCaller::pclose2(
                this->_main_capsule_launch_meta->pipe,
                this->_main_capsule_launch_meta->pid,
                /* wait */ false
            );
        }
    }

    // kill gtrace
    if(this->_gtrace_launch_meta != nullptr){
        if(this->_gtrace_launch_meta->pipe != nullptr){
            pgid = getpgid(this->_gtrace_launch_meta->pid);
            if(pgid == -1){
                GW_WARN_C(
                    "failed to get pgid of %d: %s",
                    this->_gtrace_launch_meta->pid,
                    strerror(errno)
                );
                goto exit;
            }

            GW_IF_LIBC_FAILED(
                kill(-pgid, SIGTERM),
                sdk_retval,
                {
                    GW_WARN_C("failed to kill gtrace: output(%s)", strerror(errno));
                    goto exit;
                }
            );
            GW_DEBUG_C("kill gtrace process: pid(%d)", this->_gtrace_launch_meta->pid);

            GWUtilCommandCaller::pclose2(
                this->_gtrace_launch_meta->pipe,
                this->_gtrace_launch_meta->pid,
                /* wait */ false
            );
        }
    }

    // kill all profiler
    for(auto it=this->_map_ip_to_launch_meta.begin(); it!= this->_map_ip_to_launch_meta.end(); it++){
        GW_CHECK_POINTER(profiler_launch_meta = it->second);
        if(profiler_launch_meta->pipe!= nullptr){
            pgid = getpgid(profiler_launch_meta->pid);
            if(pgid == -1){
                GW_WARN_C(
                    "failed to get pgid of %d: %s",
                    profiler_launch_meta->pid,
                    strerror(errno)
                );
                goto exit;
            }

            GW_IF_LIBC_FAILED(
                kill(-pgid, SIGTERM),
                sdk_retval,
                {
                    GW_WARN_C("failed to kill profiler: output(%s)", strerror(errno));
                    goto exit;
                }
            )
            GW_DEBUG_C("kill created profiler: pid(%d)", profiler_launch_meta->pid);
            
            GWUtilCommandCaller::pclose2(
                profiler_launch_meta->pipe,
                profiler_launch_meta->pid,
                /* wait */ false
            );
        }
    }

    // shutdown all websocket daemons
    GW_DEBUG("shutting down websocket daemons...");
    GW_IF_FAILED(
        this->__shutdown_all_websocket_daemons(),
        sdk_retval,
        GW_WARN_C("failed to shutdown websocket daemons")
    );

    // delete all capsule instance
    // GW_DEBUG("deleting capsule instances...");
    // for(auto it=this->_map_capsule.begin(); it != this->_map_capsule.end(); it++){
    //     GW_IF_FAILED(
    //         this->__destory_capsule_instance(it->second),
    //         sdk_retval,
    //         GW_WARN_C("failed to destory capsule instance")
    //     );
    // }

 exit:  
    ;
}


gw_retval_t GWScheduler::serve(){
    gw_retval_t retval = GW_SUCCESS, tmp_retval = GW_SUCCESS;
    gw_websocket_desp_t ws_desp;
    std::string log_path = "";
    std::string env_value = "";
    std::string log_file_dir = "";
    std::error_code ec;

    // open websocket daemon
    tmp_retval = GWUtilSystem::get_env_variable("GW_SCHEDULER_SERVE_WS_PORT", env_value);
    if(likely(tmp_retval == GW_SUCCESS)){
        ws_desp.port = GWUtilString::string_to_uint16(env_value);
    } else {
        ws_desp.port = GW_SCHEDULER_SERVE_WS_PORT;
    }
    ws_desp.wb_callback = &GWScheduler::__websocket_callback;
    ws_desp.wb_serve_func = &GWScheduler::__websocket_serve_func;
    GW_IF_FAILED(
        this->__start_websocket_daemon(ws_desp),
        tmp_retval,
        {
            GW_WARN_C("failed to start websocket daemon");
            retval = GW_FAILED;
            goto exit;
        }
    )

    // create log file path for capsule and profiler
    log_path = this->_config.COMMON_log_path;
    if (unlikely(!std::filesystem::exists(log_path))) {
        if (unlikely(!std::filesystem::create_directories(log_path, ec))) {
            GW_WARN_C(
                "failed to create log file directory: dir(%s), err(%s)",
                log_path.c_str(), ec.message().c_str()
            );
        } else {
            GW_DEBUG_C("log file directory created: dir(%s)", log_path.c_str());
        }
    } else {
        GW_DEBUG_C("reused log file directory: dir(%s)", log_path.c_str());
    }
    if (std::filesystem::exists(log_path, ec) && std::filesystem::is_directory(log_path, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(log_path, ec)) {
            if (std::filesystem::is_regular_file(entry.path(), ec)) {
                std::filesystem::remove(entry.path(), ec);
                if (ec) {
                    GW_WARN_C(
                        "failed to remove legacy log file: path(%s), err(%s)",
                        entry.path().c_str(),
                        ec.message().c_str()
                    );
                }
            }
        }
    }

    // start gtrace if enabled
    if(this->_config.COMMON_enable_visualize){
        GW_IF_FAILED(
            this->start_gtrace(),
            retval,
            {
                GW_WARN_C("failed to start gtrace");
                goto exit;
            }
        );
    }

 exit:
    return retval;
}


gw_retval_t GWScheduler::start_capsule(
    std::vector<std::string> capsule_command, std::set<std::string> options
){
    gw_retval_t retval = GW_SUCCESS;
    std::string env_value = "";
    std::string host_ip = "";
    std::string log_path = "";
    uint16_t scheduler_port = 0;

    // obtain the ip address of the scheduler
    retval = GWUtilSystem::get_env_variable("GW_SCHEDULER_IPV4", env_value);
    if(likely(retval == GW_SUCCESS)){
        host_ip = env_value;
    } else {
        host_ip = GWUtilSocket::get_local_ip();
    }

    // obtain the port of the scheduler
    retval = GWUtilSystem::get_env_variable("GW_SCHEDULER_SERVE_WS_PORT", env_value);
    if(likely(retval == GW_SUCCESS)){
        scheduler_port = GWUtilString::string_to_uint16(env_value);
    } else {
        scheduler_port = GW_SCHEDULER_SERVE_WS_PORT;
    }

    // make the command to start main capsule
    log_path = this->_config.COMMON_log_path;
    GW_CHECK_POINTER(this->_main_capsule_launch_meta = new gw_process_launch_meta_t);
    capsule_command.insert(capsule_command.begin(), "GW_SCHEDULER_IPV4=" + host_ip);
    capsule_command.insert(capsule_command.begin(), "GW_SCHEDULER_SERVE_WS_PORT=" + std::to_string(scheduler_port));
    capsule_command.insert(capsule_command.begin(), "GW_LOG_PATH=" + log_path);
    capsule_command.insert(capsule_command.begin(), "OMPI_MCA_orte_propagate_enviro=1");
    capsule_command.insert(capsule_command.begin(), "LD_PRELOAD=libgwatch_capsule_hijack.so");
    #if GW_BACKEND_CUDA
        if(options.find("enable_cuda_coredump") != options.end()){
            capsule_command.insert(capsule_command.begin(), "GW_ENABLE_COREDUMP=1");
        }
    #endif
    GW_DEBUG_C(
        "start capsule with command: %s",
        GWUtilString::vector_to_string<std::string>(capsule_command, false, " ").c_str()
    );

    // launch main capsule
    this->_main_capsule_launch_meta->launch_cmd = capsule_command;
    GW_IF_FAILED(
        GWUtilCommandCaller::exec_async(
            this->_main_capsule_launch_meta->launch_cmd,
            this->_main_capsule_launch_meta->async_thread,
            this->_main_capsule_launch_meta->thread_promise,
            &this->_main_capsule_launch_meta->pipe,
            this->_main_capsule_launch_meta->pid,
            this->_main_capsule_launch_meta->cmd_output,
            /* ignore_error */ true,
            /* print_stdout */ false,
            /* print_stderr */ false
        ),
        retval,
        {
            GW_WARN_C(
                "failed to launch main capsule, cmd(%s)",
                GWUtilString::vector_to_string<std::string>(
                    /* data */ this->_main_capsule_launch_meta->launch_cmd,
                    /* spliter*/ " "
                ).c_str()
            );
            goto exit;
        }
    );

    GW_DEBUG_C("launched main capsule process: pid(%d)", this->_main_capsule_launch_meta->pid);

exit:
    return retval;
}


gw_retval_t GWScheduler::start_gtrace(){
    gw_retval_t retval = GW_SUCCESS;
    std::string log_path = "";

    // formup launch command
    // TODO(zhuobin): change dev / product mode automatically
    log_path = this->_config.COMMON_log_path;
    GW_CHECK_POINTER(this->_gtrace_launch_meta = new gw_process_launch_meta_t);
    this->_gtrace_launch_meta->launch_cmd = {
        "GF_DEFAULT_APP_MODE=development",
        "grafana-server",
        "--config=/etc/grafana/grafana.ini",
        "--homepath=/usr/share/grafana",
        std::format(">>{}", log_path + "/gtrace.log"),
        "2>&1"
    };

    // launch gtrace
    GW_IF_FAILED(
        GWUtilCommandCaller::exec_async(
            this->_gtrace_launch_meta->launch_cmd,
            this->_gtrace_launch_meta->async_thread,
            this->_gtrace_launch_meta->thread_promise,
            &this->_gtrace_launch_meta->pipe,
            this->_gtrace_launch_meta->pid,
            this->_gtrace_launch_meta->cmd_output,
            /* ignore_error */ true,
            /* print_stdout */ false,
            /* print_stderr */ false
        ),
        retval,
        {
            GW_WARN_C(
                "failed to launch gtrace: cmd(%s)",
                GWUtilString::vector_to_string<std::string>(
                    /* data */ this->_gtrace_launch_meta->launch_cmd,
                    /* spliter*/ " "
                ).c_str()
            );
            goto exit;
        }
    );

    GW_DEBUG_C("launched gtrace process: pid(%d)", this->_gtrace_launch_meta->pid);

exit:
    return retval;
}


gw_retval_t GWScheduler::start_profiler(std::string ip_addr){
    gw_retval_t retval = GW_SUCCESS;
    std::vector<std::string> launch_command;
    std::string env_value;
    std::string host_ip;
    uint16_t scheduler_port;
    gw_process_launch_meta_t *launch_meta;

    // TODO(zhuobin): we should support remote ssh in the future
    GW_ASSERT(ip_addr == "0.0.0.0");
    
    // avoid duplication
    if(unlikely(this->_map_ip_to_launch_meta.find(ip_addr) != this->_map_ip_to_launch_meta.end())){
        GW_WARN_C("profiler is already started on ip(%s)", ip_addr.c_str());
        retval = GW_FAILED_ALREADY_EXIST;
        goto exit;
    }

    // obtain the ip address of the scheduler
    retval = GWUtilSystem::get_env_variable("GW_SCHEDULER_IPV4", env_value);
    if(likely(retval == GW_SUCCESS)){
        host_ip = env_value;
    } else {
        host_ip = GWUtilSocket::get_local_ip();
    }

    // obtain the port of the scheduler
    retval = GWUtilSystem::get_env_variable("GW_SCHEDULER_SERVE_WS_PORT", env_value);
    if(likely(retval == GW_SUCCESS)){
        scheduler_port = GWUtilString::string_to_uint16(env_value);
    } else {
        scheduler_port = GW_SCHEDULER_SERVE_WS_PORT;
    }

    // makeup launch command
    launch_command.insert(launch_command.begin(), "gwatch_profiler_exe");
    launch_command.insert(launch_command.begin(), "GW_SCHEDULER_IPV4=" + host_ip);
    launch_command.insert(launch_command.begin(), "GW_SCHEDULER_SERVE_WS_PORT=" + std::to_string(scheduler_port));

    // launch the profiler
    GW_CHECK_POINTER(launch_meta = new gw_process_launch_meta_t);
    launch_meta->launch_cmd = launch_command;
    GW_IF_FAILED(
        GWUtilCommandCaller::exec_async(
            launch_meta->launch_cmd,
            launch_meta->async_thread,
            launch_meta->thread_promise,
            &launch_meta->pipe,
            launch_meta->pid,
            launch_meta->cmd_output,
            /* ignore_error */ true,
            /* print_stdout */ false,
            /* print_stderr */ false
        ),
        retval,
        {
            GW_WARN_C("failed to launch main capsule");
            goto exit;
        }
    );
    this->_map_ip_to_launch_meta[ip_addr] = launch_meta;

exit:
    return retval;
}


gw_retval_t GWScheduler::__create_new_capsule_instance(struct lws *wsi, GWCapsuleInstance **capsule_instance){
    gw_retval_t retval = GW_SUCCESS;

    GW_CHECK_POINTER(capsule_instance);
    GW_CHECK_POINTER(*capsule_instance = new GWCapsuleInstance(wsi, &this->_is_daemon_stop));

    return retval;
}


gw_retval_t GWScheduler::__destory_capsule_instance(GWCapsuleInstance *capsule_instance){
    gw_retval_t retval = GW_SUCCESS;

    if(capsule_instance != nullptr)
        GW_CHECK_POINTER(delete capsule_instance);

    return retval;
}


gw_retval_t GWScheduler::__create_new_gtrace_instance(
    struct lws *wsi, GWgTraceInstance **gtrace_instance
){
    gw_retval_t retval = GW_SUCCESS;

    GW_CHECK_POINTER(gtrace_instance);
    GW_CHECK_POINTER(*gtrace_instance = new GWgTraceInstance(wsi, &this->_is_daemon_stop));

    return retval;
}


gw_retval_t GWScheduler::__destory_gtrace_instance(GWgTraceInstance *gtrace_instance){
    gw_retval_t retval = GW_SUCCESS;

    GW_CHECK_POINTER(gtrace_instance);

    if(gtrace_instance != nullptr)
        delete gtrace_instance;

    return retval;
}


gw_retval_t GWScheduler::__create_new_profiler_instance(struct lws *wsi, GWProfilerInstance **profiler_instance){
    gw_retval_t retval = GW_SUCCESS;

    GW_CHECK_POINTER(profiler_instance);
    GW_CHECK_POINTER(*profiler_instance = new GWProfilerInstance(wsi, &this->_is_daemon_stop));

    return retval;
}


gw_retval_t GWScheduler::__destroy_profiler_instance(GWProfilerInstance *profiler_instance){
    gw_retval_t retval = GW_SUCCESS;
    GW_CHECK_POINTER(profiler_instance);

    if(profiler_instance != nullptr)
        delete profiler_instance;

    return retval;
}


gw_retval_t GWScheduler::__start_websocket_daemon(gw_websocket_desp_t &ws_desp_){
    gw_retval_t retval = GW_SUCCESS;
    std::string env_value = "";
    struct lws_context_creation_info ws_context_create_info = {0};
    gw_websocket_desp_t *ws_desp;

    struct lws_protocols __protocols[2] = {
        {
            .name = "gwatch-websocket-protocol",
            .callback = ws_desp_.wb_callback,
            .per_session_data_size = sizeof(void*),
            .rx_buffer_size = GW_WEBSOCKET_MAX_MESSAGE_LENGTH,
            .tx_packet_size = 0 // same as rx_buffer_size
        },
        {NULL, NULL, 0, 0} // end of list
    };

    GW_CHECK_POINTER(ws_desp_.wb_callback);
    GW_CHECK_POINTER(ws_desp_.wb_serve_func);

    // we shift the value to a heap-located valuable since websocket need to hold
    // fields such as list_wb_protocols during executions
    GW_CHECK_POINTER(ws_desp = new gw_websocket_desp_t(ws_desp_));
    ws_desp->list_wb_protocols.clear();
    ws_desp->list_wb_protocols.push_back(__protocols[0]);
    ws_desp->list_wb_protocols.push_back(__protocols[1]);

    memset(&ws_context_create_info, 0, sizeof(ws_context_create_info));
    ws_context_create_info.port = ws_desp->port;
    ws_context_create_info.protocols = (struct lws_protocols*)(ws_desp->list_wb_protocols.data());
    ws_context_create_info.user = this;

    if(unlikely(nullptr == (ws_desp->wb_context = lws_create_context(&ws_context_create_info)))){
        GW_WARN_C("failed to create websocket context");
        retval = GW_FAILED;
        goto exit;
    }

    // lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG | LLL_PARSER, NULL);

    ws_desp->serve_thread = new std::thread(&GWScheduler::__websocket_serve_func, this, ws_desp);
    GW_CHECK_POINTER(ws_desp->serve_thread);

    this->_list_webseocket_desps.push_back(ws_desp);

    // return the value back to caller
    ws_desp_ = *ws_desp;

 exit:
    return retval;
}


gw_retval_t GWScheduler::__shutdown_all_websocket_daemons(){
    gw_retval_t retval = GW_SUCCESS;

    this->_is_daemon_stop = true;

    for(auto ws_desp: this->_list_webseocket_desps){
        GW_CHECK_POINTER(ws_desp);
        GW_CHECK_POINTER(ws_desp->serve_thread);
        GW_CHECK_POINTER(ws_desp->wb_context);

        if(likely(ws_desp->serve_thread->joinable()))
            ws_desp->serve_thread->join();
        delete ws_desp->serve_thread;
    
        lws_context_destroy(ws_desp->wb_context);
    
        delete ws_desp;
    }

    return retval;
}


void GWScheduler::__websocket_serve_func(GWScheduler* scheduler, gw_websocket_desp_t* ws_desp){
    GW_CHECK_POINTER(ws_desp);

    GW_DEBUG("websocket serve daemon started");
    while(!scheduler->_is_daemon_stop){
        lws_service(ws_desp->wb_context, -1);
    }
    GW_DEBUG("websocket serve daemon stopped");
}


int GWScheduler::__websocket_callback(
    struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len
){
    int retval = 0, sdk_retval = 0;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWCapsuleInstance *capsule_instance = nullptr;
    GWgTraceInstance *gtrace_instance = nullptr;
    GWProfilerInstance *profiler_instance = nullptr;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN] = { 0 };
    uint16_t client_port;
    GWScheduler *scheduler = nullptr;
    char *recv_buf = nullptr;
    GWInternalMessage_Capsule capsule_message;
    GWInternalMessage_gTrace gtrace_message;
    GWInternalMessage_Profiler profiler_message;
    std::string wsi_uri;
    char _wsi_uri[256] = {0};
    int uri_len = 0;

    GW_CHECK_POINTER(wsi);
    GW_CHECK_POINTER(in);
    GW_CHECK_POINTER(scheduler = static_cast<GWScheduler*>(lws_context_user(lws_get_context(wsi))));

    // obtain client address
    getpeername(lws_get_socket_fd(wsi), (struct sockaddr*)&client_addr, &client_addr_len);
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    client_port = ntohs(client_addr.sin_port);

    gw_socket_addr_t client_socket_addr(client_addr);

    // check reason at https://libwebsockets.org/lws-api-doc-main/html/lws-callbacks_8h_source.html
    switch (reason)
    {
    case LWS_CALLBACK_ESTABLISHED:
        lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);

        // obtain URI of the connection
        uri_len = lws_hdr_copy(wsi, _wsi_uri, sizeof(_wsi_uri), WSI_TOKEN_GET_URI);
        if(unlikely(uri_len < 0)){
            GW_WARN("failed to obtain URI of the connection");
            retval = -1;
            goto exit;
        }
        _wsi_uri[uri_len] = '\0';
        wsi_uri = _wsi_uri;

        if(wsi_uri == "/capsule/main"){
            GW_DEBUG("capsule websocket connected: wsi(%p), ip(%s), port(%u)", wsi, client_ip, client_port);
            GW_IF_FAILED(
                scheduler->__create_new_capsule_instance(wsi, &capsule_instance),
                sdk_retval,
                {
                    GW_WARN("failed to create new capsule instance");
                    retval = -1;
                    goto exit;
                }
            );
            GW_CHECK_POINTER(capsule_instance);
            scheduler->_map_capsule[wsi] = capsule_instance;
            GW_DEBUG("created new capsule instance(%p)", capsule_instance);

            // start profiler on node with unseen ip address
            if(scheduler->_set_all_active_capsule_ip.count(client_ip) == 0){
                GW_IF_FAILED(
                    scheduler->start_profiler(client_ip),
                    sdk_retval,
                    {
                        GW_WARN("failed to start profiler for ip(%s)", client_ip);
                        retval = -1;
                        goto exit;
                    }
                );
                scheduler->_set_all_active_capsule_ip.insert(client_ip);
            }

            // send REPORT_TOPO request
            capsule_message.type_id = GW_MESSAGE_TYPEID_CAPSULE_REGISTER;
            GW_IF_FAILED(
                capsule_instance->send(capsule_message.serialize()),
                sdk_retval,
                {
                    GW_WARN("failed to send register request to capsule");
                    retval = -1;
                    goto exit;
                }
            );
        } else if(wsi_uri == "/gtrace/main"){
            GW_DEBUG("gTrace websocket connected: wsi(%p), ip(%s), port(%u)", wsi, client_ip, client_port);
            GW_IF_FAILED(
                scheduler->__create_new_gtrace_instance(wsi, &gtrace_instance),
                sdk_retval,
                {
                    GW_WARN("failed to create new gtrace instance");
                    retval = -1;
                    goto exit;
                }
            );
            GW_CHECK_POINTER(gtrace_instance);
            scheduler->_map_gtrace[wsi] = gtrace_instance;
            GW_DEBUG("created new gtrace instance(%p)", gtrace_instance);
        } else if(wsi_uri == "/profiler/main"){
            GW_DEBUG("profiler websocket connected: wsi(%p), ip(%s), port(%u)", wsi, client_ip, client_port);
            GW_IF_FAILED(
                scheduler->__create_new_profiler_instance(wsi, &profiler_instance),
                sdk_retval,
                {
                    GW_WARN("failed to create new profiler instance");
                    retval = -1;
                    goto exit;
                }
            );
            GW_CHECK_POINTER(profiler_instance);
            scheduler->_map_profiler[wsi] = profiler_instance;
            scheduler->_map_ip_to_profiler[client_ip] = profiler_instance;
            GW_DEBUG("created new profiler instance(%p)", profiler_instance);
        } else {
            GW_WARN("unknown connection URI, ignored: URI(%s)", wsi_uri.c_str());
        }
        break;

    case LWS_CALLBACK_CLOSED:
        if(scheduler->_map_capsule.count(wsi) > 0){
            GW_DEBUG("capsule websocket disconnected: wsi(%p), ip(%s), port(%u)", wsi, client_ip, client_port);
            GW_CHECK_POINTER(capsule_instance = scheduler->_map_capsule[wsi]);

            // TODO(zhuobin): update SQL

            if(scheduler->_set_all_active_capsule_ip.count(client_ip) > 0){
                scheduler->_set_all_active_capsule_ip.erase(client_ip);
            }
            scheduler->_map_disconnected_capsule[wsi] = capsule_instance;
            scheduler->_map_capsule.erase(wsi);
        } else if(scheduler->_map_gtrace.count(wsi) > 0){
            GW_DEBUG("gtrace websocket disconnected: wsi(%p), ip(%s), port(%u)", wsi, client_ip, client_port);
            GW_CHECK_POINTER(gtrace_instance = scheduler->_map_gtrace[wsi]);
            GW_IF_FAILED(
                scheduler->__destory_gtrace_instance(gtrace_instance),
                sdk_retval,
                GW_WARN("failed to destory gtrace instance")
            );
            scheduler->_map_gtrace.erase(wsi);
        } else if(scheduler->_map_profiler.count(wsi) > 0){
            GW_DEBUG("profiler websocket disconnected: wsi(%p), ip(%s), port(%u)", wsi, client_ip, client_port);
            GW_CHECK_POINTER(profiler_instance = scheduler->_map_profiler[wsi]);
            GW_IF_FAILED(
                scheduler->__destroy_profiler_instance(profiler_instance),
                sdk_retval,
                GW_WARN("failed to destory profiler instance")
            );
            scheduler->_map_profiler.erase(wsi);
            scheduler->_map_ip_to_profiler.erase(client_ip);
        } else {
            GW_WARN("disconnected from unknown remote, ignored: ip(%s), port(%u)", client_ip, client_port);
        }
        break;
    
    case LWS_CALLBACK_RECEIVE:
        if(scheduler->_map_capsule.count(wsi) > 0){
            GW_CHECK_POINTER(capsule_instance = scheduler->_map_capsule[wsi]);
            GW_IF_FAILED(
                capsule_instance->recv_to_buf(in, len),
                sdk_retval,
                {
                    GW_WARN("failed to record received message to capsule instance");
                    retval = -1;
                    goto exit;
                }
            );
            if(lws_is_final_fragment(wsi)){
                recv_buf = capsule_instance->export_recv_buf();
                GW_DEBUG("received message from capsule: wsi(%p), ip(%s), port(%u), message(%s)", wsi, client_ip, client_port, recv_buf);

                capsule_message.deserialize(recv_buf);
                capsule_instance->reset_recv_buf();

                GW_IF_FAILED(
                    scheduler->__process_capsule_resp(capsule_instance, client_socket_addr, &capsule_message),
                    sdk_retval,
                    GW_WARN("failed to process capsule message")
                );
            }
        } else if(scheduler->_map_gtrace.count(wsi) > 0){
            GW_CHECK_POINTER(gtrace_instance = scheduler->_map_gtrace[wsi]);
            GW_IF_FAILED(
                gtrace_instance->recv_to_buf(in, len),
                sdk_retval,
                {
                    GW_WARN("failed to record received message to gTrace instance");
                    retval = -1;
                    goto exit;
                }
            );
            if(lws_is_final_fragment(wsi)){
                recv_buf = gtrace_instance->export_recv_buf();
                GW_DEBUG("received message from gtrace: wsi(%p), ip(%s), port(%u), message(%s)", wsi, client_ip, client_port, recv_buf);

                gtrace_message.deserialize(recv_buf);
                gtrace_instance->reset_recv_buf();

                GW_IF_FAILED(
                    scheduler->__process_gtrace_req(gtrace_instance, client_socket_addr, &gtrace_message),
                    sdk_retval,
                    GW_WARN("failed to process gtrace message")
                );
            }
        } else if(scheduler->_map_profiler.count(wsi) > 0){
            GW_CHECK_POINTER(profiler_instance = scheduler->_map_profiler[wsi]);
            GW_IF_FAILED(
                profiler_instance->recv_to_buf(in, len),
                sdk_retval,
                {
                    GW_WARN("failed to record received message to profiler instance");
                    retval = -1;
                    goto exit;
                }
            );
            if(lws_is_final_fragment(wsi)){
                recv_buf = profiler_instance->export_recv_buf();
                GW_DEBUG("received message from profiler: wsi(%p), ip(%s), port(%u), message(%s)", wsi, client_ip, client_port, recv_buf);

                profiler_message.deserialize(recv_buf);
                profiler_instance->reset_recv_buf();

                // GW_IF_FAILED(
                //     scheduler->__process_gtrace_req(profiler_instance, client_socket_addr, &gtrace_message),
                //     sdk_retval,
                //     GW_WARN("failed to process gtrace message")
                // );
            }
        } else {
            GW_WARN("received message from unknown remote, ignored: ip(%s), port(%u)", client_ip, client_port);
        }
        break;

    case LWS_CALLBACK_SERVER_WRITEABLE:
        if(scheduler->_map_capsule.count(wsi) > 0){
            GW_CHECK_POINTER(capsule_instance = scheduler->_map_capsule[wsi]);
            GW_IF_FAILED(
                GWUtilWebSocketInstance::wsi_send_chunk(
                    reinterpret_cast<GWUtilWebSocketInstance*>(capsule_instance)
                ),
                gw_retval,
                {
                    retval = -1;
                    GW_WARN("failed to send out websocket to capsule");
                }
            );
            break;
        } else if(scheduler->_map_gtrace.count(wsi) > 0){
            GW_CHECK_POINTER(gtrace_instance = scheduler->_map_gtrace[wsi]);
            GW_IF_FAILED(
                GWUtilWebSocketInstance::wsi_send_chunk(
                    reinterpret_cast<GWUtilWebSocketInstance*>(gtrace_instance)
                ),
                gw_retval,
                {
                    retval = -1;
                    GW_WARN("failed to send out websocket to gtrace");
                }
            );
            break;
        } else if(scheduler->_map_profiler.count(wsi) > 0){
            GW_CHECK_POINTER(profiler_instance = scheduler->_map_profiler[wsi]);
            GW_IF_FAILED(
                GWUtilWebSocketInstance::wsi_send_chunk(
                    reinterpret_cast<GWUtilWebSocketInstance*>(profiler_instance)
                ),
                gw_retval,
                {
                    retval = -1;
                    GW_WARN("failed to send out websocket to profiler");
                }
            );
            break;
        }

    default:
        break;
    }

exit:
    return retval;
}
