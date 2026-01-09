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
#include <cupti_profiler_host.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "profiler/profile_image.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/hash.hpp"


/*!
 *  \brief  image to store the counter data of the pc sampling result   
 */
class GWProfileImage_CUDA_PmSampling_CounterData final : public GWProfileImage {
 public:
    /*!
     *  \brief  constructor
     */
    GWProfileImage_CUDA_PmSampling_CounterData(int device_id);
    ~GWProfileImage_CUDA_PmSampling_CounterData();


    /*!
     *  \brief  fill this image using host API
     *  \param  metric_names        list of metric names
     *  \param  max_samples         max number of samples
     *  \param  cupti_pm_sampler    cupti pm sampling object
     *  \return GW_SUCCESS for successfully fill
     */
    gw_retval_t fill(std::vector<const char*> metric_names, uint32_t max_samples, CUpti_PmSampling_Object* cupti_pm_sampler);


 private:
    int _device_id = 0;
    uint32_t _retained_device_context_time = 0;
    CUcontext _cu_context = (CUcontext)(nullptr);
    CUdevice _cu_device = (CUdevice)(0);
};


/*!
 *  \brief  image to store the configuration of the pc sampling process   
 */
class GWProfileImage_CUDA_PmSampling_Config final : public GWProfileImage {
 public:
    /*!
     *  \brief  constructor
     */
    GWProfileImage_CUDA_PmSampling_Config();
    ~GWProfileImage_CUDA_PmSampling_Config();


    /*!
     *  \brief  fill this image using host API
     *  \param  host_profiler    host-side profiler
     *  \return GW_SUCCESS for successfully fill
     */
    gw_retval_t fill(CUpti_Profiler_Host_Object* host_profiler);


 private:
};


/*!
 *  \brief  image to store the configuration of the pc sampling process   
 */
class GWProfileImage_CUDA_PmSampling_CounterAvailability final : public GWProfileImage {
 public:
    /*!
     *  \brief  constructor
     */
    GWProfileImage_CUDA_PmSampling_CounterAvailability(int device_id);
    ~GWProfileImage_CUDA_PmSampling_CounterAvailability();


    /*!
     *  \brief  fill this image using host API
     *  \return GW_SUCCESS for successfully fill
     */
    gw_retval_t fill() override;


 private:
    int _device_id;
};
