#include <iostream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "nlohmann/json.hpp"
#include "pybind11_json/pybind11_json.hpp"

#include "common/common.hpp"
#include "common/utils/exception.hpp"
#include "capsule/event.hpp"
#include "capsule/trace.hpp"
#include "binding/internal_interface.hpp"


#ifdef GW_BACKEND_CUDA
    #include "binding/python/cuda_impl/interface.hpp"
#endif // GW_BACKEND_CUDA


PYBIND11_MODULE(libpygwatch, m) {
    /* ==================== Trace Event Management ==================== */
    m.def("start_trace", &GW_INTERNAL_start_app_trace);
    m.def("stop_trace", &GW_INTERNAL_stop_app_trace);
    m.def("push_event", &GW_INTERNAL_push_app_event);
    m.def("push_parent_event", &GW_INTERNAL_push_app_parent_event);
    m.def("pop_parent_event", &GW_INTERNAL_pop_app_parent_event);

    pybind11::class_<GWEvent>(m, "GWEvent")
        .def(pybind11::init<std::string>())
        .def("record_tick", [](GWEvent &self, std::string tick_name){
            self.record_tick(tick_name);
        })
        .def("set_metadata", [](GWEvent &self, std::string key, nlohmann::json value){
            self.set_metadata(key, value);
        })
        .def("archive", [](GWEvent &self){
            self.archive();
        })
        .def("is_archived", [](GWEvent &self){
            return self.is_archived();
        })
    ;
    /* ==================== Trace Event Management ==================== */


    /* ==================== Trace Task Management ==================== */
    m.def("create_trace_task", &GW_INTERNAL_create_trace_task);
    m.def("destory_trace_task", &GW_INTERNAL_destory_trace_task);
    m.def("register_trace_task", &GW_INTERNAL_register_trace_task);
    m.def("unregister_trace_task", &GW_INTERNAL_unregister_trace_task);

    pybind11::class_<GWTraceTask>(m, "GWTraceTask")
        .def(pybind11::init<>())
        .def("get_metadata", [](GWTraceTask &self, std::string key){
            gw_retval_t tmp_retval = GW_SUCCESS;
            nlohmann::json value;
            GW_IF_FAILED(
                self.get_metadata(key, value),
                tmp_retval,
                throw GWException("failed to get metadata of GWTraceTask: key(%s)", key.c_str());
            );
            return value;
        })
        .def("set_metadata", [](GWTraceTask &self, std::string key, nlohmann::json value){
            self.set_metadata(key, value);
        })
    ;
    /* ==================== Trace Task Management ==================== */

    
    /* ==================== CUDA ==================== */
    #ifdef GW_BACKEND_CUDA
        __gw_pybind_cuda_init_binary_utilities_interface(m);
        __gw_pybind_cuda_init_cubin_interface(m);
        __gw_pybind_cuda_init_fatbin_interface(m);
        __gw_pybind_cuda_init_ptx_interface(m);
        __gw_pybind_cuda_init_kernel_def_sass_interface(m);
        __gw_pybind_cuda_init_profile_context_interface(m);
        __gw_pybind_cuda_init_profile_device_interface(m);
        __gw_pybind_cuda_init_profile_profiler_interface(m);
    #endif // GW_BACKEND_CUDA
    /* ==================== CUDA ==================== */
}
