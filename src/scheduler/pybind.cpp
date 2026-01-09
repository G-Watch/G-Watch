#include <iostream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "common/common.hpp"
#include "common/utils/exception.hpp"
#include "scheduler/scheduler.hpp"
#include "scheduler/agent/context.hpp"


PYBIND11_MODULE(libgwatch_scheduler, m) {
    /* ==================== Scheduler ==================== */
    pybind11::class_<GWScheduler>(m, "GWScheduler")
        .def(pybind11::init<gw_scheduler_config_t>())
        .def("serve", [](GWScheduler &self){
            gw_retval_t retval;
            GW_IF_FAILED(
                self.serve(),
                retval,
                throw GWException("scheduler failed to serve");
            );
        })
        .def("start_capsule", [](
            GWScheduler &self, std::vector<std::string> capsule_command, std::set<std::string> options
        ){
            gw_retval_t retval;
            GW_IF_FAILED(
                self.start_capsule(capsule_command, options),
                retval,
                throw GWException("scheduler failed to start capsule");
            );
        })
        .def("start_gtrace", [](GWScheduler &self){
            gw_retval_t retval;
            GW_IF_FAILED(
                self.start_gtrace(),
                retval,
                throw GWException("scheduler failed to start gtrace");
            );
        })
       .def("get_capsule_world_size", [](GWScheduler &self){
            return self.get_capsule_world_size();
       })
    ;

    /* ==================== Agent ==================== */
    pybind11::class_<gw_scheduler_config_t>(m, "GWSchedulerConfig")
        .def(pybind11::init<>())
        .def_readwrite(
            "COMMON_job_name",
            &gw_scheduler_config_t::COMMON_job_name
        )
        .def_readwrite(
            "COMMON_python_package_installtion_path",
            &gw_scheduler_config_t::COMMON_python_package_installtion_path
        )
        .def_readwrite(
            "COMMON_log_path",
            &gw_scheduler_config_t::COMMON_log_path
        )
        .def_readwrite(
            "COMMON_enable_visualize",
            &gw_scheduler_config_t::COMMON_enable_visualize
        )
        .def_readwrite(
            "AGENT_enable",
            &gw_scheduler_config_t::AGENT_enable
        )
        .def_readwrite(
            "AGENT_workdir",
            &gw_scheduler_config_t::AGENT_workdir
        )
    ;
}
