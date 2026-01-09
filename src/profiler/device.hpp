#pragma once

#include <iostream>
#include <vector>
#include <string>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"
#include "profiler/metric.hpp"


/*!
 *  \brief  represent an underlying hardware device
 */
class GWProfileDevice {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     *  \param  device_id       index of the device
     */
    GWProfileDevice(int device_id);
    ~GWProfileDevice();


    /*!
     *  \brief  obtain the chip name of the device
     *  \return chip name
     */
    inline std::string get_chip_name(){ return this->_chip_name; }


    /*!
     *  \brief  obtain the device id
     *  \return device id
     */
    inline int get_device_id() const { return this->_device_id; }


 protected:
    // index of the device
    int _device_id;

    // chip name of the device
    std::string _chip_name;
    /* ==================== Common ==================== */


    /* ==================== Metrics ==================== */
 public:
    /*!
     *  \brief  export metric properties of current device
     *  \param  metric_properties_cache_path    path to output metric properties
     */
    virtual gw_retval_t export_metric_properties(std::string metric_properties_cache_path=""){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }
 

 protected:
    // all metric supported in this device
    std::vector<GWProfileMetric*> _gw_metrics;
    /* ==================== Metrics ==================== */
};
