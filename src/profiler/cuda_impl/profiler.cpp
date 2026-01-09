#include <iostream>
#include <map>
#include <thread>
#include <string.h>
#include <format>

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cupti_target.h>
#include <cupti_profiler_target.h>
#include <cupti_checkpoint.h>
#include <cupti_activity.h>
#include <nvperf_host.h>
#include <nvperf_cuda_host.h>
#include <nvperf_target.h>

#if GW_CUDA_VERSION_MAJOR >= 12 && GW_CUDA_VERSION_MINOR >= 6
    #include <cupti_profiler_host.h>
    #include <cupti_pmsampling.h>
#endif

#if GW_CUDA_VERSION_MAJOR >= 9
    #include <cupti_pcsampling.h>
#endif

#include "common/cuda_impl/cupti/common/helper_cupti.h"
#include "common/cuda_impl/cupti/extensions/include/profilerhost_util/Eval.h"
#include "common/cuda_impl/cupti/extensions/include/profilerhost_util/Metric.h"
#include "common/cuda_impl/cupti/extensions/include/profilerhost_util/Utils.h"
#include "common/cuda_impl/cupti/extensions/include/profilerhost_util/Parser.h"

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/timer.hpp"
#include "common/utils/hash.hpp"

#include "profiler/cuda_impl/device.hpp"
#include "profiler/cuda_impl/profiler.hpp"


GWProfileImageLibrary GWProfiler_CUDA::image_library_counter_availability;
GWProfileImageLibrary GWProfiler_CUDA::image_library_configuration;
GWProfileImageLibrary GWProfiler_CUDA::image_library_counter_data_prefix;
GWProfileImageLibrary GWProfiler_CUDA::image_library_counter_data;
GWProfileImageLibrary GWProfiler_CUDA::image_library_scratch_buffer;
#if GW_CUDA_VERSION_MAJOR >= 12 && GW_CUDA_VERSION_MINOR >= 6
    GWProfileImageLibrary GWProfiler_CUDA::image_library_pm_sampling_counter_availability;
    GWProfileImageLibrary GWProfiler_CUDA::image_library_pm_sampling_config;
#endif


static constexpr uint32_t kMaxNumRangePerProfile = 512; 
static constexpr uint32_t kMaxRangeNameLength = 128; 


GWProfiler_CUDA::GWProfiler_CUDA(GWProfileDevice* gw_device, std::vector<std::string> metric_names, gw_profiler_mode_t profiler_mode)
    :   GWProfiler(gw_device, metric_names, profiler_mode),
        _RangeProfile_is_session_created(false),
        _image_counter_availability(nullptr),
        _image_configuration(nullptr),
        _image_counter_data_prefix(nullptr),
        _image_counter_data(nullptr),
        _image_scratch_buffer(nullptr)
{
    gw_retval_t retval;
    int sdk_retval;
    std::string chip_name;
    uint64_t range_profile_image_size = 0, pm_sampling_image_size = 0;
    GWProfileImage *image;
    GWProfileDevice_CUDA *gw_device_cuda;

    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(gw_device));

    chip_name = gw_device_cuda->get_chip_name();
    GW_ASSERT(chip_name.size() > 0);

    this->_tick_create = GWUtilTscTimer::get_tsc();
    if(metric_names.size() > 0){
        this->_sign = GWProfileImage::sign(metric_names, chip_name);
    }
 
    if(profiler_mode == GWProfiler_Mode_CUDA_Range_Profiling){
        /* ==================== initialize of images for range profiling ==================== */
        GW_DEBUG_C("initializing profiler with range profiling mode...");
        
        // config the counter availability image
        retval = this->image_library_counter_availability.get_image_by_sign(this->_sign, &image);
        if(retval == GW_FAILED_NOT_EXIST){
            this->_image_counter_availability = new GWProfileImage_CUDA_RangeProfile_CounterAvailability(gw_device_cuda->get_device_id());
            GW_CHECK_POINTER(this->_image_counter_availability);
            GW_IF_FAILED(this->_image_counter_availability->fill(), sdk_retval, {
                throw GWException("failed to fill counter availability image");
            });
            this->image_library_counter_availability.store_image(
                this->_sign, reinterpret_cast<GWProfileImage*>(this->_image_counter_availability)
            );
        } else {
            GW_CHECK_POINTER(this->_image_counter_availability = reinterpret_cast<GWProfileImage_CUDA_RangeProfile_CounterAvailability*>(image));
        }
        range_profile_image_size += this->_image_counter_availability->size();

        // config the configuration image
        retval = this->image_library_configuration.get_image_by_sign(this->_sign, &image);
        if(retval == GW_FAILED_NOT_EXIST){
            this->_image_configuration = new GWProfileImage_CUDA_RangeProfile_Configuration(
                chip_name.c_str(),
                this->_image_counter_availability,
                metric_names
            );
            GW_CHECK_POINTER(this->_image_configuration);
            GW_IF_FAILED(this->_image_configuration->fill(), sdk_retval, {
                throw GWException("failed to fill configuration image");
            });
            this->image_library_configuration.store_image(
                this->_sign, reinterpret_cast<GWProfileImage*>(this->_image_configuration)
            );
        } else {
            GW_CHECK_POINTER(this->_image_configuration = reinterpret_cast<GWProfileImage_CUDA_RangeProfile_Configuration*>(image));
        }
        range_profile_image_size += this->_image_configuration->size();

        // config the counter data prefix image
        retval = this->image_library_counter_data_prefix.get_image_by_sign(this->_sign, &image);
        if(retval == GW_FAILED_NOT_EXIST){
            this->_image_counter_data_prefix = new GWProfileImage_CUDA_RangeProfile_CounterDataPrefix(
                chip_name.c_str(),
                this->_image_counter_availability,
                metric_names
            );
            GW_CHECK_POINTER(this->_image_counter_data_prefix);
            GW_IF_FAILED(this->_image_counter_data_prefix->fill(), sdk_retval, {
                throw GWException("failed to fill counter data prefix image");
            });
            this->image_library_counter_data_prefix.store_image(
                this->_sign, reinterpret_cast<GWProfileImage*>(this->_image_counter_data_prefix)
            );
        } else {
            GW_CHECK_POINTER(this->_image_counter_data_prefix = reinterpret_cast<GWProfileImage_CUDA_RangeProfile_CounterDataPrefix*>(image));
        }
        range_profile_image_size += this->_image_counter_data_prefix->size();

        // config the counter data image
        // note: we can't not reuse data image!
        this->_image_counter_data = new GWProfileImage_CUDA_RangeProfile_CounterData(
            /* image_counter_data_prefix */ this->_image_counter_data_prefix,
            /* max_num_ranges */ kMaxNumRangePerProfile,
            /* max_num_range_tree_nodes */ kMaxNumRangePerProfile, // TODO: is this correct?
            /* max_num_range_name_length */ kMaxRangeNameLength
        );
        GW_CHECK_POINTER(this->_image_counter_data);
        GW_IF_FAILED(this->_image_counter_data->fill(), sdk_retval, {
            throw GWException("failed to fill counter data image");
        });
        range_profile_image_size += this->_image_counter_data->size();

        // config the scratch buffer image
        retval = this->image_library_scratch_buffer.get_image_by_sign(this->_sign, &image);
        if(retval == GW_FAILED_NOT_EXIST){
            this->_image_scratch_buffer = new GWProfileImage_CUDA_RangeProfile_ScratchBuffer(this->_image_counter_data);
            GW_CHECK_POINTER(this->_image_scratch_buffer);
            GW_IF_FAILED(this->_image_scratch_buffer->fill(), sdk_retval, {
                throw GWException("failed to fill scratch buffer image");
            });
            this->image_library_scratch_buffer.store_image(
                this->_sign, reinterpret_cast<GWProfileImage*>(this->_image_scratch_buffer)
            );
        } else {
            GW_CHECK_POINTER(this->_image_scratch_buffer = reinterpret_cast<GWProfileImage_CUDA_RangeProfile_ScratchBuffer*>(image));
        }
        range_profile_image_size += this->_image_scratch_buffer->size();
        /* ==================== initialize of images for range profiling ==================== */
    } else if (profiler_mode == GWProfiler_Mode_CUDA_PM_Sampling) {
        /* ==================== initialize of images for pm sampling ==================== */
        #if GW_CUDA_VERSION_MAJOR >= 12 && GW_CUDA_VERSION_MINOR >= 6
            GW_DEBUG_C("initializing profiler with pm sampling mode...");

            CUpti_Profiler_Initialize_Params profilerInitializeParams = { 
                CUpti_Profiler_Initialize_Params_STRUCT_SIZE 
            };

            // config the counter availability image
            retval = this->image_library_pm_sampling_counter_availability.get_image_by_sign(this->_sign, &image);
            if(retval == GW_FAILED_NOT_EXIST){
                this->_image_pm_sampling_counter_availability = new GWProfileImage_CUDA_PmSampling_CounterAvailability(
                    gw_device_cuda->get_device_id()
                );
                GW_CHECK_POINTER(this->_image_pm_sampling_counter_availability);
                GW_IF_FAILED(this->_image_pm_sampling_counter_availability->fill(), sdk_retval, {
                    throw GWException("failed to fill pm sampling counter availability image");
                });
                this->image_library_pm_sampling_counter_availability.store_image(
                    this->_sign, reinterpret_cast<GWProfileImage*>(this->_image_pm_sampling_counter_availability)
                );
            } else {
                GW_CHECK_POINTER(this->_image_pm_sampling_counter_availability 
                    = reinterpret_cast<GWProfileImage_CUDA_PmSampling_CounterAvailability*>(image));
            }
            pm_sampling_image_size += this->_image_pm_sampling_counter_availability->size();

            // host-side profiler
            if(unlikely(this->_map_pm_sampling_host_profiler.count(this->_sign) == 0)){
                CUpti_Profiler_Host_Initialize_Params hostInitializeParams = {
                    CUpti_Profiler_Host_Initialize_Params_STRUCT_SIZE
                };
                CUpti_Profiler_Host_ConfigAddMetrics_Params configAddMetricsParams {
                    CUpti_Profiler_Host_ConfigAddMetrics_Params_STRUCT_SIZE
                };
                std::vector<const char*>  _list_metric_names;

                // create host-side profiler
                hostInitializeParams.profilerType = CUPTI_PROFILER_TYPE_PM_SAMPLING;
                hostInitializeParams.pChipName = chip_name.c_str();
                hostInitializeParams.pCounterAvailabilityImage = this->_image_pm_sampling_counter_availability->data();
                GW_IF_CUPTI_FAILED(
                    cuptiProfilerHostInitialize(&hostInitializeParams),
                    sdk_retval,
                    {
                        throw GWException("failed to initialize CUPTI host-side profiler for pm sampling");
                    }
                );
                this->_map_pm_sampling_host_profiler[this->_sign] = hostInitializeParams.pHostObject;

                // add configuration to host-side profiler for genetrating configuration image
                for (const auto& s : metric_names) { _list_metric_names.push_back(s.c_str()); }
                configAddMetricsParams.pHostObject = hostInitializeParams.pHostObject;
                configAddMetricsParams.ppMetricNames = _list_metric_names.data();
                configAddMetricsParams.numMetrics = _list_metric_names.size();
                GW_IF_CUPTI_FAILED(
                    cuptiProfilerHostConfigAddMetrics(&configAddMetricsParams),
                    sdk_retval,
                    {
                        throw GWException("failed to add configuration to pm sampling host-side profiler");
                    }
                );
            }
            this->_pm_sampling_current__host_profiler = this->_map_pm_sampling_host_profiler[this->_sign];

            // config image
            retval = this->image_library_pm_sampling_config.get_image_by_sign(this->_sign, &image);
            if(retval == GW_FAILED_NOT_EXIST){
                this->_image_pm_sampling_config = new GWProfileImage_CUDA_PmSampling_Config();
                GW_CHECK_POINTER(this->_image_pm_sampling_config);
                GW_IF_FAILED(
                    this->_image_pm_sampling_config->fill(
                        this->_pm_sampling_current__host_profiler
                    ),
                    retval, 
                    {
                        throw GWException("failed to fill pm sampling configuration image");
                    }
                );
            }

            // CUPTI profiler
            GW_IF_CUPTI_FAILED(
                cuptiProfilerInitialize(&profilerInitializeParams),
                sdk_retval,
                {
                    throw GWException("failed to initialize CUPTI profiler for pm sampling");
                }
            );

            // register callback for casting the GPU timestamp to CPU timestamp
            GW_IF_CUPTI_FAILED(
                cuptiActivityRegisterTimestampCallback(&GWUtilTscTimer::get_tsc),
                sdk_retval,
                {
                    throw GWException("failed to register timmestamp caster for pm sammpling");
                }
            );
        #endif
        /* ==================== initialize of images for pm sampling ==================== */
    } else if (profiler_mode == GWProfiler_Mode_CUDA_PC_Sampling) {
        #if GW_CUDA_VERSION_MAJOR >= 9
            memset(&(this->_pc_sampling_data_for_sampling), 0, sizeof(CUpti_PCSamplingData));
            memset(&(this->_pc_sampling_data_for_collecting), 0, sizeof(CUpti_PCSamplingData));
        #endif
    } else {
        GW_ERROR_C_DETAIL("shouldn't be here")
    }

    this->_tick_image_ready = GWUtilTscTimer::get_tsc();

    GW_DEBUG_C(
        "created profiler with necessary images: duration(%lf ms)",
        this->_tsc_timer.tick_to_ms(this->_tick_image_ready - this->_tick_create)
    );
}


GWProfiler_CUDA::~GWProfiler_CUDA(){
    if(this->_profiler_mode == GWProfiler_Mode_CUDA_PM_Sampling){
        #if GW_CUDA_VERSION_MAJOR >= 12 && GW_CUDA_VERSION_MINOR >= 6
            int sdk_retval;
            CUpti_Profiler_DeInitialize_Params profilerDeInitializeParams = { 
                CUpti_Profiler_DeInitialize_Params_STRUCT_SIZE
            };

            GW_IF_CUPTI_FAILED(
                cuptiProfilerDeInitialize(&profilerDeInitializeParams),
                sdk_retval,
                {
                    throw GWException("failed to deinitialize CUPTI profiler for pm sampling");
                }
            );
        #endif
    } else if(this->_profiler_mode == GWProfiler_Mode_CUDA_PC_Sampling){
        #if GW_CUDA_VERSION_MAJOR >= 9
            if(this->_pc_sampling_data_for_sampling.pPcData != nullptr){
                free(this->_pc_sampling_data_for_sampling.pPcData);
            }
            if(this->_pc_sampling_data_for_collecting.pPcData != nullptr){
                free(this->_pc_sampling_data_for_collecting.pPcData);
            }
        #endif
    }
}


gw_retval_t GWProfiler_CUDA::RangeProfile_start_session(
    int max_launches_per_pass,
    int max_ranges_per_pass,
    GWProfiler_CUDA_RangeProfile_RangeMode cupti_profile_range_mode,
    GWProfiler_CUDA_RangeProfile_ReplayMode cupti_profile_reply_mode,
    int cupti_profile_min_nesting_level,
    int cupti_profile_num_nesting_levels,
    int cupti_profile_target_nesting_levels
){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_BeginSession_Params begin_session_params = { CUpti_Profiler_BeginSession_Params_STRUCT_SIZE };
    CUpti_Profiler_SetConfig_Params set_config_params = { CUpti_Profiler_SetConfig_Params_STRUCT_SIZE };
    GWProfileDevice_CUDA *gw_device_cuda;

    GW_CHECK_POINTER(this->_image_configuration);
    GW_CHECK_POINTER(this->_image_counter_data);
    GW_CHECK_POINTER(this->_image_scratch_buffer);
    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));
    GW_CHECK_POINTER(gw_device_cuda->get_cu_context());

    GW_IF_CUDA_DRIVER_FAILED(cuCtxPushCurrent(gw_device_cuda->get_cu_context()), sdk_retval, {
        GW_WARN_C("failed to push context to current CPU thread before starting session");
        retval = GW_FAILED_SDK;
        goto exit;
    })

    begin_session_params.counterDataImageSize = this->_image_counter_data->size();
    begin_session_params.pCounterDataImage = this->_image_counter_data->data();
    begin_session_params.counterDataScratchBufferSize = this->_image_scratch_buffer->size();
    begin_session_params.pCounterDataScratchBuffer = this->_image_scratch_buffer->data();
    begin_session_params.ctx = gw_device_cuda->get_cu_context();
    begin_session_params.maxLaunchesPerPass = max_launches_per_pass;
    begin_session_params.maxRangesPerPass = max_ranges_per_pass;
    begin_session_params.pPriv = NULL;
    begin_session_params.range 
        = cupti_profile_range_mode == kGWProfiler_CUDA_RangeProfile_RangeMode_AutoRange 
        ? CUPTI_AutoRange : CUPTI_UserRange;
    begin_session_params.replayMode 
        = cupti_profile_reply_mode == kGWProfiler_CUDA_RangeProfile_ReplayMode_KernelReplay
        ? CUPTI_KernelReplay : CUPTI_UserReplay;
    GW_IF_CUPTI_FAILED(cuptiProfilerBeginSession(&begin_session_params), sdk_retval, {
        GW_WARN_C("failed to start profile session");
        retval = GW_FAILED;
        goto exit;
    });
    this->_RangeProfile_is_session_created = true;

    set_config_params.pConfig = this->_image_configuration->data();
    set_config_params.configSize = this->_image_configuration->size();
    set_config_params.passIndex = 0; // only set under CUPTI_ApplicationReplay
    set_config_params.minNestingLevel = cupti_profile_min_nesting_level;
    set_config_params.numNestingLevels = cupti_profile_num_nesting_levels;
    set_config_params.targetNestingLevel = cupti_profile_target_nesting_levels;
    GW_IF_CUPTI_FAILED(cuptiProfilerSetConfig(&set_config_params), sdk_retval, {
        GW_WARN_C("failed to config the created profile session");
        retval = GW_FAILED;
        goto exit;
    });

    GW_DEBUG_C(
        "create CUPTI session: "
        "device_id(%d), "
        "ctx(%p), "
        "max_launches_per_pass(%d), "
        "max_ranges_per_pass(%d), "
        "cupti_profile_range_mode(%s), "
        "cupti_profile_reply_mode(%s), "
        "cupti_profile_min_nesting_level(%d), "
        "cupti_profile_num_nesting_levels(%d), "
        "cupti_profile_target_nesting_levels(%d)"
        ,
        gw_device_cuda->get_device_id(),
        gw_device_cuda->get_cu_context(),
        max_launches_per_pass,
        max_ranges_per_pass,
        cupti_profile_range_mode == 0 ? "auto" : "user",
        cupti_profile_reply_mode == 0 ? "kernel" : "user",
        cupti_profile_min_nesting_level,
        cupti_profile_num_nesting_levels,
        cupti_profile_target_nesting_levels
    );

exit:
    if(unlikely(this->_RangeProfile_is_session_created == true && retval != GW_SUCCESS)){
        GW_IF_FAILED(this->RangeProfile_destroy_session(), sdk_retval, {
            GW_WARN_C("failed to destory session, after something failed during creation");
        });
    }

    return retval;
}


gw_retval_t GWProfiler_CUDA::RangeProfile_destroy_session(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_UnsetConfig_Params unset_config_params 
        = { CUpti_Profiler_UnsetConfig_Params_STRUCT_SIZE };
    CUpti_Profiler_EndSession_Params end_session_params 
        = { CUpti_Profiler_EndSession_Params_STRUCT_SIZE };
    GWProfileDevice_CUDA *gw_device_cuda;

    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));

    unset_config_params.ctx = gw_device_cuda->get_cu_context();
    GW_IF_CUPTI_FAILED(cuptiProfilerUnsetConfig(&unset_config_params), sdk_retval, {
        GW_WARN_C("failed to unset the config of profile session");
        retval = GW_FAILED;
        goto exit;
    });

    end_session_params.ctx = gw_device_cuda->get_cu_context();
    GW_IF_CUPTI_FAILED(cuptiProfilerEndSession(&end_session_params), sdk_retval, {
        GW_WARN_C("failed to end the profile session");
        retval = GW_FAILED;
        goto exit;
    });

    this->_RangeProfile_is_session_created = false;

    GW_DEBUG_C(
        "close CUPTI session: device_id(%d), ctx(%p)",
        gw_device_cuda->get_device_id(),
        gw_device_cuda->get_cu_context()
    );

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::RangeProfile_begin_pass(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_BeginPass_Params RangeProfile_begin_pass_params 
        = { CUpti_Profiler_BeginPass_Params_STRUCT_SIZE };
    GWProfileDevice_CUDA *gw_device_cuda;

    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));
    GW_CHECK_POINTER(gw_device_cuda->get_cu_context());

    RangeProfile_begin_pass_params.ctx = gw_device_cuda->get_cu_context();
    GW_IF_CUPTI_FAILED(cuptiProfilerBeginPass(&RangeProfile_begin_pass_params), sdk_retval, {
        GW_WARN_C("failed to begin pass");
        retval = GW_FAILED;
        goto exit;
    });

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::RangeProfile_end_pass(bool& is_last_pass){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_EndPass_Params RangeProfile_end_pass_params 
        = { CUpti_Profiler_EndPass_Params_STRUCT_SIZE };
    GWProfileDevice_CUDA *gw_device_cuda;

    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));
    GW_CHECK_POINTER(gw_device_cuda->get_cu_context());

    RangeProfile_end_pass_params.ctx = gw_device_cuda->get_cu_context();
    GW_IF_CUPTI_FAILED(cuptiProfilerEndPass(&RangeProfile_end_pass_params), sdk_retval, {
        GW_WARN_C("failed to end pass");
        retval = GW_FAILED;
        goto exit;
    });

    is_last_pass = RangeProfile_end_pass_params.allPassesSubmitted;

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::RangeProfile_enable_profiling(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_EnableProfiling_Params RangeProfile_enable_profiling_params
        = { CUpti_Profiler_EnableProfiling_Params_STRUCT_SIZE };
    GWProfileDevice_CUDA *gw_device_cuda;

    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));
    GW_CHECK_POINTER(gw_device_cuda->get_cu_context());

    RangeProfile_enable_profiling_params.ctx = gw_device_cuda->get_cu_context();
    GW_IF_CUPTI_FAILED(cuptiProfilerEnableProfiling(&RangeProfile_enable_profiling_params), sdk_retval, {
        GW_WARN_C("failed to enable profiling");
        retval = GW_FAILED;
        goto exit;
    });

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::RangeProfile_disable_profiling(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_DisableProfiling_Params RangeProfile_disable_profiling_params 
        = { CUpti_Profiler_DisableProfiling_Params_STRUCT_SIZE };
    GWProfileDevice_CUDA *gw_device_cuda;

    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));
    GW_CHECK_POINTER(gw_device_cuda->get_cu_context());

    RangeProfile_disable_profiling_params.ctx = gw_device_cuda->get_cu_context();
    GW_IF_CUPTI_FAILED(cuptiProfilerDisableProfiling(&RangeProfile_disable_profiling_params), sdk_retval, {
        GW_WARN_C("failed to disable profiling");
        retval = GW_FAILED;
        goto exit;
    });

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::RangeProfile_push_range(std::string range_name){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_PushRange_Params RangeProfile_push_range_params 
        = { CUpti_Profiler_PushRange_Params_STRUCT_SIZE };
    GWProfileDevice_CUDA *gw_device_cuda;
    
    GW_ASSERT(range_name.size() > 0);
    GW_ASSERT(range_name.size() < kMaxRangeNameLength);
    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));
    GW_CHECK_POINTER(gw_device_cuda->get_cu_context());

    RangeProfile_push_range_params.ctx = gw_device_cuda->get_cu_context();
    RangeProfile_push_range_params.pRangeName = range_name.c_str();
    RangeProfile_push_range_params.rangeNameLength = range_name.size();
    GW_IF_CUPTI_FAILED(cuptiProfilerPushRange(&RangeProfile_push_range_params), sdk_retval, {
        GW_WARN_C("failed to push range: range_name(%s)", range_name.c_str());
        retval = GW_FAILED;
        goto exit;
    });

exit:
    return retval;
}

gw_retval_t GWProfiler_CUDA::RangeProfile_pop_range(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_PopRange_Params RangeProfile_pop_range_params 
        = { CUpti_Profiler_PopRange_Params_STRUCT_SIZE };
    GWProfileDevice_CUDA *gw_device_cuda;

    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));
    GW_CHECK_POINTER(gw_device_cuda->get_cu_context());

    RangeProfile_pop_range_params.ctx = gw_device_cuda->get_cu_context();
    GW_IF_CUPTI_FAILED(cuptiProfilerPopRange(&RangeProfile_pop_range_params), sdk_retval, {
        GW_WARN_C("failed to pop range");
        retval = GW_FAILED;
        goto exit;
    });

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::checkpoint(){
    using namespace NV::Cupti::Checkpoint;
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;   
    CUpti_Checkpoint* new_ckpt;
    GWProfileDevice_CUDA *gw_device_cuda;

    #if GWATCH_PRINT_DEBUG
        uint64_t s_tick, e_tick;
    #endif

    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));

    GW_CHECK_POINTER(new_ckpt = new CUpti_Checkpoint);
    memset(new_ckpt, 0, sizeof(CUpti_Checkpoint));

    new_ckpt->structSize = CUpti_Checkpoint_STRUCT_SIZE;
    GW_CHECK_POINTER(new_ckpt->ctx = gw_device_cuda->get_cu_context());
    new_ckpt->allowOverwrite = 0;
    new_ckpt->optimizations = CUPTI_CHECKPOINT_OPT_NONE;

    #if GWATCH_PRINT_DEBUG
        s_tick = GWUtilTscTimer::get_tsc();
    #endif
    GW_IF_CUPTI_FAILED(cuptiCheckpointSave(new_ckpt), sdk_retval, {
        GW_WARN_C("failed to checkpoint GPU memory state");
        retval = GW_FAILED_SDK;
        goto exit;
    });
    #if GWATCH_PRINT_DEBUG
        e_tick = GWUtilTscTimer::get_tsc();
    #endif

    this->_cupti_ckpts.push(new_ckpt);

    GW_DEBUG_C(
        "checkpoint memory GPU state: duration(%lf ms)",
        this->_tsc_timer.tick_to_ms(e_tick - s_tick)
    );

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::restore(bool do_pop){
    using namespace NV::Cupti::Checkpoint;
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Checkpoint* ckpt;
    #if GWATCH_PRINT_DEBUG
        uint64_t s_tick, e_tick;
    #endif

    GW_CHECK_POINTER(ckpt = this->_cupti_ckpts.top());

    #if GWATCH_PRINT_DEBUG
        s_tick = GWUtilTscTimer::get_tsc();
    #endif
    GW_IF_CUPTI_FAILED(cuptiCheckpointRestore(ckpt), sdk_retval, {
        GW_WARN_C("failed to restore GPU memory state");
        retval = GW_FAILED_SDK;
        goto exit;
    })
    #if GWATCH_PRINT_DEBUG
        e_tick = GWUtilTscTimer::get_tsc();
    #endif

    if(do_pop){
        GW_IF_CUPTI_FAILED(cuptiCheckpointFree(ckpt), sdk_retval, {
            GW_WARN_C("failed to free the checkpoint");
            retval = GW_FAILED_SDK;
            goto exit;
        })
        this->_cupti_ckpts.pop();
    }

    GW_DEBUG_C(
        "restore memory GPU state: duration(%lf ms)",
        this->_tsc_timer.tick_to_ms(e_tick - s_tick)
    );

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::free_checkpoint(){
    using namespace NV::Cupti::Checkpoint;
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Checkpoint* ckpt;

    if(this->_cupti_ckpts.empty()){
        goto exit;
    }

    GW_CHECK_POINTER(ckpt = this->_cupti_ckpts.top());
    GW_IF_CUPTI_FAILED(cuptiCheckpointFree(ckpt), sdk_retval, {
        GW_WARN_C("failed to free the checkpoint");
        retval = GW_FAILED_SDK;
        goto exit;
    })
    this->_cupti_ckpts.pop();

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::RangeProfile_flush_data(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_FlushCounterData_Params flush_counter_data_params
        = { CUpti_Profiler_FlushCounterData_Params_STRUCT_SIZE };
    GWProfileDevice_CUDA *gw_device_cuda;

    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));
    
    flush_counter_data_params.ctx = gw_device_cuda->get_cu_context();
    GW_IF_CUPTI_FAILED(cuptiProfilerFlushCounterData(&flush_counter_data_params), sdk_retval, {
        GW_WARN_C("failed to flush image");
        retval = GW_FAILED;
        goto exit;
    });

    if(unlikely(flush_counter_data_params.numRangesDropped != 0)){
        GW_WARN_C("failed to flush image, dropped range detected");
        retval = GW_FAILED;
        goto exit;
    }

    if(unlikely(flush_counter_data_params.numTraceBytesDropped != 0)){
        GW_WARN_C("failed to flush image, dropped trace bytes detected");
        retval = GW_FAILED;
        goto exit;
    }

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::reset_counter_data(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;

    GW_CHECK_POINTER(this->_image_counter_data);
    GW_IF_FAILED(this->_image_counter_data->fill(), sdk_retval, {
        throw GWException("failed to fill counter data image while reseting it");
    });

    return retval;
}


gw_retval_t GWProfiler_CUDA::RangeProfile_get_metrics(
    std::map<std::string, std::map<std::string, double>> &value_map
){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    uint64_t i, j;
    std::vector<uint8_t> scratch_buffer;
    NVPW_CUDA_MetricsEvaluator_CalculateScratchBufferSize_Params calculate_scratch_buffer_size_param 
        = {NVPW_CUDA_MetricsEvaluator_CalculateScratchBufferSize_Params_STRUCT_SIZE};
    NVPW_CUDA_MetricsEvaluator_Initialize_Params metric_evaluator_initialize_params 
        = {NVPW_CUDA_MetricsEvaluator_Initialize_Params_STRUCT_SIZE};
    NVPW_CounterData_GetNumRanges_Params get_num_ranges_params 
        = { NVPW_CounterData_GetNumRanges_Params_STRUCT_SIZE };
    NVPW_MetricsEvaluator_Destroy_Params metric_evaluator_destroy_params 
        = { NVPW_MetricsEvaluator_Destroy_Params_STRUCT_SIZE };
    NVPW_MetricsEvaluator* metric_evaluator = nullptr;
    std::string req_name;
    bool isolated = true;
    bool keep_instances = true;
    GWProfileDevice_CUDA *gw_device_cuda;

    GW_CHECK_POINTER(this->_image_counter_data->data());
    GW_CHECK_POINTER(this->_image_counter_availability->data());
    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));

    value_map.clear();

    // create scratch buffer
    calculate_scratch_buffer_size_param.pChipName = gw_device_cuda->get_chip_name().c_str();
    calculate_scratch_buffer_size_param.pCounterAvailabilityImage = this->_image_counter_availability->data();
    GW_IF_NVPW_FAILED(
        NVPW_CUDA_MetricsEvaluator_CalculateScratchBufferSize(&calculate_scratch_buffer_size_param),
        sdk_retval,
        {
            GW_WARN_C("failed to calculate scratch buffer size whiile evaluating metrics");
            retval = GW_FAILED;
            goto exit;
        }
    );
    scratch_buffer.resize(calculate_scratch_buffer_size_param.scratchBufferSize, 0);

    // initialize metric evaluator
    metric_evaluator_initialize_params.scratchBufferSize = scratch_buffer.size();
    metric_evaluator_initialize_params.pScratchBuffer = scratch_buffer.data();
    metric_evaluator_initialize_params.pChipName = gw_device_cuda->get_chip_name().c_str();
    metric_evaluator_initialize_params.pCounterAvailabilityImage = this->_image_counter_availability->data();
    metric_evaluator_initialize_params.pCounterDataImage = this->_image_counter_data->data();
    metric_evaluator_initialize_params.counterDataImageSize = this->_image_counter_data->size();
    GW_IF_NVPW_FAILED(
        NVPW_CUDA_MetricsEvaluator_Initialize(&metric_evaluator_initialize_params),
        sdk_retval,
        {
            GW_WARN_C("failed to initialize metric evaluator");
            retval = GW_FAILED;
            goto exit;
        }
    );
    GW_CHECK_POINTER(metric_evaluator = metric_evaluator_initialize_params.pMetricsEvaluator);

    // get number of range inside the flushed data
    get_num_ranges_params.pCounterDataImage = this->_image_counter_data->data();
    GW_IF_NVPW_FAILED(
        NVPW_CounterData_GetNumRanges(&get_num_ranges_params),
        sdk_retval,
        {
            GW_WARN_C("failed to get number of ranges");
            retval = GW_FAILED;
            goto exit;
        }
    );

    // iterate over profiled metric names
    for (std::string metric_name : this->_metric_names){
        NVPW_MetricEvalRequest metric_eval_request;
        NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params convert_metric_to_eval_request 
            = {NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params_STRUCT_SIZE};

        // convert metric name to eval request
        NV::Metric::Parser::ParseMetricNameString(metric_name, &req_name, &isolated, &keep_instances);
        convert_metric_to_eval_request.pMetricsEvaluator = metric_evaluator;
        convert_metric_to_eval_request.pMetricName = req_name.c_str();
        convert_metric_to_eval_request.pMetricEvalRequest = &metric_eval_request;
        convert_metric_to_eval_request.metricEvalRequestStructSize = NVPW_MetricEvalRequest_STRUCT_SIZE;
        GW_IF_NVPW_FAILED(
            NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest(&convert_metric_to_eval_request),
            sdk_retval,
            {
                GW_WARN_C(
                    "failed to convert metric name to eval request: metric_name(%s)",
                    metric_name.c_str()
                );
                retval = GW_FAILED;
                goto exit;
            }
        );

        // iterate over ranges
        for (i=0; i<get_num_ranges_params.numRanges; i++){
            std::string range_name;
            std::vector<const char*> description_ptrs;
            double metric_value;
            NVPW_Profiler_CounterData_GetRangeDescriptions_Params get_range_desc_params
                = { NVPW_Profiler_CounterData_GetRangeDescriptions_Params_STRUCT_SIZE };
            NVPW_MetricsEvaluator_SetDeviceAttributes_Params set_device_attrib_params 
                = { NVPW_MetricsEvaluator_SetDeviceAttributes_Params_STRUCT_SIZE };
            NVPW_MetricsEvaluator_EvaluateToGpuValues_Params evaluate_to_gpu_values_params 
                = { NVPW_MetricsEvaluator_EvaluateToGpuValues_Params_STRUCT_SIZE };

            // form the range name
            get_range_desc_params.pCounterDataImage = this->_image_counter_data->data();
            get_range_desc_params.rangeIndex = i;
            GW_IF_NVPW_FAILED(
                NVPW_Profiler_CounterData_GetRangeDescriptions(&get_range_desc_params),
                sdk_retval,
                {
                    GW_WARN_C(
                        "failed to obtain size of range descrption: metric_name(%s), range_id(%lu)",
                        metric_name.c_str(), i
                    );
                    retval = GW_FAILED;
                    goto exit;
                }
            );
            description_ptrs.resize(get_range_desc_params.numDescriptions);
            get_range_desc_params.ppDescriptions = description_ptrs.data();
            GW_IF_NVPW_FAILED(
                NVPW_Profiler_CounterData_GetRangeDescriptions(&get_range_desc_params),
                sdk_retval,
                {
                    GW_WARN_C(
                        "failed to obtain range descrption: metric_name(%s), range_id(%lu)",
                        metric_name.c_str(), i
                    );
                    retval = GW_FAILED;
                    goto exit;
                }
            );
            for (j = 0; j < get_range_desc_params.numDescriptions; j++){
                if(j){
                    range_name += "/";
                }
                range_name += description_ptrs[j];
            }

            // set device attributes
            set_device_attrib_params.pMetricsEvaluator = metric_evaluator;
            set_device_attrib_params.pCounterDataImage = this->_image_counter_data->data();
            set_device_attrib_params.counterDataImageSize = this->_image_counter_data->size();
            GW_IF_NVPW_FAILED(
                NVPW_MetricsEvaluator_SetDeviceAttributes(&set_device_attrib_params),
                sdk_retval,
                {
                    GW_WARN_C(
                        "failed to set device attribute before evalute metric: metric_name(%s), range_id(%lu)",
                        metric_name.c_str(), i
                    );
                    retval = GW_FAILED;
                    goto exit;
                }
            );

            // evaluate to gpu metrics
            evaluate_to_gpu_values_params.pMetricsEvaluator = metric_evaluator;
            evaluate_to_gpu_values_params.pMetricEvalRequests = &metric_eval_request;
            evaluate_to_gpu_values_params.numMetricEvalRequests = 1;
            evaluate_to_gpu_values_params.metricEvalRequestStructSize = NVPW_MetricEvalRequest_STRUCT_SIZE;
            evaluate_to_gpu_values_params.metricEvalRequestStrideSize = sizeof(NVPW_MetricEvalRequest);
            evaluate_to_gpu_values_params.pCounterDataImage = this->_image_counter_data->data();
            evaluate_to_gpu_values_params.counterDataImageSize = this->_image_counter_data->size();
            evaluate_to_gpu_values_params.rangeIndex = i;
            evaluate_to_gpu_values_params.isolated = true;
            evaluate_to_gpu_values_params.pMetricValues = &metric_value;
            GW_IF_NVPW_FAILED(
                NVPW_MetricsEvaluator_EvaluateToGpuValues(&evaluate_to_gpu_values_params),
                sdk_retval,
                {
                    GW_WARN_C(
                        "failed to evaluate to gpu value: metric_name(%s), range_id(%lu)",
                        metric_name.c_str(), i
                    );
                    retval = GW_FAILED;
                    goto exit;
                }
            );

            value_map[range_name][metric_name] = metric_value;
        } // iterate over ranges
    } // iterate over profiled metric names

exit:
    if(likely(metric_evaluator != nullptr)){
        metric_evaluator_destroy_params.pMetricsEvaluator = metric_evaluator;
        GW_IF_NVPW_FAILED(
            NVPW_MetricsEvaluator_Destroy(&metric_evaluator_destroy_params),
            sdk_retval,
            {
                GW_WARN_C("failed to destory metric evaluator");
                retval = GW_FAILED;
            }
        );
    }

    return retval;
}


#if GW_CUDA_VERSION_MAJOR >= 12 && GW_CUDA_VERSION_MINOR >= 6


gw_retval_t GWProfiler_CUDA::PmSampling_enable_profiling(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;

    CUpti_PmSampling_Enable_Params enableParams {
        CUpti_PmSampling_Enable_Params_STRUCT_SIZE
    };

    if(this->_cupti_pm_sampler == nullptr){
        enableParams.deviceIndex = this->_gw_device->get_device_id();
        GW_IF_CUPTI_FAILED(
            cuptiPmSamplingEnable(&enableParams),
            sdk_retval,
            {
                GW_WARN_C("failed to enable pm sampling");
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );
        GW_CHECK_POINTER(this->_cupti_pm_sampler = enableParams.pPmSamplingObject);
        GW_DEBUG_C("created pm sampler");
    } else {
        GW_WARN_C(
            "when enabling pm sampling, "
            "pm sampler already exists, there might be some problems in your program"
        );
    }

    GW_DEBUG_C(
        "enabled pm sampling: device_id(%d)", this->_gw_device->get_device_id()
    );

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::PmSampling_disable_profiling(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_PmSampling_Disable_Params disableParams {
        CUpti_PmSampling_Disable_Params_STRUCT_SIZE
    };

    if(this->_cupti_pm_sampler != nullptr){
        disableParams.pPmSamplingObject = this->_cupti_pm_sampler;
        GW_IF_CUPTI_FAILED(
            cuptiPmSamplingDisable(&disableParams),
            sdk_retval,
            {
                GW_WARN_C("failed to disable pm sampling");
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );
        this->_cupti_pm_sampler = nullptr;
    } else {
        GW_WARN_C(
            "failed to disable pm sampling, "
            "pm sampler does not exist, there might be some problems in your program"
        );
    }

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::PmSampling_set_config(uint64_t hw_buf_size, uint64_t sampling_interval, uint32_t max_samples){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    std::vector<const char*>  metric_names_;
    CUpti_PmSampling_SetConfig_Params setConfigParams = {
        CUpti_PmSampling_SetConfig_Params_STRUCT_SIZE
    };
    
    GW_CHECK_POINTER(this->_image_pm_sampling_config);

    // setup config
    setConfigParams.pPmSamplingObject = this->_cupti_pm_sampler;
    setConfigParams.configSize = this->_image_pm_sampling_config->size();
    setConfigParams.pConfig = this->_image_pm_sampling_config->data();
    setConfigParams.hardwareBufferSize = hw_buf_size;
    setConfigParams.samplingInterval = sampling_interval;
    setConfigParams.triggerMode = CUpti_PmSampling_TriggerMode::CUPTI_PM_SAMPLING_TRIGGER_MODE_GPU_SYSCLK_INTERVAL;
    GW_IF_CUPTI_FAILED(
        cuptiPmSamplingSetConfig(&setConfigParams),
        sdk_retval,
        {
            GW_WARN_C("failed to set pm sampling config");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

    // create a new counter data image
    if(this->_image_pm_sampling_counter_data != nullptr){ delete this->_image_pm_sampling_counter_data; }
    GW_CHECK_POINTER(
        this->_image_pm_sampling_counter_data = new GWProfileImage_CUDA_PmSampling_CounterData(
            this->_gw_device->get_device_id()
        )
    );
    for(auto& metric_name : this->_metric_names){ metric_names_.push_back(metric_name.c_str()); }
    GW_IF_FAILED(
        this->_image_pm_sampling_counter_data->fill(metric_names_, max_samples, this->_cupti_pm_sampler),
        retval,
        {
            GW_WARN_C("failed to fill pm sampling counter data image");
            goto exit;
        }
    );

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::PmSampling_start_profiling(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_PmSampling_Start_Params startProfilingParams = {
        CUpti_PmSampling_Start_Params_STRUCT_SIZE
    };

    if(this->_cupti_pm_sampler != nullptr){
        startProfilingParams.pPmSamplingObject = this->_cupti_pm_sampler;
        GW_IF_CUPTI_FAILED(
            cuptiPmSamplingStart(&startProfilingParams),
            sdk_retval,
            {
                GW_WARN_C("failed to start pm sampling");
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );
    } else {
        GW_WARN_C(
            "failed to start pm sampling, "
            "pm sampler does not exist, there might be some problems in your program"
        );
    }

    // clear old result (if any)
    this->_PmSampling_map_metrics.clear();

    // start decoding thread
    this->_PmSampling_is_profiling = true;
    GW_CHECK_POINTER(
        this->_PmSampling_decode_thread = new std::thread(&GWProfiler_CUDA::__PmSampling_decode_counter_data, this)
    );

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::PmSampling_stop_profiling(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_PmSampling_Stop_Params stopProfilingParams = {
        CUpti_PmSampling_Stop_Params_STRUCT_SIZE
    };

    GW_CHECK_POINTER(this->_PmSampling_decode_thread);

    stopProfilingParams.pPmSamplingObject = this->_cupti_pm_sampler;
    GW_IF_CUPTI_FAILED(
        cuptiPmSamplingStop(&stopProfilingParams),
        sdk_retval,
        {
            GW_WARN_C("failed to stop pm sampling");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

    // stop decoding thread
    this->_PmSampling_is_profiling = false;
    if(this->_PmSampling_decode_thread->joinable()){
        this->_PmSampling_decode_thread->join();
    }
    delete this->_PmSampling_decode_thread;
    this->_PmSampling_decode_thread = nullptr;

exit:
    return retval;
}



gw_retval_t GWProfiler_CUDA::PmSampling_get_metrics(std::vector<gw_profiler_pm_sample_t> &map_metrics){
    GW_ASSERT(this->_PmSampling_decode_thread == nullptr and this->_PmSampling_is_profiling == false);
    map_metrics = this->_PmSampling_map_metrics;
    return GW_SUCCESS;
}


void GWProfiler_CUDA::__PmSampling_decode_counter_data() {
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    uint64_t i, j;
    CUpti_PmSampling_DecodeData_Params decodeDataParams = {
        CUpti_PmSampling_DecodeData_Params_STRUCT_SIZE
    };
    CUpti_PmSampling_GetCounterDataInfo_Params counterDataInfo {
        CUpti_PmSampling_GetCounterDataInfo_Params_STRUCT_SIZE
    };
    std::vector<const char*>  metric_names_;

    GW_CHECK_POINTER(this->_cupti_pm_sampler);
    GW_CHECK_POINTER(this->_pm_sampling_current__host_profiler);

    for(auto& metric_name : this->_metric_names){ metric_names_.push_back(metric_name.c_str()); }

    while(this->_PmSampling_is_profiling){
        decodeDataParams.pPmSamplingObject = this->_cupti_pm_sampler;
        decodeDataParams.pCounterDataImage = this->_image_pm_sampling_counter_data->data();
        decodeDataParams.counterDataImageSize = this->_image_pm_sampling_counter_data->size();
        GW_DEBUG("pPmSamplingObject(%p), pCounterDataImage.size(%lu)", this->_cupti_pm_sampler, this->_image_pm_sampling_counter_data->size());
        GW_IF_CUPTI_FAILED(
            cuptiPmSamplingDecodeData(&decodeDataParams),
            sdk_retval,
            {
                GW_WARN_C("failed to decode pm sampling counter data");
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );

        counterDataInfo.pCounterDataImage = this->_image_pm_sampling_counter_data->data();
        counterDataInfo.counterDataImageSize = this->_image_pm_sampling_counter_data->size();
        GW_IF_CUPTI_FAILED(
            cuptiPmSamplingGetCounterDataInfo(&counterDataInfo),
            sdk_retval,
            {
                GW_WARN_C("failed to get pm sampling counter data info");
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );

        
        for(i = 0; i < counterDataInfo.numCompletedSamples; i++){
            this->_PmSampling_map_metrics.push_back(gw_profiler_pm_sample_t{});
            gw_profiler_pm_sample_t& sample = this->_PmSampling_map_metrics.back();

            CUpti_PmSampling_CounterData_GetSampleInfo_Params getSampleInfoParams = {
                CUpti_PmSampling_CounterData_GetSampleInfo_Params_STRUCT_SIZE
            };
            CUpti_Profiler_Host_EvaluateToGpuValues_Params evalauateToGpuValuesParams {
                CUpti_Profiler_Host_EvaluateToGpuValues_Params_STRUCT_SIZE
            };
            std::vector<double> metric_results(metric_names_.size());

            // obtain timestamp
            getSampleInfoParams.pPmSamplingObject = this->_cupti_pm_sampler;
            getSampleInfoParams.pCounterDataImage = this->_image_pm_sampling_counter_data->data();
            getSampleInfoParams.counterDataImageSize = this->_image_pm_sampling_counter_data->size();
            getSampleInfoParams.sampleIndex = i;
            GW_IF_CUPTI_FAILED(
                cuptiPmSamplingCounterDataGetSampleInfo(&getSampleInfoParams),
                sdk_retval,
                {
                    GW_WARN_C("failed to get sample timestamp from pm sampling image");
                    retval = GW_FAILED_SDK;
                    goto exit;
                }
            );
            sample.s_tick = getSampleInfoParams.startTimestamp;
            sample.e_tick = getSampleInfoParams.endTimestamp;

            // obtain metric values
            evalauateToGpuValuesParams.pHostObject = this->_pm_sampling_current__host_profiler;
            evalauateToGpuValuesParams.pCounterDataImage = this->_image_pm_sampling_counter_data->data();
            evalauateToGpuValuesParams.counterDataImageSize = this->_image_pm_sampling_counter_data->size();
            evalauateToGpuValuesParams.ppMetricNames = metric_names_.data();
            evalauateToGpuValuesParams.numMetrics = metric_names_.size();
            evalauateToGpuValuesParams.rangeIndex = i;
            evalauateToGpuValuesParams.pMetricValues = metric_results.data();
            GW_IF_CUPTI_FAILED(
                cuptiProfilerHostEvaluateToGpuValues(&evalauateToGpuValuesParams),
                sdk_retval,
                {
                    GW_WARN_C("failed to get sample metric value from pm sampling image");
                    retval = GW_FAILED_SDK;
                    goto exit;
                }
            );

            for (j = 0; j < metric_names_.size(); ++j) {
                sample.map_emtrics[metric_names_[j]] = metric_results[j];
            }
        }
    }

exit:
    // return retval;
    ;
}

#endif


#if GW_CUDA_VERSION_MAJOR >= 9


gw_retval_t GWProfiler_CUDA::PcSampling_enable_profiling(const GWKernelDef* kernel_def){
    gw_retval_t retval = GW_SUCCESS;
    uint64_t i = 0, nb_stall_reasons = 0;
    int sdk_retval = 0;
    GWProfileDevice_CUDA *gw_device_cuda = nullptr;
    std::vector<uint32_t> tmp_list_pc_sampling_stall_reason_index;
    std::vector<char*> tmp_list_pc_sampling_stall_reason_string;
    CUpti_PCSamplingGetNumStallReasonsParams numStallReasonsParams = {};
    CUpti_PCSamplingEnableParams pcSamplingEnableParams = {};
    CUpti_PCSamplingGetStallReasonsParams stallReasonsParams = {};
    CUpti_PCSamplingConfigurationInfo enableStartStop = {};
    CUpti_PCSamplingConfigurationInfo samplingDataBuffer = {};
     CUpti_PCSamplingConfigurationInfo hardwareBufferSize = {};
    std::vector<CUpti_PCSamplingConfigurationInfo> pcSamplingConfigurationInfo;
    CUpti_PCSamplingConfigurationInfoParams pcSamplingConfigurationInfoParams = {};

    GW_CHECK_POINTER(kernel_def);
    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));

    pcSamplingEnableParams.size = CUpti_PCSamplingEnableParamsSize;
    pcSamplingEnableParams.ctx = gw_device_cuda->get_cu_context();

    // enable pc sampling
    GW_IF_CUPTI_FAILED(
        cuptiPCSamplingEnable(&pcSamplingEnableParams),
        sdk_retval,
        {
            GW_WARN_C(
                "failed to enable cupti pc sampling: sdk_retval(%d)",
                sdk_retval
            );
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

    if(this->_map_pc_sampling_stall_reason.size() == 0){
        // get number of stall reasons on this device
        numStallReasonsParams.size = CUpti_PCSamplingGetNumStallReasonsParamsSize;
        numStallReasonsParams.ctx = gw_device_cuda->get_cu_context();
        numStallReasonsParams.numStallReasons = &nb_stall_reasons;
        GW_IF_CUPTI_FAILED(
            cuptiPCSamplingGetNumStallReasons(&numStallReasonsParams),
            sdk_retval,
            {
                GW_WARN_C(
                    "failed to obtain number of stall reasons of cupti pc sampling: sdk_retval(%d)",
                    sdk_retval
                );
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );

        // allocate area for storing stall reasons
        tmp_list_pc_sampling_stall_reason_index.resize(nb_stall_reasons);
        tmp_list_pc_sampling_stall_reason_string.resize(nb_stall_reasons);
        for (i = 0; i < nb_stall_reasons; i++){
            tmp_list_pc_sampling_stall_reason_string[i] = (char *)calloc(CUPTI_STALL_REASON_STRING_SIZE, sizeof(char));
            GW_CHECK_POINTER(tmp_list_pc_sampling_stall_reason_string[i]);
        }

        // fill stall reasons index and string
        stallReasonsParams.size = CUpti_PCSamplingGetStallReasonsParamsSize;
        stallReasonsParams.ctx = gw_device_cuda->get_cu_context();
        stallReasonsParams.numStallReasons = nb_stall_reasons;
        stallReasonsParams.stallReasonIndex = tmp_list_pc_sampling_stall_reason_index.data();
        stallReasonsParams.stallReasons = tmp_list_pc_sampling_stall_reason_string.data();
        GW_IF_CUPTI_FAILED(
            cuptiPCSamplingGetStallReasons(&stallReasonsParams),
            sdk_retval,
            {
                GW_WARN_C(
                    "failed to obtain stall reasons of cupti pc sampling: sdk_retval(%d)",
                    sdk_retval
                );
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );

        // record to current profiler
        for(i=0; i<nb_stall_reasons; i++){
            this->_map_pc_sampling_stall_reason[tmp_list_pc_sampling_stall_reason_index[i]] 
                = tmp_list_pc_sampling_stall_reason_string[i];
        }
    } else {
        nb_stall_reasons = this->_map_pc_sampling_stall_reason.size();
    }

    // allocate area for storing profiling data
    if(unlikely(this->_pc_sampling_data_for_sampling.pPcData != nullptr)){
        free(this->_pc_sampling_data_for_sampling.pPcData);
    }
    this->_pc_sampling_data_for_sampling.size = sizeof(CUpti_PCSamplingData);
    this->_pc_sampling_data_for_sampling.collectNumPcs = kernel_def->list_instructions.size();
    this->_pc_sampling_data_for_sampling.pPcData = (CUpti_PCSamplingPCData *)calloc(
        this->_pc_sampling_data_for_sampling.collectNumPcs, sizeof(CUpti_PCSamplingPCData)
    );
    GW_CHECK_POINTER(this->_pc_sampling_data_for_sampling.pPcData);
    for(i = 0; i < this->_pc_sampling_data_for_sampling.collectNumPcs; i++){
        this->_pc_sampling_data_for_sampling.pPcData[i].stallReason = (CUpti_PCSamplingStallReason *)calloc(
            nb_stall_reasons, sizeof(CUpti_PCSamplingStallReason)
        );
        GW_CHECK_POINTER(this->_pc_sampling_data_for_sampling.pPcData[i].stallReason);
    }

    // PC Sampling configuration attributes
    enableStartStop.attributeType = CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_ENABLE_START_STOP_CONTROL;
    enableStartStop.attributeData.enableStartStopControlData.enableStartStopControl = true;

    samplingDataBuffer.attributeType = CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_SAMPLING_DATA_BUFFER;
    samplingDataBuffer.attributeData.samplingDataBufferData.samplingDataBuffer = (void *)&
    (this->_pc_sampling_data_for_sampling);

    // NOTE(zhuobin): this is necessary, otherwise the hardware buffer size will be full and cause failure
    //                  during collecting
    // REF(zhuobin): https://docs.nvidia.com/cupti/api/structCUpti__PCSamplingData.html#_CPPv4N20CUpti_PCSamplingData18hardwareBufferFullE
    hardwareBufferSize.attributeType = CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_HARDWARE_BUFFER_SIZE;
    hardwareBufferSize.attributeData.hardwareBufferSizeData.hardwareBufferSize = GB(2);

    pcSamplingConfigurationInfo.push_back(enableStartStop);
    pcSamplingConfigurationInfo.push_back(samplingDataBuffer);
    pcSamplingConfigurationInfo.push_back(hardwareBufferSize);

    pcSamplingConfigurationInfoParams.size = CUpti_PCSamplingConfigurationInfoParamsSize;
    pcSamplingConfigurationInfoParams.ctx = gw_device_cuda->get_cu_context();
    pcSamplingConfigurationInfoParams.numAttributes = pcSamplingConfigurationInfo.size();
    pcSamplingConfigurationInfoParams.pPCSamplingConfigurationInfo = pcSamplingConfigurationInfo.data();
    GW_IF_CUPTI_FAILED(
        cuptiPCSamplingSetConfigurationAttribute(&pcSamplingConfigurationInfoParams),
        sdk_retval,
        {
            GW_WARN_C(
                "failed to set configuration attribute of cupti pc sampling: sdk_retval(%d)",
                sdk_retval
            );
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

exit:
    for (i = 0; i < tmp_list_pc_sampling_stall_reason_string.size(); i++){
        if(tmp_list_pc_sampling_stall_reason_string[i] != nullptr){
            free(tmp_list_pc_sampling_stall_reason_string[i]);
        }
    }
    return retval;
}


gw_retval_t GWProfiler_CUDA::PcSampling_disable_profiling(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval = 0;
    GWProfileDevice_CUDA *gw_device_cuda = nullptr;
    CUpti_PCSamplingDisableParams pcSamplingDisableParams = {};

    GW_CHECK_POINTER(this->_pc_sampling_data_for_sampling.pPcData);
    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));

    pcSamplingDisableParams.size = CUpti_PCSamplingDisableParamsSize;
    pcSamplingDisableParams.ctx = gw_device_cuda->get_cu_context();
    GW_IF_CUPTI_FAILED(
        cuptiPCSamplingDisable(&pcSamplingDisableParams),
        sdk_retval,
        {
            GW_WARN_C(
                "failed to disable pc sampling: sdk_retval(%d)",
                sdk_retval
            );
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::PcSampling_start_profiling(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval = 0;
    GWProfileDevice_CUDA *gw_device_cuda = nullptr;
    CUpti_PCSamplingStartParams pcSamplingStartParams = {};

    GW_CHECK_POINTER(this->_pc_sampling_data_for_sampling.pPcData);
    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));

    pcSamplingStartParams.size = CUpti_PCSamplingStartParamsSize;
    pcSamplingStartParams.ctx = gw_device_cuda->get_cu_context();
    GW_IF_CUPTI_FAILED(
        cuptiPCSamplingStart(&pcSamplingStartParams),
        sdk_retval,
        {
            GW_WARN_C(
                "failed to start pc sampling: sdk_retval(%d)",
                sdk_retval
            );
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::PcSampling_stop_profiling(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval = 0;
    GWProfileDevice_CUDA *gw_device_cuda = nullptr;
    CUpti_PCSamplingStopParams pcSamplingStopParams = {};

    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));

    pcSamplingStopParams.size = CUpti_PCSamplingStopParamsSize;
    pcSamplingStopParams.ctx = gw_device_cuda->get_cu_context();
    GW_IF_CUPTI_FAILED(
        cuptiPCSamplingStop(&pcSamplingStopParams),
        sdk_retval,
        {
            GW_WARN_C(
                "failed to stop pc sampling: sdk_retval(%d)",
                sdk_retval
            );
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

exit:
    return retval;
}


gw_retval_t GWProfiler_CUDA::PcSampling_get_metrics(std::map<uint64_t, std::map<uint32_t, uint64_t>>& map_metrics){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval = 0;
    uint64_t i=0, j=0, nb_stall_reasons=0, pc_offset = 0;
    CUpti_PCSamplingGetDataParams pcSamplingGetDataParams = {};
    GWProfileDevice_CUDA *gw_device_cuda = nullptr;

    GW_CHECK_POINTER(this->_pc_sampling_data_for_sampling.pPcData);
    GW_CHECK_POINTER(gw_device_cuda = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_device));

    nb_stall_reasons = this->_map_pc_sampling_stall_reason.size();

    // allocate area for collecting profiling data
    if(unlikely(this->_pc_sampling_data_for_collecting.pPcData != nullptr)){
        free(this->_pc_sampling_data_for_collecting.pPcData);
    }
    this->_pc_sampling_data_for_collecting.size = sizeof(CUpti_PCSamplingData);
    this->_pc_sampling_data_for_collecting.collectNumPcs 
        = this->_pc_sampling_data_for_sampling.collectNumPcs;
    this->_pc_sampling_data_for_collecting.pPcData = (CUpti_PCSamplingPCData *)calloc(
        this->_pc_sampling_data_for_collecting.collectNumPcs, sizeof(CUpti_PCSamplingPCData)
    );
    GW_CHECK_POINTER(this->_pc_sampling_data_for_collecting.pPcData);
    for(i = 0; i < this->_pc_sampling_data_for_collecting.collectNumPcs; i++){
        this->_pc_sampling_data_for_collecting.pPcData[i].stallReason = (CUpti_PCSamplingStallReason *)calloc(
            nb_stall_reasons, sizeof(CUpti_PCSamplingStallReason)
        );
        GW_CHECK_POINTER(this->_pc_sampling_data_for_collecting.pPcData[i].stallReason);
    }

    // collect pc sampling data
    pcSamplingGetDataParams.size = CUpti_PCSamplingGetDataParamsSize;
    pcSamplingGetDataParams.ctx = gw_device_cuda->get_cu_context();
    pcSamplingGetDataParams.pcSamplingData = (void *)&(this->_pc_sampling_data_for_collecting);
    GW_IF_CUPTI_FAILED(
        cuptiPCSamplingGetData(&pcSamplingGetDataParams),
        sdk_retval,
        {
            GW_WARN_C(
                "failed to collect pc sampling data: sdk_retval(%d)",
                sdk_retval
            );
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

    // organize pc sampling data
    map_metrics.clear();
    for (i=0; i<this->_pc_sampling_data_for_collecting.totalNumPcs; i++){
        pc_offset = this->_pc_sampling_data_for_collecting.pPcData[i].pcOffset;
        map_metrics[pc_offset] = {};
        for (j=0; j<this->_pc_sampling_data_for_collecting.pPcData[i].stallReasonCount; j++){
            map_metrics
                [pc_offset]
                [this->_pc_sampling_data_for_collecting.pPcData[i].stallReason[j].pcSamplingStallReasonIndex]
                = this->_pc_sampling_data_for_collecting.pPcData[i].stallReason[j].samples;
        }
    }

exit:
    return retval;
}

#endif
