#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <mutex>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/timer.hpp"
#include "common/utils/socket.hpp"
#include "scheduler/serve/profiler_message.hpp"
#include "profiler/profiler.hpp"
#include "profiler/profile_image.hpp"

class GWProfileDevice;
class GWProfiler;


/*!
 *  \brief  GWProfileContext entrypoint
 */
class GWProfileContext {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  (de)constructor
     */
    GWProfileContext(bool lazy_init_device = true);
    ~GWProfileContext();


    /*!
     *  \brief  obtain all device under this profiler context
     *  \return map from device id to device
     */
    std::map<int, const GWProfileDevice*> get_devices() const {
        std::map<int, const GWProfileDevice*> retval_map;
        for(auto iter = this->_gw_devices.begin(); iter != this->_gw_devices.end(); iter++){
            retval_map[iter->first] = reinterpret_cast<const GWProfileDevice*>(iter->second);
        }
        return retval_map; 
    }


    /*!
     *  \brief  runnning the serving thread of profiler
     *  \note   this function is only called in serperated capsule mode
     *  \return GW_SUCCESS for successfully initialized
     */
    gw_retval_t serve();


    // global index of the profiler context (i.e., node ip address)
    std::string global_id;


 protected:
    /*!
     *  \brief  init speficied device under this profiler context
     *  \param  device_id       index of the device to be profiled
     *  \return GW_SUCCESS for successfully initialized
     */
    virtual gw_retval_t __init_gw_device(int device_id) = 0;

    // path of the log file
    std::string _log_file_path;

    // devices
    int _num_device;

    // map from device id to device
    std::map<int, GWProfileDevice*> _gw_devices;

    // global tsc timer
    GWUtilTscTimer *_tsc_timer;
    /* ==================== Common ==================== */


    /* ==================== Profiling ==================== */
 public:
    inline void lock_device(int device_id){
        std::lock_guard<std::mutex> lock(this->_device_mutex_map_mutex);
        if(unlikely(this->_device_mutex_map.find(device_id) == this->_device_mutex_map.end())){
            this->_device_mutex_map[device_id];
        }
        this->_device_mutex_map[device_id].lock();
        GW_DEBUG_C(
            "locked device profiling lock: device_id(%d)",
            device_id
        )
    }


    inline void unlock_device(int device_id){
        std::lock_guard<std::mutex> lock(this->_device_mutex_map_mutex);
        if(unlikely(this->_device_mutex_map.find(device_id) == this->_device_mutex_map.end())){
            GW_WARN_C_DETAIL(
                "failed to unlock device profiling lock, no device lock founded: device_id(%d)",
                device_id
            );
        }
        this->_device_mutex_map[device_id].unlock();
        GW_DEBUG_C(
            "unlocked device profiling lock: device_id(%d)",
            device_id
        )
    }


    /*!
     *  \brief  create a new profiler for profiling specified metrics on specified device
     *  \note   this API would create all necessary images for profiling
     *  \param  device_id       index of the device to be profiled
     *  \param  metric_names    vector of metrics to be profiled
     *  \param  profiler_mode   mode of the profiler
     *  \param  gw_profiler     the created profiler
     */
    virtual gw_retval_t create_profiler(
        int device_id,
        const std::vector<std::string>& metric_names,
        gw_profiler_mode_t profiler_mode,
        GWProfiler** gw_profiler
    ){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  destory profiler created by create_profiler
     *  \param  gw_profiler    the profiler to be destoryed
     */
    virtual void destroy_profiler(GWProfiler* gw_profiler){
        GW_ERROR_C("not implemented");
    }


 protected:
    // profiling lock on each device
    std::map<int, std::mutex> _device_mutex_map;

    // mutex for _device_mutex_map
    std::mutex _device_mutex_map_mutex;
    /* ==================== Profiling ==================== */


    /* ======================== Communication w/ Scheduler ======================== */
 public:
    /*!
     *  \brief  send message to the scheduler
     *  \return GW_SUCCESS if success,
     *          GW_FAILED_NOT_READY for no websocket connection is built
     */
    gw_retval_t send_to_scheduler(GWInternalMessage_Profiler *message);


 private:
    /*!
     *  \brief  process the scheduler message: SET_METRICS
     *  \param  message     the received message
     */
    void __process_SET_METRICS(GWInternalMessage_Profiler *message);


    /*!
     *  \brief  process the scheduler message: BEGIN
     *  \param  message     the received message
     */
    void __process_BEGIN(GWInternalMessage_Profiler *message);


    /*!
     *  \brief  process the scheduler message: END
     *  \param  message     the received message
     */
    void __process_END(GWInternalMessage_Profiler *message);


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
     *  \brief  daemon thread of the context
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
    GWUtilWebSocketInstance *_ws_intance;

    // websocket context
    gw_websocket_desp_t _ws_desp;

    // context daemon thread handle
    std::thread *_daemon_thread;

    // capsule heartbeat thread handle
    std::thread *_heartbeat_thread;

    // signal for stopping the daemon thread
    volatile bool _is_daemon_stop;
    /* ======================== Communication w/ Scheduler ======================== */


    /* ==================== Serve Context ==================== */
 protected:
    // identify whether the context is under serve mode
    bool _is_serving;
    
    // map of created profiler
    // device_id -> profiler_sign -> profiler
    std::map<int, std::map<gw_image_sign_t, GWProfiler*>> _map_profiler;
    /* ==================== Serve Context ==================== */
};
