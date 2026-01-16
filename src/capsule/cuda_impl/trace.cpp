#pragma once

#include <iostream>
#include <vector>
#include <csignal>

#include <pthread.h>

#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/cuda.hpp"
#include "common/utils/string.hpp"
#include "common/cuda_impl/binary/cubin.hpp"
#include "common/cuda_impl/assemble/kernel_cuda.hpp"
#include "common/cuda_impl/assemble/kernel_def_sass.hpp"
#include "capsule/capsule.hpp"
#include "capsule/event.hpp"
#include "capsule/hijack/cuda_impl/runtime.hpp"
#include "common/cuda_impl/real_apis.hpp"
#include "capsule/cuda_impl/trace.hpp"
#include "profiler/cuda_impl/profiler.hpp"


GWTraceTask_CUDA::GWTraceTask_CUDA()
    : GWTraceTask()
{}


GWTraceTask_CUDA::~GWTraceTask_CUDA()
{}


gw_retval_t GWTraceTask_CUDA::__execute_instrument_cxt(GWCapsule* capsule, GWInstrumentCxt* instrument_cxt) const {
    using cu_module_load_data_func_t = CUresult(CUmodule*, const void*);
    using cu_module_get_function_func_t = CUresult(CUfunction*, CUmodule, const char*);
    using cu_func_get_attribute_func_t = CUresult(int*, CUfunction_attribute, CUfunction);
    using cu_launch_kernel_func_t = CUresult(
        CUfunction, unsigned int, unsigned int, unsigned int,
        unsigned int, unsigned int, unsigned int, unsigned int,
        CUstream, void**, void**
    );
    using cu_stream_synchronize_t = CUresult(CUstream);

    gw_retval_t retval = GW_SUCCESS, tmp_retval = GW_SUCCESS;
    GWKernel *kernel = nullptr;
    GWKernelDef *kernel_def = nullptr;
    GWKernelExt_CUDA *kernel_ext_cuda = nullptr;
    GWKernelDefExt_CUDA_SASS *kernel_def_ext_cuda_sass = nullptr;
    GWBinaryImage *binary_cubin = nullptr;
    GWBinaryImageExt_CUDACubin *binary_ext_cuda_cubin = nullptr;
    uint64_t i = 0, cpu_thread_id = 0;
    std::vector<void*> new_parameter_list;
    CUresult cudv_retval = CUDA_SUCCESS;
    CUfunction cu_function_instrumented = (CUfunction)0;
    CUmodule cu_module = (CUmodule)0;
    GWEvent *trace_event = nullptr, *ckpt_event = nullptr, *restore_event = nullptr;
    GWEvent *instrument_event = nullptr, *run_event = nullptr, *collect_event = nullptr;
    GWEvent *load_event = nullptr, *unload_event = nullptr;
    GWCapsule::gw_capsule_cuda_state_t cuda_state = {
        .cupti_ckpt = nullptr,
        .cu_context = (CUcontext)0,
        .cu_device = (CUdevice)0,
        .saved_memory_size = 0
    };
    std::map<uint64_t, std::map<uint32_t, uint64_t>> map_pc_sampling_result = {};
    std::vector<std::map<uint64_t, std::map<uint32_t, uint64_t>>> list_map_pc_sampling_result;


    /*!
     *  \brief  utility function for merging PC sampling result
     *  \param  maps  PC sampling result
     *  \return merged PC sampling result
     */
    auto __util_merge_pc_sampling_result = [](const std::vector<std::map<uint64_t, std::map<uint32_t, uint64_t>>>& maps) 
        -> std::map<uint64_t, std::map<uint32_t, uint64_t>>
    {
        std::map<uint64_t, std::map<uint32_t, uint64_t>> result;
        for (auto const& m : maps) {
            for (auto const& [outer_k, inner_map] : m) {
                for (auto const& [inner_k, v] : inner_map) {
                    result[outer_k][inner_k] += v;
                }
            }
        }
        return result;
    };

    
    auto __check_and_set_function_attribute = [&](CUfunction _cu_function) -> gw_retval_t {
        gw_retval_t _retval = GW_SUCCESS;
        CUresult _cudv_retval = CUDA_SUCCESS;
        CUdevice _current_cuda_dev = (CUdevice)0;
        int _dev_attr_smem_size = 0;
        int _func_attr_static_smem_size = 0, _func_attr_max_smem_size = 0;

        // obtain current_device configurations
        GW_IF_FAILED(
            GWUtilCUDA::get_current_device(_current_cuda_dev),
            _retval,
            {
                GW_WARN_C(
                    "failed to obtain current device: error(%s)",
                    gw_retval_str(_retval)
                );
                goto _exit;
            }
        );
        GW_IF_CUDA_DRIVER_FAILED(
            cuDeviceGetAttribute(&_dev_attr_smem_size, CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK_OPTIN, _current_cuda_dev),
            _cudv_retval,
            {
                GW_WARN_C(
                    "failed to get device attribute CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK: error(%s)",
                    GWUtilCUDA::get_driver_error_string(_cudv_retval).c_str()
                );
                goto _exit;
            }
        );

        // check and set the function attributes (e.g., smem config)
        GW_IF_CUDA_DRIVER_FAILED(
            cuFuncGetAttribute(&_func_attr_static_smem_size, CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES, _cu_function),
            _cudv_retval,
            {
                GW_WARN_C(
                    "failed to get function attribute CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES: error(%s)",
                    GWUtilCUDA::get_driver_error_string(_cudv_retval).c_str()
                );
                goto _exit;
            }
        );
        GW_IF_CUDA_DRIVER_FAILED(
            cuFuncGetAttribute(&_func_attr_max_smem_size, CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, _cu_function),
            _cudv_retval,
            {
                GW_WARN_C(
                    "failed to get function attribute CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES: error(%s)",
                    GWUtilCUDA::get_driver_error_string(_cudv_retval).c_str()
                );
                goto _exit;
            }
        );
        if(unlikely(_func_attr_max_smem_size < kernel->shared_mem_bytes)){
            // https://docs.nvidia.com/cuda/archive/13.0.0/cuda-driver-api/group__CUDA__EXEC.html#group__CUDA__EXEC_1g0e37dce0173bc883aa1e5b14dd747f26
            if(unlikely(kernel->shared_mem_bytes + _func_attr_static_smem_size > _dev_attr_smem_size)){
                GW_WARN_C(
                    "failed to collect tracing results, "
                    "the usage of SMEM exceed the hardware limit: "
                    "kernel(%s), "
                    "dynamic smem(%d), "
                    "static smem(%d), "
                    "dev_attr_smem_size(%d)",
                    kernel_def->mangled_prototype.c_str(),
                    kernel->shared_mem_bytes,
                    _func_attr_static_smem_size,
                    _dev_attr_smem_size
                );
                _retval = GW_FAILED_INVALID_INPUT;
                goto _exit;
            } else {
                GW_IF_CUDA_DRIVER_FAILED(
                    cuFuncSetAttribute(_cu_function, CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, kernel->shared_mem_bytes),
                    _cudv_retval,
                    {
                        GW_WARN_C(
                            "failed to set function attribute CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES: error(%s)",
                            GWUtilCUDA::get_driver_error_string(_cudv_retval).c_str()
                        );
                        goto _exit;
                    }
                );
            }
        }

    _exit:
        return _retval;
    };


    /*!
     *  \brief  lambda for executing instrumentation context once to colloect profiling results
     *  \todo   add support for range profiling
     *  \param  _cu_function    CUDA function to be profiled
     *  \param  _profiler_mode  profiler mode
     *  \return GW_SUCCESS if success
     */
    auto __execute_instrument_cxt_for_collecting_profiler_result = [&](
        CUfunction _cu_function, void **_params, gw_profiler_mode_t _profiler_mode, uint64_t _repeat_times=16
    ) -> gw_retval_t {
        gw_retval_t _retval = GW_SUCCESS;
        GWProfiler *_profiler = nullptr;
        GWProfiler_CUDA *_profiler_pc_sampling = nullptr;
        CUresult _cudv_retval = CUDA_SUCCESS;
        GWEvent *_pc_sampling_event = nullptr;
        int _device_id = 0;
        uint64_t _i = 0;
        uint64_t _profile_time = 0;
        CUlaunchConfig cu_launch_config = {};
        std::map<uint64_t, std::map<uint32_t, uint64_t>> _map_pc_sampling_result = {};

        std::vector<uint64_t> _list_param_offset = {}, _list_param_size = {};
        std::vector<uint64_t> _list_parameter_value = {};
        uint64_t _nb_params = 0;
 
        GW_CHECK_POINTER(trace_event);

        // obtain function parameter info
        GW_IF_FAILED(
            GWUtilCUDA::get_func_param_info(_cu_function, _list_param_offset, _list_param_size, _nb_params),
            _retval,
            {
                GW_WARN_C(
                    "failed to obtain function parameter info: error(%s)",
                    gw_retval_str(_retval)
                );
                goto _real_exit;
            }
        );
        _list_parameter_value.resize(_nb_params);
        for(_i = 0; _i < _nb_params; _i++){ _list_parameter_value[_i] = (uint64_t)(_params[_i]); }

        if(_profiler_mode == GWProfiler_Mode_CUDA_PC_Sampling){
            GW_CHECK_POINTER(_pc_sampling_event = new GWEvent("[Profile] PC Sampling"));
        } else {
            GW_CHECK_POINTER(_pc_sampling_event = new GWEvent("[Profile] Unknown"));
        }
        _pc_sampling_event->type_id = GW_EVENT_TYPE_GWATCH;
        _pc_sampling_event->thread_id = cpu_thread_id;
        capsule->ensure_event_trace();
        GWCapsule::event_trace->push_event(_pc_sampling_event);
        GWCapsule::event_trace->push_parent_event(_pc_sampling_event);

        _pc_sampling_event->record_tick("begin");

        // obtain the current device id
        GW_IF_FAILED(
            GWUtilCUDA::get_current_device_id(_device_id),
            _retval,
            {
                GW_WARN_C(
                    "failed to collect profiler result, failed to obtain current device id: error(%s)",
                    gw_retval_str(_retval)
                );
                goto _real_exit;
            }
        );

        // obtain the device profiling lock
        capsule->profile_context_cuda->lock_device(_device_id);

        // create the profiler if it's not existed, and enable it, then start it
        if(_profiler_mode == GWProfiler_Mode_CUDA_PC_Sampling){
            GW_IF_FAILED(
                capsule->profile_context_cuda->create_profiler(
                    /* device_id */ _device_id,
                    /* metric_names */ {},
                    /* profiler_mode */ GWProfiler_Mode_CUDA_PC_Sampling,
                    /* gw_profiler */ &_profiler
                ),
                _retval,
                {
                    GW_WARN_C(
                        "failed to create profiler for PC Sampling: "
                        "kernel(%s), error(%s)",
                        kernel_def->mangled_prototype.c_str(), gw_retval_str(_retval)
                    );
                    goto _exit;
                }
            );
            GW_CHECK_POINTER(_profiler_pc_sampling = reinterpret_cast<GWProfiler_CUDA*>(_profiler));

            GW_IF_FAILED(
                _profiler_pc_sampling->PcSampling_enable_profiling(/* kernel */ kernel->get_def()),
                _retval,
                {
                    GW_WARN_C("failed to enable pc sampling");
                    goto _exit;
                }
            );
        
        _start_pm_sampling_profiler:
            GW_IF_FAILED(
                _profiler_pc_sampling->PcSampling_start_profiling(),
                _retval,
                {
                    GW_WARN_C("failed to start pc sampling");
                    goto _exit;
                }
            );
        } else {
            GW_ERROR_C_DETAIL("shouldn't be here");
        }

        // run the kernel
        cu_launch_config.gridDimX = kernel->grid_dim_x;
        cu_launch_config.gridDimY = kernel->grid_dim_y;
        cu_launch_config.gridDimZ = kernel->grid_dim_z;
        cu_launch_config.blockDimX = kernel->block_dim_x;
        cu_launch_config.blockDimY = kernel->block_dim_y;
        cu_launch_config.blockDimZ = kernel->block_dim_z;
        cu_launch_config.sharedMemBytes = kernel->shared_mem_bytes + instrument_cxt->added_shared_memory_size;
        cu_launch_config.attrs = kernel_ext_cuda->params().attrs;
        cu_launch_config.numAttrs = kernel_ext_cuda->params().num_attrs;
        GW_IF_CUDA_DRIVER_FAILED(
            cuLaunchKernelEx(&cu_launch_config, _cu_function, _params, kernel_ext_cuda->params().extra),
            _cudv_retval,
            {
                GW_WARN_C(
                    "failed to launch function for collecting profiler result, "
                    "failed to call cuLaunchKernelEx: "
                    "kernel(%s), CUresult(%d), "
                    "gridDim(%d, %d, %d), blockDim(%d, %d, %d), "
                    "sharedMemBytes(%d), attrs(%p), numAttrs(%d), "
                    "param_offset(%s), param_size(%s), param_value(%s)",
                    kernel_def->mangled_prototype.c_str(), _cudv_retval,
                    cu_launch_config.gridDimX, cu_launch_config.gridDimY, cu_launch_config.gridDimZ,
                    cu_launch_config.blockDimX, cu_launch_config.blockDimY, cu_launch_config.blockDimZ,
                    cu_launch_config.sharedMemBytes, cu_launch_config.attrs, cu_launch_config.numAttrs,
                    GWUtilString::vector_to_string(_list_param_offset, false).c_str(),
                    GWUtilString::vector_to_string(_list_param_size, false).c_str(),
                    GWUtilString::vector_to_string(_list_parameter_value, true).c_str()
                );

                _pc_sampling_event->set_metadata(
                    "error", "Failed to call cuLaunchKernelEx: " + GWUtilCUDA::get_driver_error_string(_cudv_retval)
                );

                retval = GW_FAILED_SDK;
                goto _exit;
            }
        );
        GW_IF_CUDA_DRIVER_FAILED(
            cuStreamSynchronize((CUstream)(kernel->stream)),
            _cudv_retval,
            {
                GW_WARN_C(
                    "failed to launch function for collecting profiler result, "
                    "failed to call cuStreamSynchronize: "
                    "kernel(%s), CUresult(%d), "
                    "gridDim(%d, %d, %d), blockDim(%d, %d, %d), "
                    "sharedMemBytes(%d), attrs(%p), numAttrs(%d), "
                    "param_offset(%s), param_size(%s), param_value(%s)",
                    kernel_def->mangled_prototype.c_str(), _cudv_retval,
                    cu_launch_config.gridDimX, cu_launch_config.gridDimY, cu_launch_config.gridDimZ,
                    cu_launch_config.blockDimX, cu_launch_config.blockDimY, cu_launch_config.blockDimZ,
                    cu_launch_config.sharedMemBytes, cu_launch_config.attrs, cu_launch_config.numAttrs,
                    GWUtilString::vector_to_string(_list_param_offset, false).c_str(),
                    GWUtilString::vector_to_string(_list_param_size, false).c_str(),
                    GWUtilString::vector_to_string(_list_parameter_value, true).c_str()
                );
                
                _pc_sampling_event->set_metadata(
                    "error", "Failed to call cuStreamSynchronize: " + GWUtilCUDA::get_driver_error_string(_cudv_retval)
                );
                
                retval = GW_FAILED_SDK;
                goto _exit;
            }
        );
        GW_DEBUG_C(
            "launched function for collecting profiler result: "
            "kernel(%s), CUresult(%d), "
            "gridDim(%d, %d, %d), blockDim(%d, %d, %d), "
            "sharedMemBytes(%d), attrs(%p), numAttrs(%d), "
            "param_offset(%s), param_size(%s), param_value(%s)",
            kernel_def->mangled_prototype.c_str(), _cudv_retval,
            cu_launch_config.gridDimX, cu_launch_config.gridDimY, cu_launch_config.gridDimZ,
            cu_launch_config.blockDimX, cu_launch_config.blockDimY, cu_launch_config.blockDimZ,
            cu_launch_config.sharedMemBytes, cu_launch_config.attrs, cu_launch_config.numAttrs,
            GWUtilString::vector_to_string(_list_param_offset, false).c_str(),
            GWUtilString::vector_to_string(_list_param_size, false).c_str(),
            GWUtilString::vector_to_string(_list_parameter_value, true).c_str()
        );

        // stop the profiler, and collect results
        if(_profiler_mode == GWProfiler_Mode_CUDA_PC_Sampling){
            GW_IF_FAILED(
                _profiler_pc_sampling->PcSampling_stop_profiling(),
                _retval,
                {
                    GW_WARN_C("failed to stop pc sampling");
                    _pc_sampling_event->set_metadata("error", "Failed to stop pc sampling");
                    goto _exit;
                }
            );

            GW_IF_FAILED(
                _profiler_pc_sampling->PcSampling_get_metrics(_map_pc_sampling_result),
                _retval,
                {
                    GW_WARN_C("failed to get pc sampling metrics");
                    _pc_sampling_event->set_metadata("error", "Failed to get pc sampling metrics");
                    goto _exit;
                }
            );
            list_map_pc_sampling_result.push_back(_map_pc_sampling_result);

            // record stall reason string (for once)
            if(!instrument_cxt->has_trace_result(
                /* panel_name */ "sass_analysis",
                /* key */ "aux_pc_sampling_stall_reason"
            )){
                instrument_cxt->set_trace_result(
                    /* panel_name */ "sass_analysis",
                    /* key */ "aux_pc_sampling_stall_reason", 
                    /* value */ _profiler_pc_sampling->PcSampling_get_stall_reason()
                );
            }

            _profile_time += 1;
            if(_profile_time < _repeat_times){
                goto _start_pm_sampling_profiler;
            }

            GW_IF_FAILED(
                _profiler_pc_sampling->PcSampling_disable_profiling(),
                _retval,
                {
                    GW_WARN_C("failed to disable pc sampling");
                    _pc_sampling_event->set_metadata("error", "Failed to disable pc sampling");
                    goto _exit;
                }
            );

            // destory the profiler
            capsule->profile_context_cuda->destroy_profiler(_profiler);
        } else {
            GW_ERROR_C_DETAIL("shouldn't be here");
        }

        // restore GPU memory state
        // GW_IF_FAILED(
        //     capsule->CUDA_restore_from_latest(/* do_pop */ false),
        //     _retval,
        //     {
        //         GW_WARN_C(
        //             "failed to restore CUDA state after collecting profiler result: "
        //             "kernel(%s), error(%s)",
        //             kernel_def->mangled_prototype.c_str(), gw_retval_str(_retval)
        //         );
        //         _pc_sampling_event->set_metadata("error", "Failed to restore CUDA state after collecting profiler metrics");
        //         goto _exit;
        //     }
        // );

    _exit:
        // unlock device profiler lock
        capsule->profile_context_cuda->unlock_device(_device_id);

    _real_exit:
        _pc_sampling_event->record_tick("end");
        GWCapsule::event_trace->pop_parent_event(_pc_sampling_event);
        _pc_sampling_event->archive();
        return _retval;
    };


    /*!
     *  \brief  lambda for executing instrumentation context once to colloect tracing results
     *  \param  _cu_function        instrumented CUfunction to be launched
     *  \param  _parameter_list     parameter list of the CUfunction
     *  \return GW_SUCCESS if success
     */
    auto __execute_instrument_cxt_for_collecting_trace_result = [&](CUfunction _cu_function, void **_params) -> gw_retval_t {
        gw_retval_t _retval = GW_SUCCESS;
        uint64_t _i = 0;
        CUresult _cudv_retval = CUDA_SUCCESS;
        GWEvent *_trace_event = nullptr;
        CUlaunchConfig cu_launch_config = {};
        std::vector<uint64_t> _list_param_offset = {}, _list_param_size = {};
        std::vector<uint64_t> _list_parameter_value = {};
        uint64_t _nb_params = 0;
        
        GW_CHECK_POINTER(trace_event);

        // obtain function parameter info
        GW_IF_FAILED(
            GWUtilCUDA::get_func_param_info(_cu_function, _list_param_offset, _list_param_size, _nb_params),
            _retval,
            {
                GW_WARN_C(
                    "failed to obtain function parameter info: error(%s)",
                    gw_retval_str(_retval)
                );
                goto _exit;
            }
        );
        _list_parameter_value.resize(_nb_params);
        for(_i = 0; _i < _nb_params; _i++){ _list_parameter_value[_i] = (uint64_t)(_params[_i]); }

        GW_CHECK_POINTER(_trace_event = new GWEvent("[Trace]"));
        _trace_event->type_id = GW_EVENT_TYPE_GWATCH;
        _trace_event->thread_id = cpu_thread_id;
        capsule->ensure_event_trace();
        GWCapsule::event_trace->push_event(_trace_event);
        GWCapsule::event_trace->push_parent_event(_trace_event);

        _trace_event->record_tick("begin");

        cu_launch_config.gridDimX = kernel->grid_dim_x;
        cu_launch_config.gridDimY = kernel->grid_dim_y;
        cu_launch_config.gridDimZ = kernel->grid_dim_z;
        cu_launch_config.blockDimX = kernel->block_dim_x;
        cu_launch_config.blockDimY = kernel->block_dim_y;
        cu_launch_config.blockDimZ = kernel->block_dim_z;
        cu_launch_config.sharedMemBytes = kernel->shared_mem_bytes + instrument_cxt->added_shared_memory_size;
        cu_launch_config.attrs = kernel_ext_cuda->params().attrs;
        cu_launch_config.numAttrs = kernel_ext_cuda->params().num_attrs;
        GW_IF_CUDA_DRIVER_FAILED(
            cuLaunchKernelEx(&cu_launch_config, _cu_function, _params, kernel_ext_cuda->params().extra),
            _cudv_retval,
            {
                GW_WARN_C(
                    "failed to launch function for collecting tracing result, "
                    "failed to call cuLaunchKernelEx: "
                    "kernel(%s), CUresult(%d), "
                    "gridDim(%d, %d, %d), blockDim(%d, %d, %d), "
                    "sharedMemBytes(%d), attrs(%p), numAttrs(%d), "
                    "param_offset(%s), param_size(%s), param_value(%s)",
                    kernel_def->mangled_prototype.c_str(), _cudv_retval,
                    cu_launch_config.gridDimX, cu_launch_config.gridDimY, cu_launch_config.gridDimZ,
                    cu_launch_config.blockDimX, cu_launch_config.blockDimY, cu_launch_config.blockDimZ,
                    cu_launch_config.sharedMemBytes, cu_launch_config.attrs, cu_launch_config.numAttrs,
                    GWUtilString::vector_to_string(_list_param_offset, false).c_str(),
                    GWUtilString::vector_to_string(_list_param_size, false).c_str(),
                    GWUtilString::vector_to_string(_list_parameter_value, true).c_str()
                );

                _trace_event->set_metadata("error", "Failed to call cuLaunchKernel to launch instrumented function for tracing");

                _retval = GW_FAILED_SDK;
                goto _exit;
            }
        );
        GW_IF_CUDA_DRIVER_FAILED(
            cuStreamSynchronize((CUstream)(kernel->stream)),
            _cudv_retval,
            {
                GW_WARN_C(
                    "failed to launch function for collecting tracing result, "
                    "failed to call cuStreamSynchronize: "
                    "kernel(%s), CUresult(%d), "
                    "gridDim(%d, %d, %d), blockDim(%d, %d, %d), "
                    "sharedMemBytes(%d), attrs(%p), numAttrs(%d), "
                    "param_offset(%s), param_size(%s), param_value(%s)",
                    kernel_def->mangled_prototype.c_str(), _cudv_retval,
                    cu_launch_config.gridDimX, cu_launch_config.gridDimY, cu_launch_config.gridDimZ,
                    cu_launch_config.blockDimX, cu_launch_config.blockDimY, cu_launch_config.blockDimZ,
                    cu_launch_config.sharedMemBytes, cu_launch_config.attrs, cu_launch_config.numAttrs,
                    GWUtilString::vector_to_string(_list_param_offset, false).c_str(),
                    GWUtilString::vector_to_string(_list_param_size, false).c_str(),
                    GWUtilString::vector_to_string(_list_parameter_value, true).c_str()
                );

                _trace_event->set_metadata("error", "Failed to call cuStreamSynchronize to synchronize stream after launching instrumented function for tracing");

                _retval = GW_FAILED_SDK;
                goto _exit;
            }
        );
        GW_DEBUG_C(
            "launched function for collecting tracing result: "
            "kernel(%s), CUresult(%d), "
            "gridDim(%d, %d, %d), blockDim(%d, %d, %d), "
            "sharedMemBytes(%d), attrs(%p), numAttrs(%d), "
            "param_offset(%s), param_size(%s), param_value(%s)",
            kernel_def->mangled_prototype.c_str(), _cudv_retval,
            cu_launch_config.gridDimX, cu_launch_config.gridDimY, cu_launch_config.gridDimZ,
            cu_launch_config.blockDimX, cu_launch_config.blockDimY, cu_launch_config.blockDimZ,
            cu_launch_config.sharedMemBytes, cu_launch_config.attrs, cu_launch_config.numAttrs,
            GWUtilString::vector_to_string(_list_param_offset, false).c_str(),
            GWUtilString::vector_to_string(_list_param_size, false).c_str(),
            GWUtilString::vector_to_string(_list_parameter_value, true).c_str()
        );

        // collect trace result
        GW_IF_FAILED(
            instrument_cxt->collect(),
            _retval,
            {
                GW_WARN_C(
                    "failed to collect trace result after tracing: "
                    "kernel(%s), error(%s)",
                    kernel_def->mangled_prototype.c_str(), gw_retval_str(_retval)
                );
                _trace_event->set_metadata("error", "Failed to collect trace result after tracing");

                goto _exit;
            }
        );

        // restore the CUDA state
        // GW_IF_FAILED(
        //     // NOTE(zhuobin):   running the instrument context for collecting trace
        //     //                  is its last run, so we pop out the checkpoint
        //     capsule->CUDA_restore_from_latest(/* do_pop */ true),
        //     _retval,
        //     {
        //         GW_WARN_C(
        //             "failed to restore CUDA state after tracing: "
        //             "kernel(%s), error(%s)",
        //             kernel_def->mangled_prototype.c_str(), gw_retval_str(_retval)
        //         );
        //         _trace_event->set_metadata("error", "Failed to restore CUDA state after tracing");

        //         goto _exit;
        //     }
        // );

    _exit:
        _trace_event->record_tick("end");
        GWCapsule::event_trace->pop_parent_event(_trace_event);
        _trace_event->archive();
        return _retval;
    };

    GW_CHECK_POINTER(capsule);
    GW_CHECK_POINTER(kernel = instrument_cxt->get_kernel());
    GW_CHECK_POINTER(kernel_def = kernel->get_def());
    GW_CHECK_POINTER(kernel_ext_cuda = GWKernelExt_CUDA::get_ext_ptr(kernel));
    GW_CHECK_POINTER(kernel_def_ext_cuda_sass = GWKernelDefExt_CUDA_SASS::get_ext_ptr(kernel_def));
    GW_CHECK_POINTER(binary_cubin = kernel_def_ext_cuda_sass->params().binary_cuda_cubin);
    GW_CHECK_POINTER(binary_ext_cuda_cubin = GWBinaryImageExt_CUDACubin::get_ext_ptr(binary_cubin));

    // load real CUDA functions
    __init_real_cuda_apis();

    // get current CPU thread index
    cpu_thread_id = (uint64_t)(pthread_self());

    // setup events
    GW_CHECK_POINTER(trace_event = new GWEvent(std::format("[Child Trace] {}", instrument_cxt->get_type())));
    trace_event->set_metadata("payload_type", "trace_single_kernel");
    trace_event->set_metadata("Trace UUID", instrument_cxt->global_id);
    trace_event->set_metadata("Trace Application", instrument_cxt->get_type());
    trace_event->type_id = GW_EVENT_TYPE_GWATCH;
    trace_event->thread_id = cpu_thread_id;
    capsule->ensure_event_trace();
    GWCapsule::event_trace->push_event(trace_event);
    GWCapsule::event_trace->push_parent_event(trace_event);
    trace_event->record_tick("begin");

    GW_CHECK_POINTER(instrument_event = new GWEvent("Instrument"));
    instrument_event->type_id = GW_EVENT_TYPE_GWATCH;
    instrument_event->thread_id = cpu_thread_id;
    capsule->ensure_event_trace();
    GWCapsule::event_trace->push_event(instrument_event);

    // step 1: checkpoint GPU memory
    // TODO(zhuobin):   I don't know why checkpointing only work here, put
    //                  before execution would cause segfault within cuda driver
    // GW_IF_FAILED(
    //     capsule->CUDA_checkpoint(cuda_state, /* do_push */ true),
    //     retval,
    //     {
    //         GW_WARN(
    //             "failed to checkpoint the state before tracing: error(%s)",
    //             gw_retval_str(retval)
    //         );
    //         goto exit;
    //     }
    // );

    // step 2: dynamic instrumentation
    instrument_event->record_tick("begin");
    GW_IF_FAILED(
        binary_ext_cuda_cubin->dynamic_instrument(instrument_cxt),
        retval,
        {
            GW_WARN_C(
                "failed to instrument kernel for tracing: kernel(%s), err(%s)",
                kernel_def->mangled_prototype.c_str(), gw_retval_str(retval)
            );
            goto exit;
        }
    );
    instrument_event->record_tick("end");
    instrument_event->archive();

    // TODO(zhuobin):
    // we shall check whether the instrumented kernel would have performance issue
    // e.g., add too much register / smem

    // step 3: load the instrumented binary
    GW_IF_CUDA_DRIVER_FAILED(
        real_cuModuleLoadData(&cu_module, instrument_cxt->instrumented_binary_bytes.data()),
        cudv_retval,
        {
            GW_WARN_C(
                "failed to load instrumented binary: kernel(%p), CUresult(%d)",
                kernel_def->mangled_prototype.c_str(), cudv_retval
            );
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

    // step 4: obtain the instrumented function for tracing
    GW_IF_CUDA_DRIVER_FAILED(
        real_cuModuleGetFunction(&cu_function_instrumented, cu_module, kernel_def->mangled_prototype.c_str()),
        cudv_retval,
        {
            GW_WARN_C(
                "failed to obtain instrumented function for tracing control flow: kernel(%s), CUresult(%d)",
                kernel_def->mangled_prototype.c_str(), cudv_retval
            );
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

    // step 5: check and set function attributes
    GW_IF_FAILED(
        __check_and_set_function_attribute(cu_function_instrumented),
        retval,
        {
            GW_WARN_C(
                "failed to check and set function attributes: kernel(%s), error(%s)",
                kernel_def->mangled_prototype.c_str(), gw_retval_str(retval)
            );
            goto exit;
        }
    );

    // step 6: form new parameter list
    for(i=0; i<kernel_def->list_param_sizes_reversed.size(); i++){
        new_parameter_list.push_back(kernel_ext_cuda->params().params[i]);
    }
    for(i=0; i<instrument_cxt->list_added_parameters.size(); i++){
        new_parameter_list.push_back(&(instrument_cxt->list_added_parameters[i]));
    }

    // step 7: running with PC sampling (if enabled)
    if(
        this->_map_metadata.find("enable_pc_sampling") != this->_map_metadata.end()
        and capsule->profile_context_cuda != nullptr
    ){
        GW_CHECK_POINTER(capsule->profile_context_cuda);

        // pc sampling on origin kernel
        list_map_pc_sampling_result.clear();
        GW_IF_FAILED(
            __execute_instrument_cxt_for_collecting_profiler_result(
                /* cu_function */ kernel_def_ext_cuda_sass->params().cu_function,
                /* param_list */ kernel_ext_cuda->params().params,
                /* profiler_mode */ GWProfiler_Mode_CUDA_PC_Sampling,
                /* repeat_times */ 256
            ),
            retval,
            {
                GW_WARN_C("failed to collect pc sampling profiler result of origin kernel");
                goto pc_sampling_exit;
            }
        );
        map_pc_sampling_result = __util_merge_pc_sampling_result(list_map_pc_sampling_result);
        instrument_cxt->set_trace_result(
            /* panel_name */ "sass_analysis",
            /* key */ "result_pc_sampling_origin",
            /* value */ map_pc_sampling_result
        );

        // pc sampling on instrumented kernel
        list_map_pc_sampling_result.clear();
        GW_IF_FAILED(
            __execute_instrument_cxt_for_collecting_profiler_result(
                /* cu_function */ cu_function_instrumented,
                /* param_list */ new_parameter_list.data(),
                /* profiler_mode */ GWProfiler_Mode_CUDA_PC_Sampling,
                /* repeat_times */ 256
            ),
            retval,
            {
                GW_WARN_C("failed to collect pc sampling profiler result of instrumented kernel");
                goto pc_sampling_exit;
            }
        );
        map_pc_sampling_result = __util_merge_pc_sampling_result(list_map_pc_sampling_result);
        instrument_cxt->set_trace_result(
            /* panel_name */ "sass_analysis",
            /* key */ "result_pc_sampling_instrument",
            /* value */ map_pc_sampling_result
        );

    pc_sampling_exit:
        ;
    }

    // GW_IF_FAILED(
    //     instrument_cxt->collect(),
    //     retval,
    //     {
    //         GW_WARN_C("failed to collect trace result");
    //         goto exit;
    //     }
    // );

    // step 8: running with range profiling (if enabled)
    // TODO(zhuobin)

    // step 9: running for tracing
    GW_IF_FAILED(
        __execute_instrument_cxt_for_collecting_trace_result(
            /* cu_function */ cu_function_instrumented,
            /* param_list */ new_parameter_list.data()
        ),
        retval,
        GW_WARN_C("failed to collect trace result");
    );

    // step 10: destory the loaded CUmodule
    // TODO(zhuobin): unload the module to save CUDA memory

exit:
    if(trace_event != nullptr){
        GWCapsule::event_trace->pop_parent_event(trace_event);
        trace_event->record_tick("end");
        trace_event->archive();
    }
    return retval;
}
