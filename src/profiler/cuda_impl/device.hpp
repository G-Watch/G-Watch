#pragma once

#include <iostream>
#include <vector>
#include <string>

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cupti_target.h>
#include <cupti_profiler_target.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "profiler/device.hpp"
#include "common/utils/exception.hpp"
#include "profiler/cuda_impl/metric.hpp"


class GWProfiler_CUDA;


/*!
 *  \brief  represent an underlying CUDA device
 */
class GWProfileDevice_CUDA final : public GWProfileDevice {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     *  \param  device_id       index of the device
     */
    GWProfileDevice_CUDA(int device_id);
    GWProfileDevice_CUDA();


    /*!
     *  \brief  deconstructor
     */
    ~GWProfileDevice_CUDA();
    /* ==================== Common ==================== */

 
    /* ==================== CUDA ==================== */
 public:
    /*!
     *  \brief  identify whether this device is supported by CUPTI profiler APIs
     *  \return bool result
     */
    inline bool is_cupti_profiler_supported(){ return this->_is_cupti_profiler_supported; }


    /*!
     *  \brief  obtain the CUdevice instance
     *  \return CUdevice instance
     */
    inline CUdevice get_cu_device(){ return this->_cu_device; }


    /*!
     *  \brief  obtain the CUcontext instance
     *  \return CUcontext instance
     */
    inline CUcontext get_cu_context(){ return this->_cu_context; }


 private:
    // cuda device and context handle
    CUdevice _cu_device;
    CUcontext _cu_context;

    // compute capability
    int _compute_capability_major, _compute_capability_minor;

    // counter available image of the device
    std::vector<uint8_t> _counterAvailabilityImage;

    // identify whether this device is supported by CUPTI profiler APIs
    bool _is_cupti_profiler_supported;
    /* ==================== CUDA ==================== */


    /* ==================== Metrics ==================== */
 public:
    /*!
     *  \brief  export metric properties of current device
     *  \param  metric_properties_cache_path    path to output metric properties
     */
    gw_retval_t export_metric_properties(std::string metric_properties_cache_path="") override;
 

    /*!
     *  \brief  load metric properties of current device from cached yaml file
     *  \param  file_path   path to the cached yaml file
     *  \return GW_SUCCESS for successfully loading
     */
    gw_retval_t __load_metric_properties_from_yaml(std::string file_path);


    /*!
     *  \brief  dump metric properties of current device to yaml file
     *  \param  file_path   path to the dumped yaml file
     *  \return GW_SUCCESS for successfully dump
     */
    gw_retval_t __dump_metric_properties_to_yaml(std::string file_path);
    /* ==================== Metrics ==================== */
};
