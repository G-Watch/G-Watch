#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <string>

#include "yaml-cpp/yaml.h"

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cupti_target.h>
#include <cupti_profiler_target.h>

#include "common/cuda_impl/cupti/extensions/include/profilerhost_util/List.h"

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/timer.hpp"

#include "profiler/cuda_impl/device.hpp"
#include "profiler/cuda_impl/metric.hpp"


GWProfileDevice_CUDA::GWProfileDevice_CUDA(int device_id)
    : GWProfileDevice(device_id), _is_cupti_profiler_supported(false)
{
    int sdk_retval;

    CUpti_Profiler_DeviceSupported_Params params 
        = { CUpti_Profiler_DeviceSupported_Params_STRUCT_SIZE };
    CUpti_Device_GetChipName_Params getChipNameParams 
        = { CUpti_Device_GetChipName_Params_STRUCT_SIZE };
    CUpti_Profiler_GetCounterAvailability_Params getCounterAvailabilityParams 
        = {CUpti_Profiler_GetCounterAvailability_Params_STRUCT_SIZE};

    GW_DEBUG_C("initializing GWProfileDevice_CUDA...: device_id(%d)", device_id);

    // step 1: obtain metadata of the specified device
    GW_DEBUG_C("obtaining metadata of the device...: device_id(%d)", device_id);
    GW_IF_CUDA_DRIVER_FAILED(cuDeviceGet(&this->_cu_device, device_id), sdk_retval, {
        throw GWException("failed to get device handle: device_id(%d)", device_id);
    });
    GW_IF_CUDA_DRIVER_FAILED(
        cuDeviceGetAttribute(&this->_compute_capability_major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, this->_cu_device),
        sdk_retval, {
            throw GWException("failed to get compute capability major: device_id(%d)", device_id);
        }
    );
    GW_IF_CUDA_DRIVER_FAILED(
        cuDeviceGetAttribute(&this->_compute_capability_minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, this->_cu_device),
        sdk_retval, {
            throw GWException("failed to get compute capability minor: device_id(%d)", device_id);
        }
    );
    GW_BACK_LINE;
    GW_DEBUG_C(
        "obtained metadata of the device: device_id(%d), compute_capability(%d.%d)",
        device_id, this->_compute_capability_major, this->_compute_capability_minor
    );


    // step 2: check whether device is supported by CUPTI profiler
    GW_DEBUG_C("checking whether device supports CUPTI profiler...: device_id(%d)", device_id);
    params.cuDevice = device_id;
    GW_IF_CUPTI_FAILED(cuptiProfilerDeviceSupported(&params), sdk_retval, {
        throw GWException("failed to query profiler device support: device_id(%d)", device_id);
    })
    if (unlikely(params.isSupported != CUPTI_PROFILER_CONFIGURATION_SUPPORTED)){
        if(params.architecture == CUPTI_PROFILER_CONFIGURATION_UNSUPPORTED){
            GW_WARN_C(
                "found device doesn't support CUPTI profiler, architecture not supported: device_id(%d), compute_capability(%d.%d)",
                device_id, this->_compute_capability_major, this->_compute_capability_minor
            );
        }
        if(params.sli == CUPTI_PROFILER_CONFIGURATION_UNSUPPORTED){
            GW_WARN_C(
                "failed to init CUPTI profiling APIs, SLI not supported: device_id(%d), compute_capability(%d.%d)",
                device_id, this->_compute_capability_major, this->_compute_capability_minor
            );
        }
        if(params.vGpu == CUPTI_PROFILER_CONFIGURATION_UNSUPPORTED){
            GW_WARN_C(
                "failed to init CUPTI profiling APIs, vGpu not supported: device_id(%d), compute_capability(%d.%d)",
                device_id, this->_compute_capability_major, this->_compute_capability_minor
            );
        } else if(params.vGpu == CUPTI_PROFILER_CONFIGURATION_DISABLED){
            GW_WARN_C(
                "failed to init CUPTI profiling APIs, vGpu disabled profiling support: device_id(%d), compute_capability(%d.%d)",
                device_id, this->_compute_capability_major, this->_compute_capability_minor
            );
        }
        if(params.confidentialCompute == CUPTI_PROFILER_CONFIGURATION_UNSUPPORTED){
            GW_WARN_C(
                "failed to init CUPTI profiling APIs, Confidential Compute not supported: device_id(%d), compute_capability(%d.%d)",
                device_id, this->_compute_capability_major, this->_compute_capability_minor
            );
        }
        if(params.cmp == CUPTI_PROFILER_CONFIGURATION_UNSUPPORTED){
            GW_WARN_C(
                "failed to init CUPTI profiling APIs, NVIDIA Crypto Mining Processors (CMP) not supported: device_id(%d), compute_capability(%d.%d)",
                device_id, this->_compute_capability_major, this->_compute_capability_minor
            );
        }
        // not capatible while using CUDA 11, no params.wsl field
        // if(params.wsl == CUPTI_PROFILER_CONFIGURATION_UNSUPPORTED){
        //     GW_WARN_C(
        //         "failed to init CUPTI profiling APIs, WSL not supported: device_id(%d), compute_capability(%d.%d)",
        //         device_id, this->_compute_capability_major, this->_compute_capability_minor
        //     );
        // }
        goto exit;
    } else {
        this->_is_cupti_profiler_supported = true;
    }
    GW_BACK_LINE;
    GW_DEBUG_C("device supports CUPTI profiler...: device_id(%d), compute_capability(%d.%d)",
        device_id, this->_compute_capability_major, this->_compute_capability_minor
    );


    // step 3: obtain the counter available image of the device
    GW_DEBUG_C("obtaining counter available image of the device...: device_id(%d)", device_id);
    GW_IF_CUDA_DRIVER_FAILED(cuDevicePrimaryCtxRetain(&this->_cu_context, this->_cu_device), sdk_retval, {
        throw GWException("failed to retain the primary context of device: device_id(%d)", device_id);
    });
    getCounterAvailabilityParams.ctx = this->_cu_context;
    GW_IF_CUPTI_FAILED(cuptiProfilerGetCounterAvailability(&getCounterAvailabilityParams), sdk_retval, {
        throw GWException("failed to get CUPTI counter availability in the first pass: device_id(%d)", device_id);
    });
    if(unlikely(getCounterAvailabilityParams.counterAvailabilityImageSize == 0)){
        throw GWException("got 0 as counter availability image size: device_id(%d)", device_id);
    }
    this->_counterAvailabilityImage.resize(getCounterAvailabilityParams.counterAvailabilityImageSize);
    getCounterAvailabilityParams.pCounterAvailabilityImage = this->_counterAvailabilityImage.data();
    GW_IF_CUPTI_FAILED(cuptiProfilerGetCounterAvailability(&getCounterAvailabilityParams), sdk_retval, {
        throw GWException(
            "failed to get CUPTI counter availability in the second pass: device_id(%d), counterAvailabilityImageSize(%lu)",
            device_id,
            getCounterAvailabilityParams.counterAvailabilityImageSize
        );
    });
    GW_BACK_LINE;
    GW_DEBUG_C("obtained counter available image of the device: device_id(%d)", device_id);


    // step 4: get chip name
    getChipNameParams.deviceIndex = device_id;
    GW_IF_CUPTI_FAILED(cuptiDeviceGetChipName(&getChipNameParams), sdk_retval, {
        throw GWException("failed to get chip name: device_id(%d)", device_id);
    });
    this->_chip_name = strdup(getChipNameParams.pChipName);

exit:
    ;
}


GWProfileDevice_CUDA::~GWProfileDevice_CUDA(){}


gw_retval_t GWProfileDevice_CUDA::export_metric_properties(std::string metric_properties_cache_path){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    uint64_t i;
    std::string cache_file_path;
    GWProfileMetric_CUDA *gw_metric;
    std::vector<std::string> metrics_list;
    bool list_sub_metrics = true;

    GW_ASSERT(std::filesystem::exists(metric_properties_cache_path));
    cache_file_path = metric_properties_cache_path 
                    + std::string("/")
                    + this->_chip_name
                    + std::string("_metric_properties.yaml");

    if(std::filesystem::exists(cache_file_path)){
        GW_WARN_C(
            "found cached metric properties file, skip exporting...: path(%s), device_id(%d)",
            cache_file_path.c_str(), this->_device_id
        );
        // GW_DEBUG_C(
        //     "try initializing metadata of metrics on device from cached file...: device_id(%d), file_path(%s)",
        //     this->_device_id, cache_file_path.c_str()
        // );
        // GW_IF_FAILED(this->__load_metric_properties_from_yaml(cache_file_path), sdk_retval, {
        //     GW_WARN_C(
        //         "failed to load metric properties from cached yaml file: device_id(%d), file_path(%s)",
        //         this->_device_id, cache_file_path.c_str()
        //     );
        //     retval = (gw_retval_t)(sdk_retval);
        //     goto exit;
        // });
        // GW_BACK_LINE;
        // GW_DEBUG_C(
        //     "initialized metadata of metrics on device from cached file: device_id(%d), file_path(%s)",
        //     this->_device_id, cache_file_path.c_str()
        // );
    } else {
        GW_DEBUG_C(
            "initializing metadata of metrics on device from scratch...: device_id(%d)", this->_device_id
        );
        if(unlikely(
            !NV::Metric::Enum::ExportSupportedMetrics(
                this->_chip_name.c_str(),
                list_sub_metrics,
                this->_counterAvailabilityImage.data(),
                metrics_list
            )
        )){
            GW_WARN_C(
                "failed to export supported metrics for device: device_id(%d)", this->_device_id
            );
            retval = GW_FAILED;
            goto exit;
        }
        GW_DEBUG_C(
            "detected available metrics on device: device_id(%d), #metrics(%lu)", this->_device_id, metrics_list.size()
        );

        GW_DEBUG_C("serializing metrics...: %d/%lu", 0, metrics_list.size());
        for(i=0; i<metrics_list.size(); i++){
            GW_CHECK_POINTER(gw_metric = new GWProfileMetric_CUDA(metrics_list[i], this->_chip_name, this->_counterAvailabilityImage.data()));
            this->_gw_metrics.push_back(gw_metric);
            GW_BACK_LINE;
            GW_DEBUG_C("serializing metrics...: %lu/%lu", i, metrics_list.size());
        }

        GW_DEBUG_C("dumping to file...: path(%s)", cache_file_path.c_str());
        GW_IF_FAILED(this->__dump_metric_properties_to_yaml(cache_file_path), sdk_retval, {
            GW_WARN_C(
                "failed to dump metadata of supported metrics for device to file: device_id(%d), file_path(%s)",
                this->_device_id, cache_file_path.c_str()
            );
            retval = (gw_retval_t)(sdk_retval);
            goto exit;
        })

        GW_BACK_LINE;
        GW_DEBUG_C(
            "initialized metadata of metrics on device from scratch, and dump to cache file: device_id(%d), file_path(%s), #metrics(%lu)",
            this->_device_id, cache_file_path.c_str(), this->_gw_metrics.size()
        );
    }

exit:
    return retval;
}


gw_retval_t GWProfileDevice_CUDA::__load_metric_properties_from_yaml(std::string file_path){
    gw_retval_t retval = GW_SUCCESS;
    YAML::Node yaml_data, yaml_metric;
    uint64_t i, j;
    std::vector<std::string> raw_metric_names;
    GWProfileMetric_CUDA *gw_metric;

    GW_CHECK_POINTER(this->_counterAvailabilityImage.size() > 0);

    try {
        yaml_data = YAML::LoadFile(file_path);
        
        GW_ASSERT(yaml_data["metrics"]);
        for(i=0; i<yaml_data["metrics"].size(); i++){
            yaml_metric = yaml_data["metrics"][i];

            raw_metric_names.clear();
            GW_ASSERT(yaml_metric["raw_metric_names"]);
            for(j=0; j<yaml_data["raw_metric_names"].size(); j++){
                raw_metric_names.push_back(yaml_data["raw_metric_names"][i].as<std::string>());
            }

            GW_CHECK_POINTER(
                gw_metric = new GWProfileMetric_CUDA(
                    yaml_metric["name"].as<std::string>(),
                    this->_chip_name,
                    this->_counterAvailabilityImage.data(),
                    yaml_metric["num_isolated_passes"].as<uint64_t>(),
                    yaml_metric["num_pipelined_passes"].as<uint64_t>(),
                    yaml_metric["collection_method"].as<std::string>(),
                    raw_metric_names
                )
            );
            this->_gw_metrics.push_back(gw_metric);
        }
    } catch (const YAML::Exception& e) {
        GW_WARN_C("failed to parse yaml file: file_path(%s), error(%s)", file_path.c_str(), e.what());
        retval = GW_FAILED;
        goto exit;
    }

exit:
    return retval;
}


gw_retval_t GWProfileDevice_CUDA::__dump_metric_properties_to_yaml(std::string file_path){
    gw_retval_t retval = GW_SUCCESS;
    YAML::Node yaml_data, yaml_metric;
    uint64_t i, j;
    GWProfileMetric_CUDA *gw_metric;
    std::ofstream fout;

    yaml_data["chip_name"] = this->_chip_name;
    yaml_data["compute_capability_major"] = this->_compute_capability_major;
    yaml_data["compute_capability_minor"] = this->_compute_capability_minor;
    yaml_data["metrics"] = YAML::Node(YAML::NodeType::Sequence);

    for(i=0; i<this->_gw_metrics.size(); i++){
        GW_CHECK_POINTER(gw_metric = reinterpret_cast<GWProfileMetric_CUDA*>(this->_gw_metrics[i]));

        yaml_metric.reset();
        yaml_metric["name"] = gw_metric->get_metric_name();
        yaml_metric["num_isolated_passes"] = gw_metric->num_isolated_passes;
        yaml_metric["num_pipelined_passes"] = gw_metric->num_pipelined_passes;
        yaml_metric["collection_method"] = gw_metric->collection_method;
        yaml_metric["raw_metric_names"] = YAML::Node(YAML::NodeType::Sequence);
        for(j=0; j<gw_metric->raw_metric_names.size(); j++){
            yaml_metric["raw_metric_names"].push_back(gw_metric->raw_metric_names[j]);
        }

        yaml_data["metrics"].push_back(yaml_metric);
    }

    fout.open(file_path);
    if(likely(fout)){
        fout << yaml_data;
        fout.close();
    } else {
        GW_WARN_C("failed to open yaml file to cache metric metadatas: file_path(%s)", file_path.c_str());
        retval = GW_FAILED;
    }

    return retval;
}
