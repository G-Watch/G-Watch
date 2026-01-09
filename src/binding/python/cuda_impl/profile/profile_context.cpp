#include <iostream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "nlohmann/json.hpp"
#include "pybind11_json/pybind11_json.hpp"

#include "common/common.hpp"
#include "common/utils/exception.hpp"
#include "profiler/cuda_impl/context.hpp"
#include "profiler/cuda_impl/device.hpp"
#include "profiler/cuda_impl/profiler.hpp"
#include "profiler/cuda_impl/metric.hpp"


void __gw_pybind_cuda_init_profile_context_interface(pybind11::module_ &m){
    pybind11::class_<GWProfileContext_CUDA> _class(m, "ProfileContext");

    _class.def(pybind11::init<bool>());

    _class.def(
        "create_profiler",
        [](GWProfileContext_CUDA &self, int device_id, const std::vector<std::string>& metric_names, std::string profiler_mode_str){
            gw_retval_t retval;
            GWProfiler* gw_profiler;
            gw_profiler_mode_t profiler_mode;
            
            // dispatch profiler mode
            if(profiler_mode_str != "pm" and profiler_mode_str!= "range" and profiler_mode_str != ""){
                throw GWException("invalid profiler mode: %s, please use either 'pm' or 'range'", profiler_mode_str.c_str());
            }
            if(profiler_mode_str == "pm"){
                profiler_mode = GWProfiler_Mode_CUDA_PM_Sampling;
                #if !(GW_CUDA_VERSION_MAJOR >= 12 && GW_CUDA_VERSION_MINOR >= 6)
                    throw GWException("PM sampling is not supported before CUDA 12.6");
                #endif
            } else {
                // default is range profiling mode
                profiler_mode = GWProfiler_Mode_CUDA_Range_Profiling;
            }

            GW_IF_FAILED(self.create_profiler(device_id, metric_names, profiler_mode, &gw_profiler), retval, {
                throw GWException("failed to create profiler: retval: %s", gw_retval_str(retval));
            });
            GW_CHECK_POINTER(gw_profiler);
            return reinterpret_cast<GWProfiler_CUDA*>(gw_profiler);
        }
    );

    _class.def(
        "destroy_profiler",
        [](GWProfileContext_CUDA &self, GWProfiler_CUDA &profiler){
            self.destroy_profiler(&profiler);
        }
    );

    _class.def(
        "get_devices",
        [](GWProfileContext_CUDA &self){
            return self.get_devices();
        }
    );

    _class.def(
        "get_clock_freq",
        [](GWProfileContext_CUDA &self, int device_id){
            gw_retval_t retval;
            std::map<std::string, uint32_t> clock_map;
            GW_IF_FAILED(self.get_clock_freq(device_id, clock_map), retval, {
                throw GWException("failed to create profiler: %s", gw_retval_str(retval));
            });
            return clock_map;
        }
    );
}
