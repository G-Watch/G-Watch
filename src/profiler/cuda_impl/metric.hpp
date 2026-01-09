#pragma once

#include <iostream>
#include <vector>
#include <string>

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cupti_target.h>
#include <cupti_profiler_target.h>
#include <nvperf_host.h>
#include <nvperf_cuda_host.h>


#include "common/common.hpp"
#include "common/log.hpp"
#include "profiler/metric.hpp"
#include "common/utils/exception.hpp"


class GWProfileMetric_CUDA final : public GWProfileMetric {
 public:
    /*!
     *  \brief  constructor
     *  \param  metric_name                 name of the metric
     *  \param  chip_name                   name of the chip
     *  \param  pCounterAvailabilityImage   pointer to the counter availability image
     */
    GWProfileMetric_CUDA(std::string metric_name, std::string chip_name, const uint8_t *pCounterAvailabilityImage_);


    /*!
     *  \brief  constructor
     *  \note   this constructor is for fast resume from file
     *  \param  metric_name                 name of the metric
     *  \param  chip_name                   name of the chip
     *  \param  pCounterAvailabilityImage   pointer to the counter availability image
     *  \param  num_isolated_passes         number of isolated passes of the current metric
     *  \param  num_pipelined_passes        number of pipelined passes of the current metric
     *  \param  collection_method           collection method of the current metric
     *  \param  raw_metric_names            all raw metrics behind current metric
     */
    GWProfileMetric_CUDA(
        std::string metric_name,
        std::string chip_name,
        const uint8_t *pCounterAvailabilityImage,
        uint64_t num_isolated_passes,
        uint64_t num_pipelined_passes,
        std::string collection_method,
        std::vector<std::string> raw_metric_names
    );


    /*!
     *  \brief  deconstructor 
     */
    ~GWProfileMetric_CUDA() = default;


    /*!
     *  \brief  obtain number of passes to collect this metric
     *  \param  num_nesting_levels  number of nesting level
     *  \return number of passes to collect this metric
     */
    inline uint64_t get_nb_passes(uint64_t num_nesting_levels);

    // number of isolated passes of the current metric
    uint64_t num_isolated_passes;

    // number of pipelined passes of the current metric
    uint64_t num_pipelined_passes;

    // number of total passes of the current metric
    uint64_t num_total_passes;

    // collection method of the current metric
    std::string collection_method;

    // all raw metrics behind current metric
    std::vector<std::string> raw_metric_names;

 private:
    // all raw metric requests behind current metric
    std::vector<NVPA_RawMetricRequest> _raw_metric_requests;


    /*!
     *  \brief  obtain the raw metric name based on given metric name
     *  \param  chip_name                   name of the chip
     *  \param  metric_name                 name of the metric
     *  \param  pCounterAvailabilityImage   pointer to the counter availability image
     *  \param  raw_metric_names            name of the raw metrics to be colloected
     *  \param  raw_metric_requests         obtained raw metric requests
     *  \return GW_SUCCESS for successfully get
     */
    static gw_retval_t __get_raw_metric_requests(
        std::string chip_name,
        std::string metric_name,
        const uint8_t* pCounterAvailabilityImage,
        std::vector<std::string>& raw_metric_names,
        std::vector<NVPA_RawMetricRequest>& raw_metric_requests
    );


    /*!
     *  \brief  obtain method to get speficified metric (HW/SW)
     *  \param  metric_name name of the metric
     *  \param  method      obtained method
     *  \return GW_SUCCESS for succesfully obtained
     */
    static gw_retval_t __get_metric_collection_method(
        std::string metric_name,
        std::string &method
    );
};
