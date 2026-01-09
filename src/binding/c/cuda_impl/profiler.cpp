#include <iostream>
#include <string>

#include "common/common.hpp"
#include "profiler/cuda_impl/context.hpp"
#include "profiler/cuda_impl/device.hpp"
#include "profiler/cuda_impl/profiler.hpp"
#include "profiler/cuda_impl/metric.hpp"

// public header
#include "gwatch/cuda/profiler.hpp"
#include "gwatch/cuda/kernel.hpp"


namespace gwatch {

namespace cuda {


ProfileContext::ProfileContext(bool lazy_init_device){
    this->_gw_profile_context_cuda_handle = new GWProfileContext_CUDA(lazy_init_device);
    GW_CHECK_POINTER(this->_gw_profile_context_cuda_handle);
}


ProfileContext::~ProfileContext(){
    delete this->_gw_profile_context_cuda_handle;
}


gwError ProfileContext::create_profiler(
    int device_id,
    const std::vector<std::string>& metric_names,
    ProfilerMode profiler_mode,
    Profiler** profiler
){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler* gw_profiler = nullptr;
    gw_profiler_mode_t _profiler_mode;
    GWProfileContext_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfileContext_CUDA *)this->_gw_profile_context_cuda_handle)

    // dispatch profiler mode
    switch (profiler_mode) {
        case PM_Sampling:
            _profiler_mode = GWProfiler_Mode_CUDA_PM_Sampling;
            #if !(GW_CUDA_VERSION_MAJOR >= 12 && GW_CUDA_VERSION_MINOR >= 6)
                throw GWException("PM sampling is not supported before CUDA 12.6");
            #endif
            break;
    
        case Range_Profiling:
            _profiler_mode = GWProfiler_Mode_CUDA_Range_Profiling;
            break;

        case PC_Sampling:
            _profiler_mode = GWProfiler_Mode_CUDA_PC_Sampling;
            break;

        default:
            GW_WARN("invalid profiler mode: %d", profiler_mode);
            retval = gwFailed;
            goto exit;
    }

    // create profiler
    GW_IF_FAILED(
        self->create_profiler(device_id, metric_names, _profiler_mode, &gw_profiler),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

    // assign profiler handle
    *profiler = new Profiler();
    GW_CHECK_POINTER(*profiler);
    (*profiler)->_gw_profiler_cuda_handle = gw_profiler;

exit:
    return retval;
}


gwError ProfileContext::destroy_profiler(Profiler* profiler){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfileContext_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfileContext_CUDA *)this->_gw_profile_context_cuda_handle);
    GW_CHECK_POINTER(profiler);

    // destroy profiler
    self->destroy_profiler((GWProfiler*)(profiler->_gw_profiler_cuda_handle));

    // delete profiler handle
    delete profiler;

exit:
    return retval;
}


gwError ProfileContext::get_devices(std::map<int, ProfileDevice*>& map_devices){
    gwError retval = gwSuccess;
    GWProfileContext_CUDA *self = nullptr;
    std::map<int, const GWProfileDevice*> map_gw_profile_devices = {};
    int device_id = 0;
    ProfileDevice *profile_device = nullptr;
    const GWProfileDevice *_gw_profile_device = nullptr;

    GW_CHECK_POINTER(self = (GWProfileContext_CUDA*)this->_gw_profile_context_cuda_handle);

    // get devices
    map_gw_profile_devices = self->get_devices();
    for(auto iter = map_gw_profile_devices.begin(); iter != map_gw_profile_devices.end(); iter++){
        device_id = iter->first;
        _gw_profile_device = iter->second;
        if(this->_map_profile_device.find((void*)(_gw_profile_device)) == this->_map_profile_device.end()){
            profile_device = new ProfileDevice();
            this->_map_profile_device[(void*)(_gw_profile_device)] = profile_device;
            profile_device->_gw_profile_device_cuda_handle = _gw_profile_device;
        }
        map_devices[device_id] = this->_map_profile_device[(void*)(_gw_profile_device)];
    }

exit:
    return retval;
}


gwError ProfileContext::get_clock_freq(int device_id, std::map<std::string, uint32_t>& clock_map){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfileContext_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfileContext_CUDA *)this->_gw_profile_context_cuda_handle)
    
    // get clock freq
    GW_IF_FAILED(
        self->get_clock_freq(device_id, clock_map),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError ProfileDevice::export_metric_properties(const std::string& cache_path){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfileDevice_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfileDevice_CUDA *)this->_gw_profile_device_cuda_handle)
    
    // export metric properties
    GW_IF_FAILED(
        self->export_metric_properties(cache_path),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::RangeProfile_start_session(const RangeProfileSessionConfig& config){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // start range profile session
    GW_IF_FAILED(
        self->RangeProfile_start_session(
            config.max_launches_per_pass,
            config.max_ranges_per_pass,
            (GWProfiler_CUDA_RangeProfile_RangeMode)(config.range_mode),
            (GWProfiler_CUDA_RangeProfile_ReplayMode)(config.replay_mode),
            config.min_nesting_level,
            config.num_nesting_levels,
            config.target_nesting_levels
        ),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::RangeProfile_destroy_session(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // destroy range profile session
    GW_IF_FAILED(
        self->RangeProfile_destroy_session(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::RangeProfile_is_session_created(bool& is_created){
    gwError retval = gwSuccess;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // check if range profile session is created
    is_created = self->RangeProfile_is_session_created();

exit:
    return retval;
}


gwError Profiler::RangeProfile_begin_pass(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // begin range profile pass
    GW_IF_FAILED(
        self->RangeProfile_begin_pass(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::RangeProfile_end_pass(bool& is_last_pass){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // begin range profile pass
    GW_IF_FAILED(
        self->RangeProfile_end_pass(is_last_pass),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::RangeProfile_enable_profiling(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // enable range profile profiling
    GW_IF_FAILED(
        self->RangeProfile_enable_profiling(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::RangeProfile_disable_profiling(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // disable range profile profiling
    GW_IF_FAILED(
        self->RangeProfile_disable_profiling(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::RangeProfile_push_range(const std::string& range_name){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // push range profile range
    GW_IF_FAILED(
        self->RangeProfile_push_range(range_name),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::RangeProfile_pop_range(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // pop range profile range
    GW_IF_FAILED(
        self->RangeProfile_pop_range(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::RangeProfile_flush_data(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // flush range profile data
    GW_IF_FAILED(
        self->RangeProfile_flush_data(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::RangeProfile_get_metrics(std::map<std::string, std::map<std::string, double>>& metrics){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // get range profile metrics
    GW_IF_FAILED(
        self->RangeProfile_get_metrics(metrics),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::PcSampling_enable_profiling(KernelDef* kernel_def){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;
    GWKernelDef *_gw_kernel_def = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle);
    GW_CHECK_POINTER(_gw_kernel_def = (GWKernelDef *)kernel_def->_gw_kerneldef_cuda_handle)

    GW_IF_FAILED(
        self->PcSampling_enable_profiling(_gw_kernel_def),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::PcSampling_disable_profiling(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle);

    GW_IF_FAILED(
        self->PcSampling_disable_profiling(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::PcSampling_start_profiling(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle);

    GW_IF_FAILED(
        self->PcSampling_start_profiling(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::PcSampling_stop_profiling(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle);

    GW_IF_FAILED(
        self->PcSampling_stop_profiling(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::PcSampling_get_metrics(std::map<uint64_t, std::map<uint32_t, uint64_t>>& samples){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle);

    GW_IF_FAILED(
        self->PcSampling_get_metrics(samples),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::PcSampling_get_stall_reason(std::map<uint32_t, std::string>& map_stall_reason){
    gwError retval = gwSuccess;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle);

    // get pm sampling stall reason
    map_stall_reason = self->PcSampling_get_stall_reason();

exit:
    return retval;
}


gwError Profiler::PmSampling_enable_profiling(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // enable pm sampling profiling
    GW_IF_FAILED(
        self->PmSampling_enable_profiling(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::PmSampling_disable_profiling(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // disable pm sampling profiling
    GW_IF_FAILED(
        self->PmSampling_disable_profiling(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::PmSampling_set_config(const PMSamplingConfig& config){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // set pm sampling config
    GW_IF_FAILED(
        self->PmSampling_set_config(
            config.hw_buf_size,
            config.sampling_interval,
            config.max_samples
        ),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::PmSampling_start_profiling(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // start pm sampling profiling
    GW_IF_FAILED(
        self->PmSampling_start_profiling(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::PmSampling_stop_profiling(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // stop pm sampling profiling
    GW_IF_FAILED(
        self->PmSampling_stop_profiling(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::PmSampling_get_metrics(std::vector<PMSampleData>& samples){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;
    std::vector<gw_profiler_pm_sample_t> _samples;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // get pm sampling metrics
    GW_IF_FAILED(
        self->PmSampling_get_metrics(_samples),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

    // convert _samples to samples
    samples.reserve(_samples.size());
    for(auto iter = _samples.begin(); iter != _samples.end(); iter++){
        samples.emplace_back(PMSampleData{
            iter->index,
            iter->s_tick,
            iter->e_tick,
            iter->map_emtrics
        });
    }

exit:
    return retval;
}


gwError Profiler::checkpoint(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // checkpoint
    GW_IF_FAILED(
        self->checkpoint(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::restore(bool do_pop){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // restore
    GW_IF_FAILED(
        self->restore(do_pop),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::free_checkpoint(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // free checkpoint
    GW_IF_FAILED(
        self->free_checkpoint(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


gwError Profiler::reset_counter_data(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWProfiler_CUDA *self = nullptr;

    GW_CHECK_POINTER(self = (GWProfiler_CUDA *)this->_gw_profiler_cuda_handle)

    // reset counter data
    GW_IF_FAILED(
        self->reset_counter_data(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    )

exit:
    return retval;
}


} // namespace cuda

} // namespace gwatch
