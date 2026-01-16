#include <iostream>
#include <string>
#include <vector>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"
#include "common/assemble/kernel.hpp"
#include "common/assemble/kernel_def.hpp"
#include "capsule/capsule.hpp"
#include "capsule/event.hpp"
#include "binding/runtime_control.hpp"


void gw_rt_control_start_tracing_kernel_launch(){
    gw_retval_t retval = GW_SUCCESS;
    static bool is_hijacklib_loaded = __init_capsule();

    if(is_hijacklib_loaded){
        GW_CHECK_POINTER(capsule != nullptr);
        capsule->start_tracing_kernel_launch();
    }
}


std::vector<GWKernel*> gw_rt_control_stop_tracing_kernel_launch(){
    std::vector<GWKernel*> list_kernel = {};
    gw_retval_t retval = GW_SUCCESS;
    static bool is_hijacklib_loaded = __init_capsule();

    if(is_hijacklib_loaded){
        GW_CHECK_POINTER(capsule != nullptr);
        GW_IF_FAILED(
            capsule->stop_tracing_kernel_launch(list_kernel),
            retval,
            {
                throw GWException("failed to stop tracing kernel launch");
            }
        );
    }

    return list_kernel;
}


gw_retval_t gw_rt_control_get_kernel_def(std::string name, GWKernelDef* &kernel_def){
    gw_retval_t retval = GW_SUCCESS;
    static bool is_hijacklib_loaded = __init_capsule();

    if(is_hijacklib_loaded){
        GW_CHECK_POINTER(capsule);

        #if GW_BACKEND_CUDA
            GW_IF_FAILED(
                capsule->CUDA_get_kerneldef_by_name(name, kernel_def),
                retval,
                {
                    throw GWException("failed to get kernel definition");
                }
            );
        #else
            GW_WARN("failed to get kernel definition: backend not supported");
            retval = GW_FAILED_NOT_IMPLEMENTAED;
        #endif
    }

 exit:
    return retval;
}


gw_retval_t gw_rt_control_get_map_kerneldef(std::map<std::string, GWKernelDef*>& map_name_kerneldef){
    gw_retval_t retval = GW_SUCCESS;
    static bool is_hijacklib_loaded = __init_capsule();

    if(is_hijacklib_loaded){
        GW_CHECK_POINTER(capsule);

        #if GW_BACKEND_CUDA
            GW_IF_FAILED(
                capsule->CUDA_get_map_kerneldef(map_name_kerneldef),
                retval,
                {
                    throw GWException("failed to get map of kernel definitions");
                }
            );
        #else
            GW_WARN("failed to get map of kernel definitions: backend not supported");
            retval = GW_FAILED_NOT_IMPLEMENTAED;
        #endif
    }

 exit:
    return retval;
}
