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


void __gw_pybind_cuda_init_profile_device_interface(pybind11::module_ &m){
    pybind11::class_<GWProfileDevice_CUDA> _class(m, "ProfileDevice");

    _class.def(pybind11::init<int>());

    _class.def(
        "export_metric_properties",
        [](GWProfileDevice_CUDA &self, std::string metric_properties_cache_path){
            gw_retval_t retval;
            GW_IF_FAILED(self.export_metric_properties(metric_properties_cache_path), retval, {
                throw GWException("failed to export metric properties: %s", gw_retval_str(retval));
            });
        }
    );
}
