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


void __gw_pybind_cuda_init_profile_profiler_interface(pybind11::module_ &m){
    pybind11::class_<GWProfiler_CUDA> _class(m, "Profiler");

    /* =============== Range Profile APIs =============== */
    _class.def(
        "RangeProfile_start_session",
        [](
            GWProfiler_CUDA &self,
            int max_launches_per_pass,
            int max_ranges_per_pass,
            int cupti_profile_range_mode,
            int cupti_profile_reply_mode,
            int cupti_profile_min_nesting_level,
            int cupti_profile_num_nesting_levels,
            int cupti_profile_target_nesting_levels
        ){
            int sdk_retval;

            GW_ASSERT(
                cupti_profile_range_mode == kGWProfiler_CUDA_RangeProfile_RangeMode_AutoRange
                || cupti_profile_range_mode == kGWProfiler_CUDA_RangeProfile_RangeMode_UserRange
            );

            GW_ASSERT(
                cupti_profile_reply_mode == kGWProfiler_CUDA_RangeProfile_ReplayMode_KernelReplay
                || cupti_profile_reply_mode == kGWProfiler_CUDA_RangeProfile_ReplayMode_UserReplay
            );

            GW_IF_FAILED(
                self.RangeProfile_start_session(
                    max_launches_per_pass,
                    max_ranges_per_pass,
                    static_cast<GWProfiler_CUDA_RangeProfile_RangeMode>(cupti_profile_range_mode),
                    static_cast<GWProfiler_CUDA_RangeProfile_ReplayMode>(cupti_profile_reply_mode),
                    cupti_profile_min_nesting_level,
                    cupti_profile_num_nesting_levels,
                    cupti_profile_target_nesting_levels
                ),
                sdk_retval,
                {
                    throw GWException("failed to start session: retval(%d)", sdk_retval);
                }
            );
        }
    );

    
    _class.def(
        "RangeProfile_destroy_session",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.RangeProfile_destroy_session(), sdk_retval, {
                throw GWException("failed to close session: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def(
        "RangeProfile_is_session_created",
        [](GWProfiler_CUDA &self){
            return self.RangeProfile_is_session_created();
        }
    );


    _class.def(
        "RangeProfile_begin_pass",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.RangeProfile_begin_pass(), sdk_retval, {
                throw GWException("failed to begin pass: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def(
        "RangeProfile_end_pass",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            bool is_last_pass;

            GW_IF_FAILED(self.RangeProfile_end_pass(is_last_pass), sdk_retval, {
                throw GWException("failed to end pass: retval(%d)", sdk_retval);
            });
            
            return is_last_pass;
        }
    );


    _class.def(
        "RangeProfile_enable_profiling",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.RangeProfile_enable_profiling(), sdk_retval, {
                throw GWException("failed to enable profiling: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def(
        "RangeProfile_disable_profiling",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.RangeProfile_disable_profiling(), sdk_retval, {
                throw GWException("failed to disable profiling: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def(
        "RangeProfile_push_range",
        [](GWProfiler_CUDA &self, std::string range_name){
            int sdk_retval;
            GW_IF_FAILED(self.RangeProfile_push_range(range_name), sdk_retval, {
                throw GWException(
                    "failed to push range: retval(%d), range_name(%s)",
                    sdk_retval, range_name.c_str()
                );
            });
        }
    );


    _class.def(
        "RangeProfile_pop_range",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.RangeProfile_pop_range(), sdk_retval, {
                throw GWException("failed to pop range: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def(
        "RangeProfile_flush_data",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.RangeProfile_flush_data(), sdk_retval, {
                throw GWException("failed to flush data: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def(
        "RangeProfile_get_metrics",
        [](GWProfiler_CUDA &self) -> std::map<std::string, std::map<std::string, double>> {
            int sdk_retval;
            std::map<std::string, std::map<std::string, double>> value_map;

            GW_IF_FAILED(self.RangeProfile_get_metrics(value_map), sdk_retval, {
                throw GWException("failed to get profiling metric values: retval(%d)", sdk_retval);
            });

            return value_map;
        }
    );


    /* =============== PM Sampling APIs =============== */
    _class.def(
        "PmSampling_enable_profiling",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.PmSampling_enable_profiling(), sdk_retval, {
                throw GWException("failed to enable PM sampling profiling: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def(
        "PmSampling_disable_profiling",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.PmSampling_disable_profiling(), sdk_retval, {
                throw GWException("failed to disable PM sampling profiling: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def(
        "PmSampling_set_config",
        [](GWProfiler_CUDA &self, uint64_t hw_buf_size, uint64_t sampling_interval, uint32_t max_samples){
            int sdk_retval;
            GW_IF_FAILED(self.PmSampling_set_config(hw_buf_size, sampling_interval, max_samples), sdk_retval, {
                throw GWException("failed to setup PM sampling: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def(
        "PmSampling_start_profiling", [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.PmSampling_start_profiling(), sdk_retval, {
                throw GWException("failed to setup PM sampling: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def(
        "PmSampling_stop_profiling",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.PmSampling_stop_profiling(), sdk_retval, {
                throw GWException("failed to stop PM sampling: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def(
        "PmSampling_get_metrics",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            std::vector<gw_profiler_pm_sample_t> _result;
            GW_IF_FAILED(self.PmSampling_get_metrics(_result), sdk_retval, {
                throw GWException("failed to retrieve PM sampling result: retval(%d)", sdk_retval);
            });
            return nlohmann::json(_result).dump();
        }
    );


    /* =============== Checkpoint/Restore APIs =============== */
    _class.def(
        "checkpoint",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.checkpoint(), sdk_retval, {
                throw GWException("failed to checkpoint: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def(
        "restore",
        [](GWProfiler_CUDA &self, bool do_pop){
            int sdk_retval;
            GW_IF_FAILED(self.restore(do_pop), sdk_retval, {
                throw GWException("failed to restore: retval(%d)", sdk_retval);
            });
        }
    );


    _class.def("free_checkpoint",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.free_checkpoint(), sdk_retval, {
                throw GWException("failed to free checkpoint: retval(%d)", sdk_retval);
            });
        }
    );


    /* =============== Common APIs =============== */
    _class.def(
        "reset_counter_data",
        [](GWProfiler_CUDA &self){
            int sdk_retval;
            GW_IF_FAILED(self.reset_counter_data(), sdk_retval, {
                throw GWException("failed to reset counter data: retval(%d)", sdk_retval);
            });
        }
    );
}
