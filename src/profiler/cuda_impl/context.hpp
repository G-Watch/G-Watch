#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>

#include <cuda.h>
#include <nvml.h>
#include <cuda_runtime_api.h>
#include <cupti_target.h>
#include <cupti_profiler_target.h>

#include "common/common.hpp"
#include "profiler/context.hpp"
#include "profiler/profile_image.hpp"
#include "common/utils/timer.hpp"


class GWProfileContext_CUDA final : public GWProfileContext {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  (de)constructor
     *  \param  lazy_init_device    whether to initialize the device when constructing the context
     */
    GWProfileContext_CUDA(bool lazy_init_device = true);
    ~GWProfileContext_CUDA();


 protected:
    /*!
     *  \brief  init speficied device under this profiler context
     *  \param  device_id       index of the device to be profiled
     *  \return GW_SUCCESS for successfully initialized
     */
    gw_retval_t __init_gw_device(int device_id) override;
    /* ==================== Common ==================== */


    /* ==================== NVPW ==================== */
 private:
    /*!
     *  \brief  initialize the NVPW
     *  \return GW_SUCCESS for successfully initialized
     */
    gw_retval_t __init_nvpw();
    /* ==================== NVPW ==================== */


    /* ==================== NVML ==================== */
 private:
    /*!
     *  \brief  initialize the NVML
     *  \return GW_SUCCESS for successfully initialized
     */
    gw_retval_t __init_nvml();
    /* ==================== NVML ==================== */


    /* ==================== clock management ==================== */
 public:
    // TODO(zhuobin): should be move this function to profiler?
    gw_retval_t get_clock_freq(int device_id, std::map<std::string, uint32_t>& clock_map);
    /* ==================== clock management ==================== */


    /* ==================== power management ==================== */
 public:
    
    /* ==================== power management ==================== */


    /* ==================== Profiling ==================== */
 public:
    /*!
     *  \brief  create a new profiler for profiling specified metrics on specified device
     *  \note   this API would create all necessary images for profiling
     *  \param  device_id       index of the device to be profiled
     *  \param  metric_names    vector of metrics to be profiled
     *  \param  profiler_mode   mode of the profiler
     *  \param  gw_profiler    the created profiler
     */
    gw_retval_t create_profiler(
        int device_id,
        const std::vector<std::string>& metric_names,
        gw_profiler_mode_t profiler_mode,
        GWProfiler** gw_profiler
    ) override;


    /*!
     *  \brief  destory profiler created by create_profiler
     *  \param  gw_profiler    the profiler to be destoryed
     */
    void destroy_profiler(GWProfiler* gw_profiler) override;


 private:
    /*!
     *  \brief  initialize the CUPTI profiler APIs
     *  \return GW_SUCCESS for successfully initialized
     */
    gw_retval_t __init_profiler_apis();
    /* ==================== Profiling ==================== */
};
