#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <filesystem>

#include <cuda.h>
#include <nvml.h>
#include <cuda_runtime_api.h>
#include <cupti_target.h>
#include <cupti_profiler_target.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"

#include "profiler/cuda_impl/context.hpp"
#include "profiler/cuda_impl/device.hpp"
#include "profiler/cuda_impl/profiler.hpp"

#include "common/cuda_impl/cupti/extensions/include/profilerhost_util/Utils.h"


GWProfileContext_CUDA::GWProfileContext_CUDA(bool lazy_init_device) : GWProfileContext(lazy_init_device) {
    gw_retval_t retval = GW_SUCCESS;
    int i, sdk_retval;

    // initialize the CUDA driver
    GW_IF_CUDA_DRIVER_FAILED(cuInit(0), sdk_retval, {
        throw GWException("failed to initialize CUDA driver");
    });

    // init nvpw
    GW_IF_FAILED(this->__init_nvpw(), retval, {
        throw GWException("failed to initialize NVPW: %s", gw_retval_str(retval));
    });

    // init nvml
    GW_IF_FAILED(this->__init_nvml(), retval, {
        throw GWException("failed to initialize NVML: %s", gw_retval_str(retval));
    });

    // init profiler APIs
    GW_IF_FAILED(this->__init_profiler_apis(), retval, {
        throw GWException("failed to initialize CUPTI profiler APIs: %s", gw_retval_str(retval));
    });

    // make sure directories which used by GWProfileContext_CUDA exist
    GW_ASSERT(std::string(GW_DEFAULT_LOG_PATH).size() > 0);
    if(!std::filesystem::exists(GW_DEFAULT_LOG_PATH)){
        if (!std::filesystem::create_directories(GW_DEFAULT_LOG_PATH)) {
            throw GWException("failed to create cache path for GWProfileContext_CUDA: path(%s)", GW_DEFAULT_LOG_PATH);
        }
        GW_DEBUG("created cache path for GWProfileContext_CUDA: path(%s)", GW_DEFAULT_LOG_PATH);
    } else {
        GW_DEBUG("reused cache path for GWProfileContext_CUDA: path(%s)", GW_DEFAULT_LOG_PATH);
    }

    GW_IF_CUDA_DRIVER_FAILED(cudaGetDeviceCount(&this->_num_device), sdk_retval, {
        throw GWException("failed to detect number of CUDA device");
    });
    if(unlikely(this->_num_device == 0)){
        throw GWException("no CUDA device detected");
    }

    if(!lazy_init_device){
        for(i = 0; i < this->_num_device; i++){
            GW_IF_FAILED(
                this->__init_gw_device(i),
                retval,
                throw GWException("failed to initialize GWProfileDevice_CUDA: device_id(%d)");
            )
        }
    }

    GW_DEBUG("created gwatch instance");
}


GWProfileContext_CUDA::~GWProfileContext_CUDA(){}


gw_retval_t GWProfileContext_CUDA::__init_gw_device(int device_id){
    gw_retval_t retval = GW_SUCCESS;
    GWProfileDevice_CUDA *gw_device = nullptr;

    GW_ASSERT(device_id < this->_num_device);
    
    if(this->_gw_devices.count(device_id) == 0){
        GW_CHECK_POINTER(gw_device = new GWProfileDevice_CUDA(device_id));

        if(likely(gw_device->is_cupti_profiler_supported())){
            this->_gw_devices[device_id] = gw_device;
        } else {
            delete gw_device;
            GW_WARN_C(
                "failed to prepare profiling, device not exists or supports profile mode: device_id(%d)",
                device_id
            );
            retval = GW_FAILED_NOT_READY;
            goto exit;
        }
    }

exit:
    return retval;
}


gw_retval_t GWProfileContext_CUDA::get_clock_freq(int device_id, std::map<std::string, uint32_t>& clock_map){
    gw_retval_t retval = GW_SUCCESS;
    nvmlReturn_t nvml_retval;
    nvmlDevice_t device;
    uint32_t freq;

    clock_map.clear();

    if(unlikely(device_id >= this->_num_device)){
        GW_WARN_C(
            "invalid device id: device_id(%d), num_device(%d)",
            device_id, this->_num_device
        );
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }

    GW_IF_NVML_FAILED(
        nvmlDeviceGetHandleByIndex(device_id, &device),
        nvml_retval,
        {
            GW_WARN_C("failed to get nvml device handle");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    )
    
    GW_IF_NVML_FAILED(
        nvmlDeviceGetClock(device, NVML_CLOCK_GRAPHICS, NVML_CLOCK_ID_CURRENT, &freq),
        nvml_retval,
        {
            GW_WARN_C("failed to get nvml device handle");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    )
    clock_map["graphic.current"] = freq;

    GW_IF_NVML_FAILED(
        nvmlDeviceGetClock(device, NVML_CLOCK_SM, NVML_CLOCK_ID_CURRENT, &freq),
        nvml_retval,
        {
            GW_WARN_C("failed to get nvml device handle");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    )
    clock_map["sm.current"] = freq;

    GW_IF_NVML_FAILED(
        nvmlDeviceGetClock(device, NVML_CLOCK_MEM, NVML_CLOCK_ID_CURRENT, &freq),
        nvml_retval,
        {
            GW_WARN_C("failed to get nvml device handle");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    )
    clock_map["memory.current"] = freq;

    GW_IF_NVML_FAILED(
        nvmlDeviceGetClock(device, NVML_CLOCK_VIDEO, NVML_CLOCK_ID_CURRENT, &freq),
        nvml_retval,
        {
            GW_WARN_C("failed to get nvml device handle");
            retval = GW_FAILED_SDK;
            goto exit;
        }
    )
    clock_map["video.current"] = freq;

exit:
    return retval;
}


gw_retval_t GWProfileContext_CUDA::__init_profiler_apis(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_Initialize_Params profilerInitializeParams 
        = { CUpti_Profiler_Initialize_Params_STRUCT_SIZE };

    // initialize CUPTI profiler API
    GW_IF_CUPTI_FAILED(cuptiProfilerInitialize(&profilerInitializeParams), sdk_retval, {
        retval = GW_FAILED_SDK;
        goto exit;
    });

exit:
    return retval;
}


gw_retval_t GWProfileContext_CUDA::create_profiler(
    int device_id, const std::vector<std::string>& metric_names, gw_profiler_mode_t profiler_mode, GWProfiler** gw_profiler
){
    gw_retval_t retval = GW_SUCCESS;
    GWProfileDevice_CUDA *gw_device = nullptr;

    GW_CHECK_POINTER(gw_profiler);

    // initialize GWProfileDevice (lazyly initialized)
    GW_IF_FAILED(
        this->__init_gw_device(device_id),
        retval,
        {
            GW_WARN_C("failed to initialize GWProfileDevice_CUDA: device_id(%d)", device_id);
            goto exit;
        }
    );
    GW_CHECK_POINTER(gw_device = reinterpret_cast<GWProfileDevice_CUDA*>(this->_gw_devices[device_id]));

    // check params
    if(unlikely(profiler_mode == GWProfiler_Mode_CUDA_PM_Sampling && metric_names.size() == 0)){
        GW_WARN_C("failed to prepare pm profiling, no metric name provided");
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }
    if(unlikely(profiler_mode == GWProfiler_Mode_CUDA_Range_Profiling && metric_names.size() == 0)){
        GW_WARN_C("failed to prepare range profiling, no metric name provided");
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }

    GW_CHECK_POINTER(*gw_profiler = new GWProfiler_CUDA(gw_device, metric_names, profiler_mode));

exit:
    return retval;
}


void GWProfileContext_CUDA::destroy_profiler(GWProfiler* gw_profiler){
    GW_CHECK_POINTER(gw_profiler);
    delete gw_profiler;
}


gw_retval_t GWProfileContext_CUDA::__init_nvpw(){
    NVPA_Status nvpw_retval;
    gw_retval_t retval = GW_SUCCESS;
    NVPW_InitializeHost_Params initializeHostParams = { NVPW_InitializeHost_Params_STRUCT_SIZE };

    GW_IF_NVPW_FAILED(NVPW_InitializeHost(&initializeHostParams), nvpw_retval, {
        GW_WARN_C("failed to initialize NVPW: %s", NV::Metric::Utils::GetNVPWResultString(nvpw_retval));
        retval = GW_FAILED_SDK;
        goto exit;
    });

exit:
    return retval;
}

gw_retval_t GWProfileContext_CUDA::__init_nvml(){
    nvmlReturn_t nvml_retval;
    gw_retval_t retval = GW_SUCCESS;
    
    GW_IF_NVML_FAILED(
        nvmlInit(),
        nvml_retval,
        {
            GW_WARN_C("failed to initialize NVML: %s", nvmlErrorString(nvml_retval));
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

exit:
    return retval;
}
