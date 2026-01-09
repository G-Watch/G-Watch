#include <iostream>
#include <vector>
#include <string>

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cupti_target.h>
#include <cupti_profiler_target.h>
#include <nvperf_host.h>
#include <nvperf_cuda_host.h>

#include "common/cuda_impl/cupti/extensions/include/profilerhost_util/Parser.h"

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/timer.hpp"

#include "profiler/cuda_impl/metric.hpp"


GWProfileMetric_CUDA::GWProfileMetric_CUDA(std::string metric_name, std::string chip_name, const uint8_t *pCounterAvailabilityImage)
    :   GWProfileMetric(metric_name, chip_name),
        num_isolated_passes(0),
        num_pipelined_passes(0),
        num_total_passes(1),
        collection_method("HW")
{
    int sdk_retval;

    NVPW_CUDA_RawMetricsConfig_Create_V2_Params rawMetricsConfigCreateParams 
        = { NVPW_CUDA_RawMetricsConfig_Create_V2_Params_STRUCT_SIZE };
    NVPA_RawMetricsConfig *pRawMetricsConfig;
    NVPW_RawMetricsConfig_BeginPassGroup_Params beginPassGroupParams 
        = { NVPW_RawMetricsConfig_BeginPassGroup_Params_STRUCT_SIZE };
    NVPW_RawMetricsConfig_IsAddMetricsPossible_Params isAddMetricsPossibleParams 
        = { NVPW_RawMetricsConfig_IsAddMetricsPossible_Params_STRUCT_SIZE };
    NVPW_RawMetricsConfig_AddMetrics_Params addMetricsParams 
        = { NVPW_RawMetricsConfig_AddMetrics_Params_STRUCT_SIZE };
    NVPW_RawMetricsConfig_EndPassGroup_Params endPassGroupParams 
        = { NVPW_RawMetricsConfig_EndPassGroup_Params_STRUCT_SIZE };
    NVPW_RawMetricsConfig_GetNumPasses_Params rawMetricsConfigGetNumPassesParams
        = { NVPW_RawMetricsConfig_GetNumPasses_Params_STRUCT_SIZE };
    NVPW_RawMetricsConfig_Destroy_Params rawMetricsConfigDestroyParams = { 
        NVPW_RawMetricsConfig_Destroy_Params_STRUCT_SIZE 
    };

    //! \note   we bellow try to add the raw metrics of the metric to obtain the pass metadata of the metric

    // step 1: obtain all raw metrics behind the given metric
    GW_IF_FAILED(
        GWProfileMetric_CUDA::__get_raw_metric_requests(
            this->_metric_name,
            this->_chip_name,
            pCounterAvailabilityImage,
            this->raw_metric_names,
            this->_raw_metric_requests
        ),
        sdk_retval, {
            throw GWException(
                "failed to get raw metric requests: _metric_name(%s), _chip_name(%s)",
                _metric_name.c_str(), _chip_name.c_str()
            );
        }
    );


    // step 2: create raw metric config
    rawMetricsConfigCreateParams.activityKind = NVPA_ACTIVITY_KIND_PROFILER;
    rawMetricsConfigCreateParams.pChipName = this->_chip_name.c_str();
    rawMetricsConfigCreateParams.pCounterAvailabilityImage = pCounterAvailabilityImage;
    GW_IF_NVPW_FAILED(NVPW_CUDA_RawMetricsConfig_Create_V2(&rawMetricsConfigCreateParams), sdk_retval, {
        throw GWException(
            "failed to create raw metric config: _metric_name(%s), _chip_name(%s)",
            _metric_name.c_str(), _chip_name.c_str()
        );
    });

    // step 3: begin pass group
    pRawMetricsConfig = rawMetricsConfigCreateParams.pRawMetricsConfig;
    beginPassGroupParams.pRawMetricsConfig = pRawMetricsConfig;
    GW_IF_NVPW_FAILED(NVPW_RawMetricsConfig_BeginPassGroup(&beginPassGroupParams), sdk_retval, {
        throw GWException(
            "failed to begin pass group: _metric_name(%s), _chip_name(%s)",
            _metric_name.c_str(), _chip_name.c_str()
        );
    });

    // step 4: query whether it's possible to add raw metrics
    isAddMetricsPossibleParams.pRawMetricsConfig = pRawMetricsConfig;
    isAddMetricsPossibleParams.pRawMetricRequests = this->_raw_metric_requests.data();
    isAddMetricsPossibleParams.numMetricRequests = this->_raw_metric_requests.size();
    GW_IF_NVPW_FAILED(NVPW_RawMetricsConfig_IsAddMetricsPossible(&isAddMetricsPossibleParams), sdk_retval, {
        throw GWException(
            "failed to add raw metrics, not possible: _metric_name(%s), _chip_name(%s)",
            _metric_name.c_str(), _chip_name.c_str()
        );
    });

    // step 5: add raw metrics
    addMetricsParams.pRawMetricsConfig = pRawMetricsConfig;
    addMetricsParams.pRawMetricRequests = this->_raw_metric_requests.data();
    addMetricsParams.numMetricRequests = this->_raw_metric_requests.size();
    GW_IF_NVPW_FAILED(NVPW_RawMetricsConfig_AddMetrics(&addMetricsParams), sdk_retval, {
        throw GWException(
            "failed to add raw metrics: _metric_name(%s), _chip_name(%s)",
            _metric_name.c_str(), _chip_name.c_str()
        );
    });

    // step 6: end pass group
    endPassGroupParams.pRawMetricsConfig = pRawMetricsConfig;
    GW_IF_NVPW_FAILED(NVPW_RawMetricsConfig_EndPassGroup(&endPassGroupParams), sdk_retval, {
        throw GWException(
            "failed to end pass group: _metric_name(%s), _chip_name(%s)",
            _metric_name.c_str(), _chip_name.c_str()
        );
    });

    // step 7: obtain number of passes
    rawMetricsConfigGetNumPassesParams.pRawMetricsConfig = pRawMetricsConfig;
    GW_IF_NVPW_FAILED(NVPW_RawMetricsConfig_GetNumPasses(&rawMetricsConfigGetNumPassesParams), sdk_retval, {
        throw GWException(
            "failed to end pass group: _metric_name(%s), _chip_name(%s)",
            _metric_name.c_str(), _chip_name.c_str()
        );
    });

    this->num_isolated_passes = rawMetricsConfigGetNumPassesParams.numIsolatedPasses;
    this->num_pipelined_passes = rawMetricsConfigGetNumPassesParams.numPipelinedPasses;

    // obtain the collection method
    GW_IF_FAILED(GWProfileMetric_CUDA::__get_metric_collection_method(this->_metric_name, this->collection_method), sdk_retval, {
        throw GWException(
            "failed to obtain collection method: _metric_name(%s), _chip_name(%s)",
            _metric_name.c_str(), _chip_name.c_str()
        );
    });

    // destory raw metric config
    rawMetricsConfigDestroyParams.pRawMetricsConfig = pRawMetricsConfig;
    GW_IF_NVPW_FAILED(NVPW_RawMetricsConfig_Destroy(&rawMetricsConfigDestroyParams), sdk_retval, {
        throw GWException(
                "failed to destory raw metric config: _metric_name(%s), _chip_name(%s)",
                _metric_name.c_str(), _chip_name.c_str()
            );
    });
}


GWProfileMetric_CUDA::GWProfileMetric_CUDA(
    std::string metric_name,
    std::string chip_name,
    const uint8_t *pCounterAvailabilityImage,
    uint64_t num_isolated_passes,
    uint64_t num_pipelined_passes,
    std::string collection_method,
    std::vector<std::string> raw_metric_names
) : GWProfileMetric(metric_name, chip_name),
    num_isolated_passes(num_isolated_passes),
    num_pipelined_passes(num_pipelined_passes),
    collection_method(collection_method),
    raw_metric_names(raw_metric_names)
{
    int sdk_retval;

    // resume raw metric resuest based on their names
    GW_IF_FAILED(
        GWProfileMetric_CUDA::__get_raw_metric_requests(
            this->_metric_name,
            this->_chip_name,
            pCounterAvailabilityImage,
            this->raw_metric_names,
            this->_raw_metric_requests
        ),
        sdk_retval, {
            throw GWException(
                "failed to get raw metric requests: _metric_name(%s), _chip_name(%s)",
                _metric_name.c_str(), _chip_name.c_str()
            );
        }
    );
}


uint64_t GWProfileMetric_CUDA::get_nb_passes(uint64_t num_nesting_levels){
    return num_nesting_levels * this->num_isolated_passes + this->num_pipelined_passes;
}


gw_retval_t GWProfileMetric_CUDA::__get_metric_collection_method(
    std::string _metric_name,
    std::string &method
){
    gw_retval_t retval = GW_SUCCESS;
    const std::string SW_CHECK = "sass";

    if (_metric_name.find(SW_CHECK) != std::string::npos){
        method = "SW";
    } else {
        method = "HW";
    }

    return retval;
}


gw_retval_t GWProfileMetric_CUDA::__get_raw_metric_requests(
    std::string _metric_name,
    std::string _chip_name,
    const uint8_t* pCounterAvailabilityImage,
    std::vector<std::string>& raw_metric_names,
    std::vector<NVPA_RawMetricRequest>& raw_metric_requests
){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    uint64_t i;
    std::string req_name;
    bool isolated = true;
    bool keep_instances = true;
    std::vector<uint8_t> scratchBuffer;
    std::vector<const char*> rawDependencies;
  
    NVPW_CUDA_MetricsEvaluator_CalculateScratchBufferSize_Params calculateScratchBufferSizeParam
        = {NVPW_CUDA_MetricsEvaluator_CalculateScratchBufferSize_Params_STRUCT_SIZE};
    NVPW_CUDA_MetricsEvaluator_Initialize_Params metricEvaluatorInitializeParams 
        = {NVPW_CUDA_MetricsEvaluator_Initialize_Params_STRUCT_SIZE};
    NVPW_MetricEvalRequest metricEvalRequest;
    NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params convertMetricToEvalRequest
        = {NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params_STRUCT_SIZE};
    NVPW_MetricsEvaluator* metricEvaluator = nullptr;
    NVPW_MetricsEvaluator_GetMetricRawDependencies_Params getMetricRawDependenciesParms 
        = {NVPW_MetricsEvaluator_GetMetricRawDependencies_Params_STRUCT_SIZE};
    NVPW_MetricsEvaluator_Destroy_Params metricEvaluatorDestroyParams 
        = { NVPW_MetricsEvaluator_Destroy_Params_STRUCT_SIZE };

    GW_CHECK_POINTER(pCounterAvailabilityImage);

    // initialize scratch buffer
    calculateScratchBufferSizeParam.pChipName = _chip_name.c_str();
    calculateScratchBufferSizeParam.pCounterAvailabilityImage = pCounterAvailabilityImage;
    GW_IF_NVPW_FAILED(NVPW_CUDA_MetricsEvaluator_CalculateScratchBufferSize(&calculateScratchBufferSizeParam), sdk_retval, {
        retval = GW_FAILED;
        goto exit;
    });
    scratchBuffer.resize(calculateScratchBufferSizeParam.scratchBufferSize);

    // initialize metric evaluator
    metricEvaluatorInitializeParams.scratchBufferSize = scratchBuffer.size();
    metricEvaluatorInitializeParams.pScratchBuffer = scratchBuffer.data();
    metricEvaluatorInitializeParams.pChipName = _chip_name.c_str();
    metricEvaluatorInitializeParams.pCounterAvailabilityImage = pCounterAvailabilityImage;
    GW_IF_NVPW_FAILED(NVPW_CUDA_MetricsEvaluator_Initialize(&metricEvaluatorInitializeParams), sdk_retval, {
        retval = GW_FAILED;
        goto exit;
    });
    metricEvaluator = metricEvaluatorInitializeParams.pMetricsEvaluator;

    NV::Metric::Parser::ParseMetricNameString(_metric_name, &req_name, &isolated, &keep_instances);
    keep_instances = true;

    convertMetricToEvalRequest.pMetricsEvaluator = metricEvaluator;
    convertMetricToEvalRequest.pMetricName = req_name.c_str();
    convertMetricToEvalRequest.pMetricEvalRequest = &metricEvalRequest;
    convertMetricToEvalRequest.metricEvalRequestStructSize = NVPW_MetricEvalRequest_STRUCT_SIZE;
    GW_IF_NVPW_FAILED(NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest(&convertMetricToEvalRequest), sdk_retval, {
        retval = GW_FAILED;
        goto exit;
    });

    getMetricRawDependenciesParms.pMetricsEvaluator = metricEvaluator;
    getMetricRawDependenciesParms.pMetricEvalRequests = &metricEvalRequest;
    getMetricRawDependenciesParms.numMetricEvalRequests = 1;
    getMetricRawDependenciesParms.metricEvalRequestStructSize = NVPW_MetricEvalRequest_STRUCT_SIZE;
    getMetricRawDependenciesParms.metricEvalRequestStrideSize = sizeof(NVPW_MetricEvalRequest);
    GW_IF_NVPW_FAILED(NVPW_MetricsEvaluator_GetMetricRawDependencies(&getMetricRawDependenciesParms), sdk_retval, {
        retval = GW_FAILED;
        goto exit;
    });
    rawDependencies.resize(getMetricRawDependenciesParms.numRawDependencies);
    getMetricRawDependenciesParms.ppRawDependencies = rawDependencies.data();
    GW_IF_NVPW_FAILED(NVPW_MetricsEvaluator_GetMetricRawDependencies(&getMetricRawDependenciesParms), sdk_retval, {
        retval = GW_FAILED;
        goto exit;
    });

    for(i = 0; i < rawDependencies.size(); i++){
        raw_metric_names.push_back(rawDependencies[i]);
    }

    for (auto& rawMetricName : raw_metric_names){
        NVPA_RawMetricRequest metricRequest = { NVPA_RAW_METRIC_REQUEST_STRUCT_SIZE };
        metricRequest.pMetricName = rawMetricName.c_str();
        metricRequest.isolated = isolated;
        metricRequest.keepInstances = keep_instances;
        raw_metric_requests.push_back(metricRequest);
    }

exit:
    if(metricEvaluator != nullptr){
        metricEvaluatorDestroyParams.pMetricsEvaluator = metricEvaluator;
        GW_IF_NVPW_FAILED(NVPW_MetricsEvaluator_Destroy(&metricEvaluatorDestroyParams), sdk_retval, {
            retval = GW_FAILED;
        });
    }
    return retval;
}
