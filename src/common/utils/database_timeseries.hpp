#pragma once

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <functional>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/database.hpp"

#include <nlohmann/json.hpp>


using gw_tsdb_series_t = std::vector<const nlohmann::json>;


class GWUtilTimeSeriesSample {
 public:
    GWUtilTimeSeriesSample();
    ~GWUtilTimeSeriesSample();
    
    /*!
     *  \brief      serialize the sample into json data
     *  \return     serialized data
     */
    nlohmann::json serialize() const;
    
    
    /*!
     *  \brief      deserialize the sample from json data
     *  \param      json    json data to be deserialized
     *  \return     GW_SUCCESS for successfully deserialized
     */
    gw_retval_t deserialize(const nlohmann::json& data);

    // index of this sample
    uint64_t index = 0;
    
    // timestamp of this sample
    uint64_t timestamp = 0;
    
    // payload of this sample
    nlohmann::json payload = "";


    GWUtilTimeSeriesSample& operator=(const GWUtilTimeSeriesSample& other){
        this->timestamp = other.timestamp;
        this->payload = other.payload;
    }
};


class GWUtilTimeSeriesDatabase final : public GWUtilDatabase {
 public:
    GWUtilTimeSeriesDatabase();
    ~GWUtilTimeSeriesDatabase();
    
    
    /*!
     *  \brief  append new sample to the series
     *  \param  uri     uri of the series
     *  \param  data    new sample
     *  \return GW_SUCCESS for successfully appended
     */
    gw_retval_t append(std::string uri, GWUtilTimeSeriesSample& data);


    /*!
     *  \brief  query samples from the series
     *  \param  uri     uri of the series
     *  \param  response    queried samples
     *  \return GW_SUCCESS for successfully queried
     */
    gw_retval_t query(std::string uri, std::vector<GWUtilTimeSeriesSample> &response);


    /*!
     *  \brief  query samples from the series (lockless)
     *  \note   this function is provided for subscribe initializing function 
     *  \param  uri     uri of the series
     *  \param  response    queried samples
     *  \return GW_SUCCESS for successfully queried
     */
    gw_retval_t query_lockless(std::string uri, std::vector<GWUtilTimeSeriesSample> &response);


 private:
    std::map<std::string, std::vector<GWUtilTimeSeriesSample>> _map_series;
};
