#include <iostream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "nlohmann/json.hpp"
#include "pybind11_json/pybind11_json.hpp"

#include "common/common.hpp"
#include "common/utils/exception.hpp"
#include "common/assemble/kernel.hpp"
#include "common/cuda_impl/assemble/kernel_cuda.hpp"
#include "common/cuda_impl/assemble/kernel_def_sass.hpp"
#include "profiler/cuda_impl/context.hpp"
#include "profiler/cuda_impl/device.hpp"
#include "profiler/cuda_impl/profiler.hpp"
#include "profiler/cuda_impl/metric.hpp"
#include "binding/runtime_control.hpp"


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
            if( profiler_mode_str != "pm"
                and profiler_mode_str != "pc"
                and profiler_mode_str != "range"
                and profiler_mode_str != ""
            ){
                throw GWException(
                    "invalid profiler mode: %s, please use either 'pm', 'pc', or 'range'",
                    profiler_mode_str.c_str()
                );
            }
            if(profiler_mode_str == "pm"){
                profiler_mode = GWProfiler_Mode_CUDA_PM_Sampling;
                #if !(GW_CUDA_VERSION_MAJOR >= 12 && GW_CUDA_VERSION_MINOR >= 6)
                    throw GWException("PM sampling is not supported before CUDA 12.6");
                #endif
            } else if(profiler_mode_str == "pc"){
                profiler_mode = GWProfiler_Mode_CUDA_PC_Sampling;
                #if !(GW_CUDA_VERSION_MAJOR >= 9)
                    throw GWException("PC sampling is not supported before CUDA 9");
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


void __gw_pybind_cuda_init_rt_control_interface(pybind11::module_ &m){
    m.def(
        "start_tracing_kernel_launch",
        []()
        {
            gw_rt_control_start_tracing_kernel_launch();
        }
    );

    m.def(
        "stop_tracing_kernel_launch",
        []()
        {
            std::vector<GWKernel*> list_kernel = {};
            std::vector<GWKernelExt_CUDA*> list_kernel_ext = {};
            GWKernelExt_CUDA *kernel_ext = nullptr;
            
            list_kernel = gw_rt_control_stop_tracing_kernel_launch();

            for(auto& kernel : list_kernel){
                kernel_ext = GWKernelExt_CUDA::get_ext_ptr(kernel);
                list_kernel_ext.push_back(kernel_ext);
            }
            
            return list_kernel_ext;
        }
    );

    m.def(
        "get_kernel_def_by_name",
        [](std::string name) -> GWKernelDefExt_CUDA_SASS* {
            GWKernelDef *kernel_def = nullptr;
            GWKernelDefExt_CUDA_SASS *kernel_def_ext = nullptr;
            gw_retval_t retval = GW_SUCCESS;

            GW_IF_FAILED(
                gw_rt_control_get_kernel_def(name, kernel_def),
                retval,
                {
                    kernel_def_ext = nullptr;
                    goto exit;
                }
            );
            GW_CHECK_POINTER(kernel_def_ext = GWKernelDefExt_CUDA_SASS::get_ext_ptr(kernel_def));

         exit:
            return kernel_def_ext;
        },
        py::arg("name"),
        py::return_value_policy::reference,
        "get kernel definition by name"
    );

    m.def(
        "get_map_kerneldef",
        []() -> std::map<std::string, GWKernelDefExt_CUDA_SASS*> {
            gw_retval_t retval = GW_SUCCESS;
            std::map<std::string, GWKernelDef*> _map_name_kerneldef;
            std::map<std::string, GWKernelDefExt_CUDA_SASS*> map_name_kerneldef = {};
            GWKernelDefExt_CUDA_SASS *kernel_def_ext = nullptr;
            GW_IF_FAILED(
                gw_rt_control_get_map_kerneldef(_map_name_kerneldef),
                retval,
                goto exit;
            );

            for(auto& [name, kernel_def] : _map_name_kerneldef){
                GW_CHECK_POINTER(kernel_def_ext = GWKernelDefExt_CUDA_SASS::get_ext_ptr(kernel_def));
                map_name_kerneldef.insert({ name, kernel_def_ext });
            }

         exit:
            return map_name_kerneldef;
        },
        py::return_value_policy::reference,
        "get map of kernel definitions"
    );
}
