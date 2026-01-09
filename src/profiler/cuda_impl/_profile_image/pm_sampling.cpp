#include <iostream>
#include <filesystem>
#include <fstream>
#include <string.h>

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cupti_target.h>
#include <cupti_profiler_target.h>
#include <cupti_profiler_host.h>
#include <cupti_pmsampling.h>
#include <nvperf_host.h>

#include "common/cuda_impl/cupti/common/helper_cupti.h"
#include "common/cuda_impl/cupti/extensions/include/profilerhost_util/Eval.h"
#include "common/cuda_impl/cupti/extensions/include/profilerhost_util/Metric.h"
#include "common/cuda_impl/cupti/extensions/include/profilerhost_util/Utils.h"

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/timer.hpp"
#include "common/utils/hash.hpp"
#include "common/utils/string.hpp"

#include "profiler/cuda_impl/profiler.hpp"
#include "profiler/cuda_impl/profile_image.hpp"


#if GW_CUDA_VERSION_MAJOR >= 12 && GW_CUDA_VERSION_MINOR >= 6

#include <cupti_profiler_host.h>


GWProfileImage_CUDA_PmSampling_CounterData::GWProfileImage_CUDA_PmSampling_CounterData(int device_id)
    :   _device_id(device_id),
        GWProfileImage()
    {}


GWProfileImage_CUDA_PmSampling_CounterData::~GWProfileImage_CUDA_PmSampling_CounterData(){
    uint8_t i;
    int sdk_retval;

    for(i=0; i<this->_retained_device_context_time; i++){
        GW_IF_CUDA_DRIVER_FAILED(cuDevicePrimaryCtxRelease(this->_cu_device), sdk_retval, {
            GW_WARN_C("failed to release the primary context of device: device_id(%d)", this->_device_id);
        });
    }
}


gw_retval_t GWProfileImage_CUDA_PmSampling_CounterData::fill(
    std::vector<const char*> metric_names,
    uint32_t max_samples,
    CUpti_PmSampling_Object* cupti_pm_sampler
){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_PmSampling_GetCounterDataSize_Params getCounterDataSizeParams = {
        CUpti_PmSampling_GetCounterDataSize_Params_STRUCT_SIZE
    };
    CUpti_PmSampling_CounterDataImage_Initialize_Params initializeParams = {
        CUpti_PmSampling_CounterDataImage_Initialize_Params_STRUCT_SIZE
    };
    
    GW_IF_CUDA_DRIVER_FAILED(cuDeviceGet(&this->_cu_device, this->_device_id), sdk_retval, {
        GW_WARN_C("failed to obtain device handle: device_id(%d)", this->_device_id);
        retval = GW_FAILED;
        goto exit;
    });
    GW_IF_CUDA_DRIVER_FAILED(cuDevicePrimaryCtxRetain(&this->_cu_context, this->_cu_device), sdk_retval, {
        GW_WARN_C("failed to retain the primary context of device: device_id(%d)", this->_device_id);
        retval = GW_FAILED;
        goto exit;
    });
    this->_retained_device_context_time += 1;

    getCounterDataSizeParams.pPmSamplingObject = cupti_pm_sampler;
    getCounterDataSizeParams.numMetrics = metric_names.size();
    getCounterDataSizeParams.pMetricNames = metric_names.data();
    getCounterDataSizeParams.maxSamples  = max_samples;
    GW_IF_CUPTI_FAILED(
        cuptiPmSamplingGetCounterDataSize(&getCounterDataSizeParams),
        sdk_retval,
        {
            GW_WARN_C("failed to get pm sampling counter data size");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );
    this->_image.clear();
    this->_image.resize(getCounterDataSizeParams.counterDataSize);

    initializeParams.pPmSamplingObject = cupti_pm_sampler;
    initializeParams.counterDataSize = this->_image.size();
    initializeParams.pCounterData = this->_image.data();
    GW_IF_CUPTI_FAILED(
        cuptiPmSamplingCounterDataImageInitialize(&initializeParams),
        sdk_retval,
        {
            GW_WARN_C("failed to initialize pm sampling counter data image");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

exit:
    return retval;
}


GWProfileImage_CUDA_PmSampling_Config::GWProfileImage_CUDA_PmSampling_Config()
    : GWProfileImage()
    {}


GWProfileImage_CUDA_PmSampling_Config::~GWProfileImage_CUDA_PmSampling_Config()
    {}


gw_retval_t GWProfileImage_CUDA_PmSampling_Config::fill(CUpti_Profiler_Host_Object* host_profiler){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_Host_GetConfigImageSize_Params getConfigImageSizeParams {
        CUpti_Profiler_Host_GetConfigImageSize_Params_STRUCT_SIZE
    };
    CUpti_Profiler_Host_GetConfigImage_Params getConfigImageParams = {
        CUpti_Profiler_Host_GetConfigImage_Params_STRUCT_SIZE
    };

    GW_CHECK_POINTER(host_profiler);

    getConfigImageSizeParams.pHostObject = host_profiler;
    GW_IF_CUPTI_FAILED(
        cuptiProfilerHostGetConfigImageSize(&getConfigImageSizeParams),
        sdk_retval,
        {
            GW_WARN_C("failed to get config image size");
            retval = GW_FAILED_SDK;
            goto exit;
            
        }
    );
    this->_image.clear();
    this->_image.resize(getConfigImageSizeParams.configImageSize);
    
    getConfigImageParams.pHostObject = host_profiler;
    getConfigImageParams.pConfigImage = this->_image.data();
    getConfigImageParams.configImageSize = this->_image.size();
    GW_IF_CUPTI_FAILED(
        cuptiProfilerHostGetConfigImage(&getConfigImageParams),
        sdk_retval,
        {
            GW_WARN_C("failed to get config image");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    )

exit:
    return retval;
}


GWProfileImage_CUDA_PmSampling_CounterAvailability::GWProfileImage_CUDA_PmSampling_CounterAvailability(int device_id)
    :   _device_id(device_id),
        GWProfileImage()
    {}


GWProfileImage_CUDA_PmSampling_CounterAvailability::~GWProfileImage_CUDA_PmSampling_CounterAvailability()
    {}


gw_retval_t GWProfileImage_CUDA_PmSampling_CounterAvailability::fill(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;

    CUpti_PmSampling_GetCounterAvailability_Params getCounterAvailabilityParams { 
        CUpti_PmSampling_GetCounterAvailability_Params_STRUCT_SIZE 
    };
    getCounterAvailabilityParams.deviceIndex = this->_device_id;
    GW_IF_CUPTI_FAILED(
        cuptiPmSamplingGetCounterAvailability(&getCounterAvailabilityParams),
        sdk_retval,
        {
            GW_WARN_C(
                "failed to get counter availability for PC sampling in the first pass: device_id(%d)",
                this->_device_id
            );
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

    this->_image.clear();
    this->_image.resize(getCounterAvailabilityParams.counterAvailabilityImageSize);
    getCounterAvailabilityParams.pCounterAvailabilityImage = this->_image.data();
    GW_IF_CUPTI_FAILED(
        cuptiPmSamplingGetCounterAvailability(&getCounterAvailabilityParams),
        sdk_retval,
        {
            GW_WARN_C(
                "failed to get counter availability for PC sampling in the second pass: device_id(%d)",
                this->_device_id
            );
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

exit:
    return retval;
}

#endif
