#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <stack>

#include "common/common.hpp"
#include "common/log.hpp"
#include "profiler/device.hpp"
#include "profiler/profile_image.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/hash.hpp"
#include "common/utils/timer.hpp"


using gw_profiler_mode_t = uint8_t;


/*!
 *  \brief  CUPTI profiling context
 */
class GWProfiler {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     *  \param  gw_device       the device to profile
     *  \param  metric_names    names of all metrics to collect
     *  \param  profiler_mode   mode of the profiler
     */
    GWProfiler(GWProfileDevice* gw_device, std::vector<std::string> metric_names, gw_profiler_mode_t profiler_mode);


    /*!
     *  \brief  deconstructor
     *  \note   the deconstructor mainly does the following things:
     *          [1] release the primary context of the specified device
     */
    ~GWProfiler();


    /*!
     *  \brief  get the profiler mode
     *  \return the profiler mode
     */
    inline gw_profiler_mode_t get_profiler_mode(){ return this->_profiler_mode; }


    /*!
     *  \brief  obtain the GW device of this profiler
     *  \return the GW device of this profiler
     */
    inline GWProfileDevice* get_gw_deivce(){ return this->_gw_device; }


    // bitmap, to prevent concurrent profiler on the same device
    static std::map<int, GWProfiler*> active_profiler_map; 


    /*!
     *  \brief  obtain the sign of this profiler
     *  \return the sign of this profiler
     */
    inline gw_image_sign_t get_sign(){ return this->_sign; }


 protected:
    // mode of the profiler
    gw_profiler_mode_t _profiler_mode;

    // device which where this profiling context target at
    GWProfileDevice *_gw_device;

    // metrics to collect
    std::vector<std::string> _metric_names;

    // timing ticks
    static GWUtilTscTimer _tsc_timer;
    uint64_t _tick_create, _tick_image_ready;

    // sign of this profiler with specific metric names and chip name
    gw_image_sign_t _sign;
    /* ==================== Common ==================== */
};
