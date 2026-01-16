#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <stack>
#include <thread>

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cupti_target.h>
#include <cupti_profiler_target.h>
#include <cupti_checkpoint.h>

#include <nlohmann/json.hpp>

#if GW_CUDA_VERSION_MAJOR >= 12 && GW_CUDA_VERSION_MINOR >= 6
    #include <cupti_profiler_host.h>
    #include <cupti_pmsampling.h>
#endif

#if GW_CUDA_VERSION_MAJOR >= 9
    #include <cupti_pcsampling.h>
#endif

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/hash.hpp"
#include "common/utils/timer.hpp"
#include "common/assemble/kernel_def.hpp"
#include "profiler/profiler.hpp"
#include "profiler/cuda_impl/device.hpp"
#include "profiler/cuda_impl/profile_image.hpp"


// mode of the profiler
enum :gw_profiler_mode_t {
    GWProfiler_Mode_CUDA_Unknown = 0,
    GWProfiler_Mode_CUDA_Range_Profiling,
    GWProfiler_Mode_CUDA_PM_Sampling,
    GWProfiler_Mode_CUDA_PC_Sampling
    // TODO(zhuobin): we should support continuous sampling of PC sampling later
};


/*!
 *  \brief  range profiler range mode definition
 */
enum GWProfiler_CUDA_RangeProfile_RangeMode : int {
    kGWProfiler_CUDA_RangeProfile_RangeMode_AutoRange = 0,
    kGWProfiler_CUDA_RangeProfile_RangeMode_UserRange
};


/*!
 *  \brief  range profiler range mode definition
 */
enum GWProfiler_CUDA_RangeProfile_ReplayMode : int {
    kGWProfiler_CUDA_RangeProfile_ReplayMode_KernelReplay = 0,
    kGWProfiler_CUDA_RangeProfile_ReplayMode_UserReplay
};


// default values for range profiler
#define GW_CUPTI_DEFAULT_MAX_LAUNCHES_PER_PASS  512
#define GW_CUPTI_DEFAULT_MAX_RANGES_PER_PASS    64
#define GW_CUPTI_DEFAULT_MIN_NESTING_LEVEL      1
#define GW_CUPTI_DEFAULT_NUM_NESTING_LEVELS     1
#define GW_CUPTI_DEFAULT_TARGET_NESTING_LEVELS  1


/*!
 *  \brief  one sample under PM sampling mode
 */
typedef struct gw_profiler_pm_sample {
    uint64_t index;
    uint64_t s_tick;
    uint64_t e_tick;
    std::unordered_map<std::string, double> map_emtrics;

    /* ======================== JSON serilization ======================== */
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        struct gw_profiler_pm_sample,
        index, s_tick, e_tick, map_emtrics
    );
    /* ======================== JSON serilization ======================== */
} gw_profiler_pm_sample_t;


/*!
 *  \brief  CUPTI profiling context
 */
class GWProfiler_CUDA final : public GWProfiler {
 public:
    /* ============================== Common ============================== */
    /*!
     *  \brief  constructor
     *  \note   the constructor mainly does the following things:
     *          [1] retain the primary context of the specified device
     *          [2] initialize the Config image
     *          [3] initialize the Counter Data Prefix image
     *          [4] initialize the Counter Data image
     *          [5] initialize the Counter Data Scratch Buffer image
     *  \param  gw_device       the device to profile
     *  \param  metric_names    names of all metrics to collect
     *  \param  profiler_mode   mode of the profiler
     */
    GWProfiler_CUDA(
        GWProfileDevice* gw_device,
        std::vector<std::string> metric_names,
        gw_profiler_mode_t profiler_mode = GWProfiler_Mode_CUDA_Range_Profiling
    );


    /*!
     *  \brief  deconstructor
     *  \note   the deconstructor mainly does the following things:
     *          [1] release the primary context of the specified device
     */
    ~GWProfiler_CUDA();


    /*!
     *  \brief  reset the collected counter data to 0
     *  \return GW_SUCCESS for successfully reseted
     */
    gw_retval_t reset_counter_data();
    /* ============================== Common ============================== */


    /* ============================== PC Sampling ============================== */
    #if GW_CUDA_VERSION_MAJOR >= 9
 public:
    /*!
     *  \brief  enable profiling
     *  \param  kernel  kernel to profile
     *  \return GW_SUCCESS for successfully begin
     */
    gw_retval_t PcSampling_enable_profiling(GWKernelDef* kernel_def = nullptr);


    /*!
     *  \brief  disable profiling
     *  \return GW_SUCCESS for successfully disable
     */
    gw_retval_t PcSampling_disable_profiling();


    /*!
     *  \brief  start profiling
     *  \return GW_SUCCESS for successfully start
     */
    gw_retval_t PcSampling_start_profiling();


    /*!
     *  \brief  stop profiling
     *  \return GW_SUCCESS for successfully stop
     */
    gw_retval_t PcSampling_stop_profiling();


    /*!
     *  \brief  stop profiling
     *  \param  map_metrics map of profiling result <pc, <stall_reason_index, stall_reason_count>>
     *  \return GW_SUCCESS for successfully stop
     */
    gw_retval_t PcSampling_get_metrics(std::map<uint64_t, std::map<uint32_t, uint64_t>>& map_metrics);


    /*!
     *  \brief  get the map of stall reason
     *  \return map of stall reason: <stall_reason_index, stall_reason_string>
     */
    inline std::map<uint32_t, std::string> PcSampling_get_stall_reason() const {
        return this->_map_pc_sampling_stall_reason;
    }


 private:
    // map of stall reason: <stall_reason_index, stall_reason_string>
    std::map<uint32_t, std::string> _map_pc_sampling_stall_reason;

    // buffer to hold collected PC Sampling data in PC-To-Counter format
    CUpti_PCSamplingData _pc_sampling_data_for_sampling;
    CUpti_PCSamplingData _pc_sampling_data_for_collecting;
    #endif
    /* ============================== PC Sampling ============================== */


    /* ============================== PM Sampling ============================== */
    #if GW_CUDA_VERSION_MAJOR >= 12 && GW_CUDA_VERSION_MINOR >= 6
 public:
    /*!
     *  \brief  enable profiling
     *  \return GW_SUCCESS for successfully begin
     */
    gw_retval_t PmSampling_enable_profiling();


    /*!
     *  \brief  disable profiling
     *  \return GW_SUCCESS for successfully disable
     */
    gw_retval_t PmSampling_disable_profiling();


    /*!
     *  \brief  setup the sampling process
     *  \param  hw_buf_size              size of the hardware buffer
     *  \param  sampling_interval        sampling interval in ns
     *  \param  max_samples              max samples to collect
     *  \return GW_SUCCESS for successfully setup
     */
    gw_retval_t PmSampling_set_config(
        uint64_t hw_buf_size,
        uint64_t sampling_interval = 100000,
        uint32_t max_samples = 10000
    );


    /*!
     *  \brief  start profiling
     *  \return GW_SUCCESS for successfully start
     */
    gw_retval_t PmSampling_start_profiling();
    
    
    /*!
     *  \brief  stop profiling
     *  \return GW_SUCCESS for successfully stop
     */
    gw_retval_t PmSampling_stop_profiling();


    /*!
     *  \brief  decode the counter data
     *  \return GW_SUCCESS for successfully decoding
     */
    gw_retval_t PmSampling_get_metrics(std::vector<gw_profiler_pm_sample_t> &map_metrics);


    // image library
    static GWProfileImageLibrary image_library_pm_sampling_counter_availability;
    static GWProfileImageLibrary image_library_pm_sampling_config;

 private:
    /*!
     *  \brief  thread function for decoding PC sampling at real time
     */
    void __PmSampling_decode_counter_data();

    // pm sampler
    CUpti_PmSampling_Object *_cupti_pm_sampler = nullptr;

    // host-side profiler
    CUpti_Profiler_Host_Object *_pm_sampling_current__host_profiler = nullptr;
    std::map<gw_image_sign_t, CUpti_Profiler_Host_Object*> _map_pm_sampling_host_profiler;

    // profiling result
    volatile bool _PmSampling_is_profiling = false;
    std::thread *_PmSampling_decode_thread = nullptr;
    std::vector<gw_profiler_pm_sample_t> _PmSampling_map_metrics;

    // image
    GWProfileImage_CUDA_PmSampling_CounterAvailability *_image_pm_sampling_counter_availability = nullptr;
    GWProfileImage_CUDA_PmSampling_Config *_image_pm_sampling_config = nullptr;
    GWProfileImage_CUDA_PmSampling_CounterData *_image_pm_sampling_counter_data = nullptr;
    #endif
    /* ============================== PM Sampling ============================== */


    /* ============================== Range Profiling ============================== */
 public:
    /*!
     *  \brief  start session for profiling on specified device
     *  \ref    [1] https://docs.nvidia.com/cupti/main/main.html#cupti-profiling-api
     *          [2] ref_codes/cupti_samples/concurrent_profiling - StartSession
     *  \param  max_launches_per_pass               max of kernels within a single pass
     *  \param  max_ranges_per_pass                 max of ranges within a single pass
     *  \param  cupti_profile_range_mode            range mode, 0 for auto range, 1 for user range
     *  \param  cupti_profile_replay_mode           reply mode, 0 for kernel replay, 1 for user replay
     *  \param  cupti_profile_min_nesting_level
     *  \param  cupti_profile_num_nesting_levels
     *  \param  cupti_profile_target_nesting_levels
     *  \return GW_SUCCESS for successfully start a new session
     */
    gw_retval_t RangeProfile_start_session(
        int max_launches_per_pass = GW_CUPTI_DEFAULT_MAX_LAUNCHES_PER_PASS,
        int max_ranges_per_pass = GW_CUPTI_DEFAULT_MAX_RANGES_PER_PASS,
        GWProfiler_CUDA_RangeProfile_RangeMode cupti_profile_range_mode = GWProfiler_CUDA_RangeProfile_RangeMode::kGWProfiler_CUDA_RangeProfile_RangeMode_UserRange,
        GWProfiler_CUDA_RangeProfile_ReplayMode cupti_profile_reply_mode = GWProfiler_CUDA_RangeProfile_ReplayMode::kGWProfiler_CUDA_RangeProfile_ReplayMode_UserReplay,
        int cupti_profile_min_nesting_level = GW_CUPTI_DEFAULT_MIN_NESTING_LEVEL,
        int cupti_profile_num_nesting_levels = GW_CUPTI_DEFAULT_NUM_NESTING_LEVELS,
        int cupti_profile_target_nesting_levels = GW_CUPTI_DEFAULT_TARGET_NESTING_LEVELS
    );


    /*!
     *  \brief  destory the profile session
     *  \return GW_SUCCESS for successfully closure
     */
    gw_retval_t RangeProfile_destroy_session();


    /*!
     *  \brief  return whether a profiling session is created
     *  \return identifier of whether a profiling session is created
     */
    inline bool RangeProfile_is_session_created(){ return this->_RangeProfile_is_session_created; }


    /*!
     *  \brief  begin a profiling pass within a session
     *  \return GW_SUCCESS for successfully begin
     */
    gw_retval_t RangeProfile_begin_pass();


    /*!
     *  \brief  shutdown a profiling pass
     *  \param  is_last_pass    indicate whether this is the last pass to run
     *  \return GW_SUCCESS for successfully begin
     */
    gw_retval_t RangeProfile_end_pass(bool& is_last_pass);


    /*!
     *  \brief  enable profiling within a pass
     *  \return GW_SUCCESS for successfully begin
     */
    gw_retval_t RangeProfile_enable_profiling();


    /*!
     *  \brief  disable profiling within a pass
     *  \return GW_SUCCESS for successfully begin
     */
    gw_retval_t RangeProfile_disable_profiling();


    /*!
     *  \brief  push a range within a pass
     *  \param  range_name  range name
     *  \return GW_SUCCESS for successfully begin
     */
    gw_retval_t RangeProfile_push_range(std::string range_name);


    /*!
     *  \brief  pop a range within a pass
     *  \return GW_SUCCESS for successfully begin
     */
    gw_retval_t RangeProfile_pop_range();


    /*!
     *  \brief  flush profiling data to the image
     *  \return GW_SUCCESS for successfully flush
     */
    gw_retval_t RangeProfile_flush_data();


    /*!
     *  \brief  obtain profiled results
     *  \param  value_map   map of profiling result
     *                      range name -> list of <metric name, metric value>
     *  \return GW_SUCCESS for successfully obtained
     */
    gw_retval_t RangeProfile_get_metrics(std::map<std::string, std::map<std::string, double>> &value_map);


    // profiling data images
    static GWProfileImageLibrary image_library_counter_availability;
    static GWProfileImageLibrary image_library_configuration;
    static GWProfileImageLibrary image_library_counter_data_prefix;
    static GWProfileImageLibrary image_library_counter_data;
    static GWProfileImageLibrary image_library_scratch_buffer;

 private:
    // mark whether a profile session is created
    bool _RangeProfile_is_session_created;

    // images
    GWProfileImage_CUDA_RangeProfile_CounterAvailability *_image_counter_availability;
    GWProfileImage_CUDA_RangeProfile_Configuration *_image_configuration;
    GWProfileImage_CUDA_RangeProfile_CounterDataPrefix *_image_counter_data_prefix;
    GWProfileImage_CUDA_RangeProfile_CounterData *_image_counter_data;
    GWProfileImage_CUDA_RangeProfile_ScratchBuffer *_image_scratch_buffer;
    /* ============================== Range Profiling ============================== */


    /* ============================== Checkpoint / Restore ============================== */
 public:
    /*!
     *  \brief  checkpoint GPU memory state before pass a non-Idempotent operation
     *  \return GW_SUCCESS for successfully checkpoint
     */
    gw_retval_t checkpoint();


    /*!
     *  \brief  restore GPU memory state before re-pass a non-Idempotent operation
     *  \param  do_pop  whether to pop out previous checkpoint
     *  \return GW_SUCCESS for successfully restore
     */
    gw_retval_t restore(bool do_pop=false);

    
    /*!
     *  \brief  free the latest checkpoint
     *  \return GW_SUCCESS for successfully free
     */
    gw_retval_t free_checkpoint();


 private:
    // cupti checkpoint option
    std::stack<NV::Cupti::Checkpoint::CUpti_Checkpoint*> _cupti_ckpts;
    /* ============================== Checkpoint / Restore ============================== */
};
