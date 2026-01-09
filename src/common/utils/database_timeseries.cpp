#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <mutex>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/database_timeseries.hpp"

#include <nlohmann/json.hpp>


GWUtilTimeSeriesSample::GWUtilTimeSeriesSample()
{}


GWUtilTimeSeriesSample::~GWUtilTimeSeriesSample()
{}


nlohmann::json GWUtilTimeSeriesSample::serialize() const {
    nlohmann::json object;

    object["index"] = this->index;
    object["timestamp"] = this->timestamp;
    object["payload"] = this->payload;
    
exit:
    return object;
}


gw_retval_t GWUtilTimeSeriesSample::deserialize(const nlohmann::json& data){
    gw_retval_t retval = GW_SUCCESS;
    
    if(data.contains("index")){
        this->index = data["index"];
    } else {
        GW_WARN_C(
            "invalid timeseries sample, missing index: payload(%s)",
            data.dump().c_str()
        );
        retval = GW_FAILED_INVALID_INPUT;
    }

    if(data.contains("timestamp")){
        this->timestamp = data["timestamp"];
    } else {
        GW_WARN_C(
            "invalid timeseries sample, missing timestamp: payload(%s)",
            data.dump().c_str()
        );
        retval = GW_FAILED_INVALID_INPUT;
    }

    if(data.contains("payload")){
        this->payload = data["payload"];
    }

exit:
    return retval;
}


GWUtilTimeSeriesDatabase::GWUtilTimeSeriesDatabase() : GWUtilDatabase()
{}


GWUtilTimeSeriesDatabase::~GWUtilTimeSeriesDatabase()
{}


gw_retval_t GWUtilTimeSeriesDatabase::append(std::string uri, GWUtilTimeSeriesSample& data){
    gw_retval_t retval = GW_SUCCESS;
    typename std::map<void*, std::map<std::string, gw_db_subscribe_callback_t>>::iterator cb_outter_iter;
    typename std::map<std::string, gw_db_subscribe_callback_t>::iterator cb_inner_iter;
    void* user_data = nullptr;
    std::vector<gw_db_subscribe_context_t*> list_subs_ctx = {};
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);

    // execute callback
    this->__get_all_subscribers(nullptr, "", uri, /* uri_precise_match */ true, list_subs_ctx);
    for(auto& subs_ctx : list_subs_ctx){
        GW_IF_FAILED(
            subs_ctx->callback(
                /* subscribe_cxt */ subs_ctx,
                /* new_value */ data.serialize(),
                /* old_value */ ""
            ),
            retval,
            {
                GW_WARN_C("failed to execute callback, set failed: uri(%s)", uri.c_str());
                goto exit;
            }
        );
    }

    // append the data
    this->_map_series[uri].push_back(data);

exit:
    return retval;
}


gw_retval_t GWUtilTimeSeriesDatabase::query(std::string uri, std::vector<GWUtilTimeSeriesSample> &response){
    gw_retval_t retval = GW_SUCCESS;
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);
    return this->query_lockless(uri, response);
}


gw_retval_t GWUtilTimeSeriesDatabase::query_lockless(std::string uri, std::vector<GWUtilTimeSeriesSample> &response){
    gw_retval_t retval = GW_SUCCESS;
    std::string uri_prefix;
    std::vector<nlohmann::json> resource_list;
    
    // we only support precise query for time series database
    if(this->_map_series.count(uri) > 0){
        response = this->_map_series[uri];
    } else {
        GW_WARN_C("failed query, uri not found: uri(%s)", uri.c_str());
        retval = GW_FAILED_NOT_EXIST;
    }

exit:
    return retval;
}
