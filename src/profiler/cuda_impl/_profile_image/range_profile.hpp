#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <mutex>

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cupti_target.h>
#include <cupti_profiler_target.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "profiler/profile_image.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/hash.hpp"


/*!
 *  \brief  image to store the counter availability of the device   
 */
class GWProfileImage_CUDA_RangeProfile_CounterAvailability final : public GWProfileImage {
 public:
    /*!
     *  \brief  constructor
     *  \param  device_id   index of the device of current image
     */
    GWProfileImage_CUDA_RangeProfile_CounterAvailability(int device_id);
    ~GWProfileImage_CUDA_RangeProfile_CounterAvailability();


    /*!
     *  \brief  fill this image using host API
     *  \return GW_SUCCESS for successfully fill
     */
    gw_retval_t fill() override;
 

 private:
    // the device that has current counter availability image
    int _device_id;

    // identify the times that retain the device context
    uint8_t _retained_device_context_time;

    CUcontext _cu_context;
    CUdevice _cu_device;
};


/*!
 *  \brief  a blob to configure the session for counters to be collected
 *  \ref    https://docs.nvidia.com/cupti/main/main.html#cupti-profiler-definitions
 */
class GWProfileImage_CUDA_RangeProfile_Configuration final : public GWProfileImage {
 public:
    /*!
     *  \brief  constructor
     *  \param  chip_name                   name of the chip from which the session to create
     *  \param  image_counter_availability  pointer to the counter availability corresponding to the device
     *  \param  metric_names                names of metrics to be collected in this session
     */
    GWProfileImage_CUDA_RangeProfile_Configuration(
        const char* chip_name,
        GWProfileImage_CUDA_RangeProfile_CounterAvailability* image_counter_availability,
        std::vector<std::string>& metric_names
    );
    ~GWProfileImage_CUDA_RangeProfile_Configuration();


    /*!
     *  \brief  fill this image using host API
     *  \return GW_SUCCESS for successfully fill
     */
    gw_retval_t fill() override;
 

 private:
    // name of the chip from which the session to create
    const char *_chip_name;

    // pointer to the counter availability corresponding to the device
    GWProfileImage_CUDA_RangeProfile_CounterAvailability *_image_counter_availability;

    // names of metrics to be collected in this session
    std::vector<std::string> _metric_names;
};


/*!
 *  \brief  a metadata header for CounterData Image
 *  \ref    https://docs.nvidia.com/cupti/main/main.html#cupti-profiler-definitions
 */
class GWProfileImage_CUDA_RangeProfile_CounterDataPrefix final : public GWProfileImage {
 public:
    /*!
     *  \brief  constructor
     *  \param  chip_name                   name of the chip from which the session to create
     *  \param  image_counter_availability  pointer to the counter availability corresponding to the device
     *  \param  metric_names                names of metrics to be collected in this session
     */
    GWProfileImage_CUDA_RangeProfile_CounterDataPrefix(
        const char* chip_name,
        GWProfileImage_CUDA_RangeProfile_CounterAvailability* image_counter_availability,
        std::vector<std::string>& metric_names
    );
    ~GWProfileImage_CUDA_RangeProfile_CounterDataPrefix();


    /*!
     *  \brief  fill this image using host API
     *  \return GW_SUCCESS for successfully fill
     */
    gw_retval_t fill() override;
 

 private:
    // name of the chip from which the session to create
    const char *_chip_name;

    // pointer to the counter availability corresponding to the device
    GWProfileImage_CUDA_RangeProfile_CounterAvailability *_image_counter_availability;

    // names of metrics to be collected in this session
    std::vector<std::string> _metric_names;
};


/*!
 *  \brief  a blob which contains the values of collected counters
 *  \ref    https://docs.nvidia.com/cupti/main/main.html#cupti-profiler-definitions
 */
class GWProfileImage_CUDA_RangeProfile_CounterData final : public GWProfileImage {
 public:
    /*!
     *  \brief  constructor
     *  \param  image_counter_data_prefix   pointer to the counter data prefix image
     *  \param  metric_names                names of metrics to be collected in this session
     */
    GWProfileImage_CUDA_RangeProfile_CounterData(
        GWProfileImage_CUDA_RangeProfile_CounterDataPrefix* image_counter_data_prefix,
        uint32_t max_num_ranges,
        uint32_t max_num_range_tree_nodes,
        uint32_t max_num_range_name_length
    );
    ~GWProfileImage_CUDA_RangeProfile_CounterData();


    /*!
     *  \brief  fill this image using host API
     *  \return GW_SUCCESS for successfully fill
     */
    gw_retval_t fill() override;
 

 private:
    // pointer to the counter data prefix image
    GWProfileImage_CUDA_RangeProfile_CounterDataPrefix* _image_counter_data_prefix;

    // range constrains
    uint32_t _max_num_ranges;
    uint32_t _max_num_range_tree_nodes;
    uint32_t _max_num_range_name_length;
};


/*!
 *  \brief  TODO
 *  \ref    TODO
 */
class GWProfileImage_CUDA_RangeProfile_ScratchBuffer final : public GWProfileImage {
 public:
    /*!
     *  \brief  constructor
     *  \param  image_counter_data_prefix   pointer to the counter data image
     */
    GWProfileImage_CUDA_RangeProfile_ScratchBuffer(GWProfileImage_CUDA_RangeProfile_CounterData* image_counter_data);
    ~GWProfileImage_CUDA_RangeProfile_ScratchBuffer();


    /*!
     *  \brief  fill this image using host API
     *  \return GW_SUCCESS for successfully fill
     */
    gw_retval_t fill() override;
 

 private:
    // pointer to the counter data image
    GWProfileImage_CUDA_RangeProfile_CounterData* _image_counter_data;
};
