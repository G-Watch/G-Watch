#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <unordered_map>


#include "gwatch/common.hpp"


namespace gwatch {

namespace cuda {


// forward declarations
class ProfileContext;
class ProfileDevice;
class Profiler;
class KernelDef;


/*!
 *  \brief  profiler mode
 */
enum ProfilerMode {
    Range_Profiling = 0,
    PM_Sampling = 1,
    PC_Sampling = 2
};


/*!
 *  \brief  range profile range mode
 */
enum RangeProfileRangeMode : int {
    kRangeProfile_RangeMode_AutoRange = 0,
    kRangeProfile_RangeMode_UserRange = 1
};


/*!
 *  \brief  range profile replay mode
 */
enum RangeProfileReplayMode : int {
    kRangeProfile_ReplayMode_KernelReplay = 0,
    kRangeProfile_ReplayMode_UserReplay = 1
};


/*!
 *  \brief  range profile session config
 */
struct RangeProfileSessionConfig {
    int max_launches_per_pass;
    int max_ranges_per_pass;
    RangeProfileRangeMode range_mode;
    RangeProfileReplayMode replay_mode;
    int min_nesting_level;
    int num_nesting_levels;
    int target_nesting_levels;
};


/*!
 *  \brief  PM sample data
 */
struct PMSampleData {
    uint64_t index;
    uint64_t s_tick;
    uint64_t e_tick;
    std::unordered_map<std::string, double> map_emtrics;
};


/*!
 *  \brief  PC sample data
 */
struct PCSampleData {
    uint64_t pc;
    std::map<uint32_t, uint64_t> map_reason_count;
};


/*!
 *  \brief  PM sampling config
 */
struct PMSamplingConfig {
    uint64_t hw_buf_size;
    uint64_t sampling_interval;
    uint32_t max_samples;
};


/*!
 * \brief Profiler Context Management Class
 */
class ProfileContext {
 public:
    ProfileContext(bool lazy_init_device=true);
    ~ProfileContext();


    /*!
     * \brief create a profiler instance
     * \param device_id target device ID
     * \param metric_names list of metric names to profile
     * \param profiler_mode profiler mode (PM sampling or range profiling)
     * \param profiler output profiler instance
     * \return error code of the operation
     */
    gwError create_profiler(
        int device_id,
        const std::vector<std::string>& metric_names,
        ProfilerMode profiler_mode,
        Profiler** profiler
    );


    /*!
     * \brief destroy a profiler instance
     * \param profiler profiler instance to destroy
     * \return error code of the operation
     */
    gwError destroy_profiler(Profiler* profiler);


    /*!
     * \brief get list of available devices
     * \param devices output vector of device IDs
     * \return error code of the operation
     */
    gwError get_devices(std::map<int, ProfileDevice*>& map_devices);


    /*!
     * \brief get clock frequencies for a device
     * \param device_id target device ID
     * \param clock_map output map of clock domains to frequencies
     * \return error code of the operation
     */
    gwError get_clock_freq(int device_id, std::map<std::string, uint32_t>& clock_map);


 private:
    void* _gw_profile_context_cuda_handle = nullptr;
    std::map<const void*, ProfileDevice*> _map_profile_device = {};
};


/*!
 * \brief profile device management class
 */
class ProfileDevice {
 public:
    /*!
     * \brief export metric properties to cache file
     * \param cache_path path to export the metric properties
     * \return error code of the operation
     */
    gwError export_metric_properties(const std::string& cache_path);

 private:
    friend class ProfileContext;

    ProfileDevice() = default;
    ~ProfileDevice() = default;

    const void* _gw_profile_device_cuda_handle = nullptr;
};


/*!
 * \brief CUDA Profiler
 */
class Profiler {
 public:
    Profiler() = default;
    ~Profiler() = default;

    // Profiler is not copyable or movable
    Profiler(Profiler&&) = delete;
    Profiler& operator=(Profiler&&) = delete;

    /* =============== Range Profile APIs =============== */
    /*!
     * \brief start a range profiling session
     * \param config session configuration parameters
     * \return error code of the operation
     */
    gwError RangeProfile_start_session(const RangeProfileSessionConfig& config);


    /*!
     * \brief destroy the range profiling session
     * \return error code of the operation
     */
    gwError RangeProfile_destroy_session();


    /*!
     * \brief Check if a session is created
     * \param is_created Output flag indicating if session exists
     * \return Error code of the operation
     */
    gwError RangeProfile_is_session_created(bool& is_created);


    /*!
     * \brief begin a profiling pass
     * \return error code of the operation
     */
    gwError RangeProfile_begin_pass();


    /*!
     * \brief End a profiling pass
     * \param is_last_pass Output flag indicating if this is the last pass
     * \return Error code of the operation
     */
    gwError RangeProfile_end_pass(bool& is_last_pass);


    /*!
     * \brief enable range profiling
     * \return error code of the operation
     */
    gwError RangeProfile_enable_profiling();


    /*!
     * \brief disable range profiling
     * \return error code of the operation
     */
    gwError RangeProfile_disable_profiling();


    /*!
     * \brief push a named range onto the profiling stack
     * \param range_name name of the range
     * \return error code of the operation
     */
    gwError RangeProfile_push_range(const std::string& range_name);


    /*!
     * \brief pop a range from the profiling stack
     * \return error code of the operation
     */
    gwError RangeProfile_pop_range();


    /*!
     * \brief flush profiling data
     * \return error code of the operation
     */
    gwError RangeProfile_flush_data();


    /*!
     * \brief get collected metrics
     * \param metrics output map of range names to metric values
     * \return error code of the operation
     */
    gwError RangeProfile_get_metrics(
        std::map<std::string, std::map<std::string, double>>& metrics
    );


    /* =============== PC Sampling APIs =============== */    
    /*!
     * \brief enable PC sampling profiling
     * \param kernel_def kernel definition to profile
     * \return error code of the operation
     */
    gwError PcSampling_enable_profiling(KernelDef* kernel_def);


    /*!
     * \brief disable PC sampling profiling
     * \return error code of the operation
     */
    gwError PcSampling_disable_profiling();


    /*!
     * \brief start PC sampling profiling
     * \return error code of the operation
     */
    gwError PcSampling_start_profiling();


    /*!
     * \brief stop PC sampling profiling
     * \return error code of the operation
     */
    gwError PcSampling_stop_profiling();


    /*!
     * \brief get PC sampling metrics
     * \param samples output vector of PC samples
     * \return error code of the operation
     */
    gwError PcSampling_get_metrics(std::map<uint64_t, std::map<uint32_t, uint64_t>>& samples);


    /*!
     * \brief get stall reason map
     * \param map_stall_reason output map of stall reason codes to descriptions
     * \return error code of the operation
     */
    gwError PcSampling_get_stall_reason(std::map<uint32_t, std::string>& map_stall_reason);


    /* =============== PM Sampling APIs =============== */    
    /*!
     * \brief enable PM sampling profiling
     * \return error code of the operation
     */
    gwError PmSampling_enable_profiling();


    /*!
     * \brief disable PM sampling profiling
     * \return error code of the operation
     */
    gwError PmSampling_disable_profiling();


    /*!
     * \brief set PM sampling configuration
     * \param config PM sampling configuration parameters
     * \return error code of the operation
     */
    gwError PmSampling_set_config(const PMSamplingConfig& config);


    /*!
     * \brief start PM sampling profiling
     * \return error code of the operation
     */
    gwError PmSampling_start_profiling();


    /*!
     * \brief stop PM sampling profiling
     * \return error code of the operation
     */
    gwError PmSampling_stop_profiling();


    /*!
     * \brief get PM sampling metrics
     * \param samples output vector of PM samples
     * \return error code of the operation
     */
    gwError PmSampling_get_metrics(std::vector<PMSampleData>& samples);


    /* =============== Checkpoint/Restore APIs =============== */    
    /*!
     * \brief create a checkpoint of the current profiler state
     * \return error code of the operation
     */
    gwError checkpoint();


    /*!
     * \brief restore profiler state from checkpoint
     * \param do_pop flag indicating whether to pop the checkpoint after restore
     * \return error code of the operation
     */
    gwError restore(bool do_pop);


    /*!
     * \brief free the checkpoint data
     * \return error code of the operation
     */
    gwError free_checkpoint();


    /* =============== Common APIs =============== */    
    /*!
     * \brief reset counter data
     * \return error code of the operation
     */
    gwError reset_counter_data();


 private:
    friend class ProfileContext;

    // handle to the profiler context
    void* _gw_profiler_cuda_handle = nullptr;
};

    
} // namespace cuda

} // namespace gwatch
