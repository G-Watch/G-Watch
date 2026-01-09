#include <iostream>
#include <map>

#include "common/common.hpp"
#include "profiler/profiler.hpp"
#include "common/log.hpp"
#include "profiler/device.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/timer.hpp"
#include "common/utils/hash.hpp"


GWUtilTscTimer GWProfiler::_tsc_timer;
std::map<int, GWProfiler*> GWProfiler::active_profiler_map;


GWProfiler::GWProfiler(GWProfileDevice* gw_device, std::vector<std::string> metric_names, gw_profiler_mode_t profiler_mode)
    :   _gw_device(gw_device),
        _metric_names(metric_names),
        _profiler_mode(profiler_mode),
        _tick_create(0),
        _tick_image_ready(0)
{
    GW_CHECK_POINTER(gw_device);

    // check device conflict
    if(unlikely(
        this->active_profiler_map.count(gw_device->get_device_id()) > 0
    )){
        throw GWException(
            "failed to create profiler, concurrent profiler on the same device: device_id(%d), previous_profiler(%p)",
            gw_device->get_device_id(), this->active_profiler_map[gw_device->get_device_id()]
        );
    }
    this->active_profiler_map[gw_device->get_device_id()] = this;
}


GWProfiler::~GWProfiler(){
    GW_CHECK_POINTER(this->_gw_device);
    GW_ASSERT(this->active_profiler_map.count(this->_gw_device->get_device_id()) == 1);
    GW_CHECK_POINTER(this->active_profiler_map[this->_gw_device->get_device_id()]);

    // unregister from global map
    this->active_profiler_map.erase(this->_gw_device->get_device_id());
}
