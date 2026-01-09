#pragma once

#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <set>
#include <queue>
#include <cstdio>
#include <array>
#include <string>
#include <thread>
#include <future>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <libwebsockets.h>
#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/socket.hpp"
#include "common/utils/database_kv.hpp"
#include "common/utils/database_sql.hpp"
#include "common/utils/database_timeseries.hpp"
#include "scheduler/serve/capsule_message.hpp"
#include "scheduler/serve/capsule_instance.hpp"
#include "scheduler/serve/gtrace_message.hpp"
#include "scheduler/serve/gtrace_instance.hpp"
#include "scheduler/serve/profiler_message.hpp"
#include "scheduler/serve/profiler_instance.hpp"
#include "scheduler/agent/context.hpp"


typedef struct gw_process_launch_meta {
    std::vector<std::string> launch_cmd;
    std::string cmd_output = "";
    std::thread async_thread;
    std::promise<gw_retval_t> thread_promise;
    pid_t pid;
    FILE *pipe = nullptr;
} gw_process_launch_meta_t;


typedef struct {
    // common configuration
    std::string COMMON_job_name = "unknown_job";
    std::string COMMON_python_package_installtion_path = "";
    std::string COMMON_log_path = GW_DEFAULT_LOG_PATH;
    bool        COMMON_enable_visualize = true;

    // agent configuration
    bool        AGENT_enable = false;
    std::string AGENT_workdir = "$PWD";
} gw_scheduler_config_t;


class GWScheduler {
    /* ===================== Common ====================== */
 public:
    GWScheduler(gw_scheduler_config_t config);
    ~GWScheduler();

    /*!
     *  \brief  start schduler serving daemon
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t serve();


 protected:
    gw_scheduler_config_t _config;

    // flag to control the daemon to stop
    volatile bool _is_daemon_stop;
    /* ===================== Common ====================== */


    /* ===================== Database ====================== */
 protected:
    // kv database for storing the capsule information, watch, etc.
    GWUtilKVDatabase _db_kv;
    
    // time-series database for storing monitoring data
    GWUtilTimeSeriesDatabase _db_ts;

    // SQL database
    GWUtilSQLDatabase _db_sql;

    /*!
     *  \brief  initialize the database
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t __init_db();
    gw_retval_t __init_sql();
    gw_retval_t __init_vector();
    /* ===================== Database ====================== */


    /* ===================== Capsule / gTrace / Profiler Management ====================== */
    // ===================== Capsule =====================
 public:
    /*!
     *  \brief  obtain number of capsules
     *  \return number of capsules
     */
    inline uint64_t get_capsule_world_size(){ return this->_map_capsule.size(); }


    /*!
     *  \brief  start the execution of the main capsule
     *  \param  capsule_command     the command to launch the capsule
     *  \param  options             options to start the capsule
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t start_capsule(std::vector<std::string> capsule_command, std::set<std::string> options);


 protected:
    /*!
     *  \brief  create a new capsule instance
     *  \param  capsule_instance the capsule instance to be created
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t __create_new_capsule_instance(struct lws *wsi, GWCapsuleInstance **capsule_instance);


    /*!
     *  \brief  destory a capsule instance
     *  \param  capsule_instance the capsule instance to be destoryed
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t __destory_capsule_instance(GWCapsuleInstance *capsule_instance);


    // launch metadata of main capsule
    gw_process_launch_meta_t *_main_capsule_launch_meta = nullptr;

    // map of capsule instances
    std::map<struct lws*, GWCapsuleInstance*> _map_capsule;
    std::map<struct lws*, GWCapsuleInstance*> _map_disconnected_capsule;
    std::map<std::string, GWCapsuleInstance*> _map_global_id_to_capsule;

    // processing thread of per-capsule
    std::map<GWCapsuleInstance*, std::thread*> _map_capsule_processing_thread;

    // set of ip addresses of all active capsule 
    std::set<std::string> _set_all_active_capsule_ip;

    // map from gpu global id to gpu info
    std::map<std::string, gw_capsule_gpu_info_t> map_gpu_info;
 

    // ===================== gTrace =====================
 public:
    /*!
     *  \brief  start the execution of the gtrace
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t start_gtrace();


 protected:
    /*!
     *  \brief  create a new gtrace instance
     *  \param  wsi             the websocket instance
     *  \param  gtrace_instance the gtrace instance to be created
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t __create_new_gtrace_instance(struct lws *wsi, GWgTraceInstance **gtrace_instance);


    /*!
     *  \brief  destory a gtrace instance
     *  \param  gtrace_instance the gtrace instance to be destoryed
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t __destory_gtrace_instance(GWgTraceInstance *gtrace_instance);


    /*!
     *  \brief  callback function triggered a write to ts db, subscribe by gTrace
     *  \param  uri         the uri of the db
     *  \param  new_val     the new value of the db
     *  \param  old_val     the old value of the db
     *  \param  user_data   the user data
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    static gw_retval_t __cb_gtrace_subcribe_ts_db(
        gw_db_subscribe_context_t* sub_cxt, const nlohmann::json& new_val, const nlohmann::json& old_val
    );
    // typedef struct gtrace_subscribe_ts_db_param {
    //     int sth;
    // } gtrace_subscribe_ts_db_param_t;


    /*!
     *  \brief  init function when subscribe to a ts db resource by gTrace
     *  \param  db          the database instance
     *  \param  sub_cxt     the subscribe context
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    static gw_retval_t __init_gtrace_subcribe_ts_db(GWUtilDatabase* db, gw_db_subscribe_context_t* sub_cxt);


    /*!
     *  \brief  callback function triggered a write to SQL db, subscribe by gTrace
     *  \param  uri         the table name of the db
     *  \param  new_val     the new value of the db
     *  \param  old_val     the old value of the db
     *  \param  user_data   the user data
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    static gw_retval_t __cb_gtrace_subcribe_sql_db(
        gw_db_subscribe_context_t* sub_cxt, const nlohmann::json& new_val, const nlohmann::json& old_val
    );
    // typedef struct gtrace_subscribe_sql_db_param {
    //     int sth;
    // } gtrace_subscribe_sql_db_param_t;


    /*!
     *  \brief  init function when subscribe to a SQL db resource by gTrace
     *  \param  db          the database instance
     *  \param  sub_cxt     the subscribe context
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    static gw_retval_t __init_gtrace_subcribe_sql_db(GWUtilDatabase* db, gw_db_subscribe_context_t* sub_cxt);


    // launch metadata of gtrace
    gw_process_launch_meta_t *_gtrace_launch_meta = nullptr;

    // set of gtrace instances
    std::map<struct lws*, GWgTraceInstance*> _map_gtrace;

    // processing thread of per-gTrace
    std::map<GWgTraceInstance*, std::thread*> _map_gtrace_processing_thread;


    // ===================== Profiler =====================
 public:
    /*!
     *  \brief  start the execution of the profiler on a unseen machine
     *  \param  ip_addr     ip address of the unseen machine
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t start_profiler(std::string ip_addr = "0.0.0.0");


 protected:
    /*!
     *  \brief  create a new profiler instance
     *  \param  profiler_instance the profiler instance to be created
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t __create_new_profiler_instance(struct lws *wsi, GWProfilerInstance **profiler_instance);


    /*!
     *  \brief  destory a profiler instance
     *  \param  profiler_instance the profiler instance to be destoryed
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t __destroy_profiler_instance(GWProfilerInstance *profiler_instance);
    

    /*!
     *  \brief  callback function triggered when a profiler write to the db
     *  \param  uri         the uri of the db
     *  \param  new_val     the new value of the db
     *  \param  old_val     the old value of the db
     *  \param  user_data   the user data
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    static gw_retval_t __cb_profiler_write_db(
        const std::string& uri, const nlohmann::json& new_val, const nlohmann::json& old_val, void* user_data
    );


    // map of profiler instances
    std::map<struct lws*, GWProfilerInstance*> _map_profiler;
    std::map<std::string, GWProfilerInstance*> _map_ip_to_profiler;
    std::map<std::string, gw_process_launch_meta_t *> _map_ip_to_launch_meta;
    /* ===================== Capsule / gTrace / Profiler Management ====================== */


    /* ======================== Communication w/ Capsule ======================== */
 protected:
    /*!
     *  \brief  process the capsule response message
     *  \param  capsule_instance    the capsule instance that sends the message
     *  \param  addr                the address of the capsule
     *  \param  message             the message
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t __process_capsule_resp(
        GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
    );
    gw_retval_t __process_capsule_resp_PINGPONG(
        GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
    );
    gw_retval_t __process_capsule_resp_DB_KV_WRITE(
        GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
    );
    gw_retval_t __process_capsule_resp_DB_TS_WRITE(
        GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
    );
    gw_retval_t __process_capsule_resp_DB_SQL_CREATE_TABLE(
        GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
    );
    gw_retval_t __process_capsule_resp_DB_SQL_WRITE(
        GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
    );
    gw_retval_t __process_capsule_resp_REGISTER(
        GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
    );
    /* ======================== Communication w/ Capsule ======================== */


    /* ======================== Communication w/ gTrace ======================== */
 protected:
    /*!
     *  \brief  process the gtrace request message
     *  \param  gtrace_instance     the gtrace instance that sends the message
     *  \param  addr                the address of the capsule
     *  \param  message             the message
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t __process_gtrace_req(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_PINGPONG(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_READ_KV_DB(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_WRITE_KV_DB(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_SUBSCRIBE_TS_DB(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_UNSUBSCRIBE_TS_DB(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_READ_SQL_DB(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_SUBSCRIBE_SQL_DB(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_UNSUBSCRIBE_SQL_DB(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_AGENT_CREATE_CONTEXT(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_AGENT_DESTORY_CONTEXT(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_AGENT_CREATE_TASK(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_AGENT_DESTORY_TASK(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    gw_retval_t __process_gtrace_req_AGENT_CREATE_CONTENT(
        GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
    );
    /* ======================== Communication w/ gTrace ======================== */


    /* ======================== Communication Common ======================== */
    /*!
     *  \brief  callback function of websocket event for capsule/gTrace/profiler communication
     *  \param  wsi     websocket instance
     *  \param  reason  reason for the callback
     *  \param  user    user data
     *  \param  in      input data
     *  \param  len     length of the input data
     *  \return 0 for successful callback, -1 for failed callback
     */
    static int __websocket_callback(
        struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len
    );

    /*!
     *  \brief  start the websocket daemon (for gtrace and capsule communication)
     *  \param  ws_desp the websocket despcription
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t __start_websocket_daemon(gw_websocket_desp_t &ws_desp_);

    /*!
     *  \brief  shutdown all websocket daemons (for gtrace and capsule communication)
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t __shutdown_all_websocket_daemons();

    /*!
     *  \brief  serve function for websocket daemon
     *  \param  ws_desp the websocket despcription
     */
    static void __websocket_serve_func(GWScheduler* scheduler, gw_websocket_desp_t *ws_desp_);

    // list of descriptors for created websocket
    std::vector<gw_websocket_desp_t*> _list_webseocket_desps;
    /* ======================== Communication Common ======================== */


    /* ======================== Agent ======================== */
 public:

 protected:
    friend class GWAgentContext;
    std::map<std::string, GWAgentContext*> _map_active_context = {};
    std::map<std::string, GWAgentContext*> _map_context = {};
    /* ======================== Agent ======================== */


    /* ======================== Code Parsing ======================== */
 public:
    /*!
     *  \brief  parsing the codebase async
     *  \param  dir     the directory of the codebase
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t parse_codebase_async(std::string dir);


 protected:
    // thread for parsing codebase asynchronously
    std::thread* _parse_codebase_thread = nullptr;

    // identify whether the parsing process is preemptive
    bool _is_parse_codebase_preemptive = false;

    // parse the codebase
    static gw_retval_t __parse_codebase_func(std::string dir);
    /* ======================== Code Parsing ======================== */
};
