#pragma once
#include <iostream>
#include <map>
#include <any>
#include <string>

#include <stdint.h>
#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/message.hpp"
#include "profiler/profiler.hpp"


/*!
 *  \brief  message type for communication between scheduler and capsule
 */
enum :gw_message_typeid_t {
    // meessage for setting profiling metrics
    GW_MESSAGE_TYPEID_PROFILER_SET_METRICS = 100,

    // message for begin running the profiler
    GW_MESSAGE_TYPEID_PROFILER_BEGIN,

    // message for end the running profiler
    GW_MESSAGE_TYPEID_PROFILER_END
};


class GWInternalMessagePayload_Profiler_SetMetrics final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Profiler_SetMetrics() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Profiler_SetMetrics() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["metric_names"] = this->metric_names;
        object["sign"] = this->sign;
        object["success"] = this->success;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("metric_names"))
            this->metric_names = json["metric_names"];
        if(json.contains("sign"))
            this->sign = json["sign"];
        if(json.contains("success"))
            this->success = json["success"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_PROFILER_SET_METRICS;

    // metric names to the profiled
    std::vector<std::string> metric_names;

    // sign of the metric name and device chip name
    gw_image_sign_t sign;

    // status
    bool success = false;
};


class GWInternalMessagePayload_Profiler_End final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Profiler_End() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Profiler_End() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["device_id"] = this->device_id;
        object["map_metric_results"] = this->map_metric_results;
        object["success"] = this->success;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("device_id"))
            this->device_id = json["device_id"];
        if(json.contains("map_metric_results"))
            this->map_metric_results = json["map_metric_results"];
        if(json.contains("success"))
            this->success = json["success"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_PROFILER_END;

    // device id to be retreived result from
    int device_id = -1;

    // map of metric results
    std::map<std::string, double> map_metric_results;

    // status
    bool success = false;
};


#ifdef GW_BACKEND_CUDA
    #include "profiler/cuda_impl/profiler.hpp"


class GWInternalMessagePayload_Profiler_Begin final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Profiler_Begin() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Profiler_Begin() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["device_id"] = this->device_id;
        object["sign"] = this->sign;
        object["max_launches_per_pass"] = this->max_launches_per_pass;
        object["max_ranges_per_pass"] = this->max_ranges_per_pass;
        object["cupti_profile_range_mode"] = this->cupti_profile_range_mode;
        object["cupti_profile_reply_mode"] = this->cupti_profile_reply_mode;
        object["cupti_profile_min_nesting_level"] = this->cupti_profile_min_nesting_level;
        object["cupti_profile_num_nesting_levels"] = this->cupti_profile_num_nesting_levels;
        object["cupti_profile_target_nesting_levels"] = this->cupti_profile_target_nesting_levels;
        object["success"] = this->success;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("device_id"))
            this->device_id = json["device_id"];
        if(json.contains("sign"))
            this->sign = json["sign"];
        if(json.contains("max_launches_per_pass"))
            this->max_launches_per_pass = json["max_launches_per_pass"];
        if(json.contains("max_ranges_per_pass"))
            this->max_ranges_per_pass = json["max_ranges_per_pass"];
        if(json.contains("cupti_profile_range_mode"))
            this->cupti_profile_range_mode 
                = static_cast<GWProfiler_CUDA_RangeProfile_RangeMode>(json["cupti_profile_range_mode"]);
        if(json.contains("cupti_profile_reply_mode"))
            this->cupti_profile_reply_mode
                = static_cast<GWProfiler_CUDA_RangeProfile_ReplayMode>(json["cupti_profile_reply_mode"]);
        if(json.contains("cupti_profile_min_nesting_level"))
            this->cupti_profile_min_nesting_level = json["cupti_profile_min_nesting_level"];
        if(json.contains("cupti_profile_num_nesting_levels"))
            this->cupti_profile_num_nesting_levels = json["cupti_profile_num_nesting_levels"];
        if(json.contains("cupti_profile_target_nesting_levels"))
            this->cupti_profile_target_nesting_levels = json["cupti_profile_target_nesting_levels"];
        if(json.contains("success"))
            this->success = json["success"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_PROFILER_BEGIN;

    // device id to be profiled
    int device_id = -1;

    // sign of the metrics to be start profiling
    gw_image_sign_t sign;

    // params for start a new profile session
    int max_launches_per_pass = GW_CUPTI_DEFAULT_MAX_LAUNCHES_PER_PASS;
    int max_ranges_per_pass = GW_CUPTI_DEFAULT_MAX_RANGES_PER_PASS;
    GWProfiler_CUDA_RangeProfile_RangeMode cupti_profile_range_mode = 
        GWProfiler_CUDA_RangeProfile_RangeMode::kGWProfiler_CUDA_RangeProfile_RangeMode_UserRange;
    GWProfiler_CUDA_RangeProfile_ReplayMode cupti_profile_reply_mode =
        GWProfiler_CUDA_RangeProfile_ReplayMode::kGWProfiler_CUDA_RangeProfile_ReplayMode_UserReplay;
    int cupti_profile_min_nesting_level = GW_CUPTI_DEFAULT_MIN_NESTING_LEVEL;
    int cupti_profile_num_nesting_levels = GW_CUPTI_DEFAULT_NUM_NESTING_LEVELS;
    int cupti_profile_target_nesting_levels = GW_CUPTI_DEFAULT_TARGET_NESTING_LEVELS;

    // status
    bool success = false;
};
#else
    // add support for ROCm
#endif


class GWInternalMessage_Profiler final : public GWInternalMessage<
    GWInternalMessagePayload_Common_Heartbeat,
    GWInternalMessagePayload_Profiler_SetMetrics,
    GWInternalMessagePayload_Profiler_Begin,
    GWInternalMessagePayload_Profiler_End
>{
 public:
    GWInternalMessage_Profiler() : GWInternalMessage() {}
    ~GWInternalMessage_Profiler() = default;
};
