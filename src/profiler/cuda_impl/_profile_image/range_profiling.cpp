#include <iostream>
#include <filesystem>
#include <fstream>
#include <string.h>

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cupti_target.h>
#include <cupti_profiler_target.h>
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


/* ==================================== Image: CounterAvailability ==================================== */
GWProfileImage_CUDA_RangeProfile_CounterAvailability::GWProfileImage_CUDA_RangeProfile_CounterAvailability(int device_id)
    :   GWProfileImage(),
        _device_id(device_id), 
        _retained_device_context_time(0),
        _cu_context((CUcontext)(nullptr)),
        _cu_device((CUdevice)(0))
    {}

GWProfileImage_CUDA_RangeProfile_CounterAvailability::~GWProfileImage_CUDA_RangeProfile_CounterAvailability(){
    uint8_t i;
    int sdk_retval;

    for(i=0; i<this->_retained_device_context_time; i++){
        GW_IF_CUDA_DRIVER_FAILED(cuDevicePrimaryCtxRelease(this->_cu_device), sdk_retval, {
            GW_WARN_C("failed to release the primary context of device: device_id(%d)", this->_device_id);
        });
    }
}

gw_retval_t GWProfileImage_CUDA_RangeProfile_CounterAvailability::fill(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_GetCounterAvailability_Params getCounterAvailabilityParams 
        = { CUpti_Profiler_GetCounterAvailability_Params_STRUCT_SIZE };

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

    // for first pass of cuptiProfilerGetCounterAvailability, it returns size needed for data
    getCounterAvailabilityParams.ctx = this->_cu_context;
    GW_IF_CUPTI_FAILED(cuptiProfilerGetCounterAvailability(&getCounterAvailabilityParams), sdk_retval, {
        GW_WARN_C(
            "failed to get CUPTI counter availability in the first pass: device_id(%d)", this->_device_id
        );
        retval = GW_FAILED;
        goto exit;
    });

    // for second pass of cuptiProfilerGetCounterAvailability, it returns counterAvailabilityImage
    this->_image.clear();
    this->_image.resize(getCounterAvailabilityParams.counterAvailabilityImageSize);
    getCounterAvailabilityParams.pCounterAvailabilityImage = this->_image.data();
    GW_IF_CUPTI_FAILED(cuptiProfilerGetCounterAvailability(&getCounterAvailabilityParams), sdk_retval, {
        GW_WARN_C(
            "failed to get CUPTI counter availability in the second pass: device_id(%d)", this->_device_id
        );
        retval = GW_FAILED;
        goto exit;
    });

    GW_DEBUG_C(
        "fill counter availability image: device_id(%d), size(%lu)",
        this->_device_id,
        this->_image.size()
    );

exit:
    return retval;
}
/* ==================================== Image: CounterAvailability ==================================== */




/* ==================================== Image: Configuration ==================================== */
GWProfileImage_CUDA_RangeProfile_Configuration::GWProfileImage_CUDA_RangeProfile_Configuration(
    const char* chip_name,
    GWProfileImage_CUDA_RangeProfile_CounterAvailability* image_counter_availability,
    std::vector<std::string>& metric_names
) : GWProfileImage(),
    _image_counter_availability(image_counter_availability),
    _metric_names(metric_names)
{
    GW_CHECK_POINTER(image_counter_availability);
    GW_ASSERT(metric_names.size() > 0);
    this->_chip_name = strdup(chip_name);
}

GWProfileImage_CUDA_RangeProfile_Configuration::~GWProfileImage_CUDA_RangeProfile_Configuration(){}

gw_retval_t GWProfileImage_CUDA_RangeProfile_Configuration::fill(){
    gw_retval_t retval = GW_SUCCESS;

    GW_CHECK_POINTER(this->_image_counter_availability);

    this->_image.clear();
    if(unlikely(
        !NV::Metric::Config::GetConfigImage(
            this->_chip_name, this->_metric_names, this->_image, this->_image_counter_availability->data()
        )
    )){
        GW_WARN_C("failed to create configImage");
        retval = GW_FAILED;
        goto exit;
    }

    GW_DEBUG_C(
        "fill configuration image: size(%lu), chip_name(%s), metric_names(%s)",
        this->_image.size(),
        this->_chip_name,
        GWUtilString::merge(this->_metric_names, ',').c_str()
    );

exit:
    return retval;
}
/* ==================================== Image: Configuration ==================================== */




/* ==================================== Image: CounterDataPrefix ==================================== */
GWProfileImage_CUDA_RangeProfile_CounterDataPrefix::GWProfileImage_CUDA_RangeProfile_CounterDataPrefix(
    const char* chip_name,
    GWProfileImage_CUDA_RangeProfile_CounterAvailability* image_counter_availability,
    std::vector<std::string>& metric_names
) : GWProfileImage(),
    _image_counter_availability(image_counter_availability),
    _metric_names(metric_names)
{
    GW_CHECK_POINTER(image_counter_availability);
    GW_ASSERT(metric_names.size() > 0);
    this->_chip_name = strdup(chip_name);
}


GWProfileImage_CUDA_RangeProfile_CounterDataPrefix::~GWProfileImage_CUDA_RangeProfile_CounterDataPrefix(){}


gw_retval_t GWProfileImage_CUDA_RangeProfile_CounterDataPrefix::fill(){
    gw_retval_t retval = GW_SUCCESS;

    GW_CHECK_POINTER(this->_image_counter_availability);

    this->_image.clear();
    if(unlikely(
        !NV::Metric::Config::GetCounterDataPrefixImage(
            this->_chip_name, this->_metric_names, this->_image, this->_image_counter_availability->data()
        )
    )){
        GW_WARN_C("failed to create counterDataPrefixImage");
        retval = GW_FAILED;
        goto exit;
    }

    GW_DEBUG_C(
        "fill counter data prefix image: size(%lu), chip_name(%s), metric_names(%s)",
        this->_image.size(),
        this->_chip_name,
        GWUtilString::merge(this->_metric_names, ',').c_str()
    );

exit:
    return retval;
}
/* ==================================== Image: CounterDataPrefix ==================================== */




/* ==================================== Image: CounterData ==================================== */
GWProfileImage_CUDA_RangeProfile_CounterData::GWProfileImage_CUDA_RangeProfile_CounterData(
    GWProfileImage_CUDA_RangeProfile_CounterDataPrefix* image_counter_data_prefix,
    uint32_t max_num_ranges,
    uint32_t max_num_range_tree_nodes,
    uint32_t max_num_range_name_length
) : GWProfileImage(),
    _image_counter_data_prefix(image_counter_data_prefix),
    _max_num_ranges(max_num_ranges),
    _max_num_range_tree_nodes(max_num_range_tree_nodes),
    _max_num_range_name_length(max_num_range_name_length)
{
    GW_CHECK_POINTER(image_counter_data_prefix);
}


GWProfileImage_CUDA_RangeProfile_CounterData::~GWProfileImage_CUDA_RangeProfile_CounterData(){}


gw_retval_t GWProfileImage_CUDA_RangeProfile_CounterData::fill(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_CounterDataImageOptions counter_data_image_options;
    CUpti_Profiler_CounterDataImage_CalculateSize_Params calculate_size_params 
        = { CUpti_Profiler_CounterDataImage_CalculateSize_Params_STRUCT_SIZE };
    CUpti_Profiler_CounterDataImage_Initialize_Params initialize_params 
        = { CUpti_Profiler_CounterDataImage_Initialize_Params_STRUCT_SIZE };

    GW_CHECK_POINTER(this->_image_counter_data_prefix);

    // calculate size of counterDataImage based on counterDataPrefixImage and options
    counter_data_image_options.pCounterDataPrefix = this->_image_counter_data_prefix->data();
    counter_data_image_options.counterDataPrefixSize = this->_image_counter_data_prefix->size();
    counter_data_image_options.maxNumRanges = this->_max_num_ranges;
    counter_data_image_options.maxNumRangeTreeNodes = this->_max_num_range_tree_nodes;
    counter_data_image_options.maxRangeNameLength = this->_max_num_range_name_length;
    calculate_size_params.pOptions = &counter_data_image_options;
    calculate_size_params.sizeofCounterDataImageOptions 
        = CUpti_Profiler_CounterDataImageOptions_STRUCT_SIZE;
    GW_IF_CUPTI_FAILED(
        cuptiProfilerCounterDataImageCalculateSize(&calculate_size_params),
        sdk_retval, {
            GW_WARN_C("failed to calculate the size of counter data image");
            retval = GW_FAILED;
            goto exit;
        }
    );

    // resize the image
    this->_image.clear();
    this->_image.resize(calculate_size_params.counterDataImageSize);
    
    // initialize counterDataImage
    initialize_params.pOptions = &counter_data_image_options;
    initialize_params.sizeofCounterDataImageOptions = CUpti_Profiler_CounterDataImageOptions_STRUCT_SIZE;
    initialize_params.counterDataImageSize = this->_image.size();
    initialize_params.pCounterDataImage = this->_image.data();
    GW_IF_CUPTI_FAILED(
        cuptiProfilerCounterDataImageInitialize(&initialize_params),
        sdk_retval, {
            GW_WARN_C("failed to initialize the counter data image");
            retval = GW_FAILED;
            goto exit;
        }
    );

    GW_DEBUG_C(
        "fill counter data image: "
        "size(%lu), "
        "max_num_ranges(%u), "
        "max_num_range_tree_nodes(%u), "
        "max_num_range_name_length(%u)"
        ,
        this->_image.size(),
        this->_max_num_ranges,
        this->_max_num_range_tree_nodes,
        this->_max_num_range_name_length
    );

exit:
    return retval;
}
/* ==================================== Image: CounterData ==================================== */




/* ==================================== Image: ScratchBuffer ==================================== */
GWProfileImage_CUDA_RangeProfile_ScratchBuffer::GWProfileImage_CUDA_RangeProfile_ScratchBuffer(
    GWProfileImage_CUDA_RangeProfile_CounterData* image_counter_data
) : GWProfileImage(),
    _image_counter_data(image_counter_data)
{
    GW_CHECK_POINTER(image_counter_data);
}

GWProfileImage_CUDA_RangeProfile_ScratchBuffer::~GWProfileImage_CUDA_RangeProfile_ScratchBuffer(){}

gw_retval_t GWProfileImage_CUDA_RangeProfile_ScratchBuffer::fill(){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    CUpti_Profiler_CounterDataImage_CalculateScratchBufferSize_Params scratch_buffer_size_params 
        = { CUpti_Profiler_CounterDataImage_CalculateScratchBufferSize_Params_STRUCT_SIZE };
    CUpti_Profiler_CounterDataImage_InitializeScratchBuffer_Params init_scratch_buffer_params 
        = { CUpti_Profiler_CounterDataImage_InitializeScratchBuffer_Params_STRUCT_SIZE };

    GW_CHECK_POINTER(this->_image_counter_data);

    scratch_buffer_size_params.counterDataImageSize = this->_image_counter_data->size();
    scratch_buffer_size_params.pCounterDataImage = this->_image_counter_data->data();
    GW_IF_CUPTI_FAILED(
        cuptiProfilerCounterDataImageCalculateScratchBufferSize(&scratch_buffer_size_params),
        sdk_retval, {
            GW_WARN_C("failed to calculate the size of scratch buffer");
            retval = GW_FAILED;
            goto exit;
        }
    );

    // resize the image
    this->_image.clear();
    this->_image.resize(scratch_buffer_size_params.counterDataScratchBufferSize);

    // initialize counterDataScratchBuffer
    init_scratch_buffer_params.counterDataImageSize = this->_image_counter_data->size();
    init_scratch_buffer_params.pCounterDataImage = this->_image_counter_data->data();
    init_scratch_buffer_params.counterDataScratchBufferSize = this->_image.size();
    init_scratch_buffer_params.pCounterDataScratchBuffer = this->_image.data();
    GW_IF_CUPTI_FAILED(
        cuptiProfilerCounterDataImageInitializeScratchBuffer(&init_scratch_buffer_params),
        sdk_retval, {
            GW_WARN_C("failed to initialize the scratch buffer");
            retval = GW_FAILED;
            goto exit;
        }
    );

    GW_DEBUG_C("fill scratch image: size(%lu)", this->_image.size());

exit:
    return retval;
}
/* ==================================== Image: ScratchBuffer ==================================== */
