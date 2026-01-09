#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "capsule/cuda_impl/hijack/runtime.hpp"
#include "capsule/cuda_impl/hijack/real_apis.hpp"


CUresult cuLibraryGetKernel(
    CUkernel* pKernel, CUlibrary library, const char* name
){
    gw_retval_t gw_retval = GW_SUCCESS;
    CUresult cudv_retval;
    CUmodule linked_module = (CUmodule)0;
    CUfunction linked_function = (CUfunction)0;

    GW_CHECK_POINTER(capsule);

    // obtain the real cuda apis
    __init_real_cuda_apis();

    /* ================ call actual function ================ */
    cudv_retval = real_cuLibraryGetKernel(pKernel, library, name);
    GW_DEBUG("called cuLibraryGetKernel");
    /* ================ call actual function ================ */


    /* ================ parse the CUfunction (lazyly) ================ */
    if(likely(cudv_retval == CUDA_SUCCESS)){
        GW_IF_CUDA_DRIVER_FAILED(
            real_cuLibraryGetModule(&linked_module, library),
            cudv_retval,
            {
                GW_WARN_DETAIL("failed to get module from library: %s", gw_retval_str(gw_retval));
                goto exit;
            }
        );

        GW_IF_CUDA_DRIVER_FAILED(
            real_cuKernelGetFunction(&linked_function, *pKernel),
            cudv_retval,
            {
                GW_WARN_DETAIL("failed to get function from kernel: %s", gw_retval_str(gw_retval));
                goto exit;
            }
        );

        GW_IF_FAILED(
            capsule->CUDA_record_mapping_cumodule_culibrary(linked_module, library),
            gw_retval,
            {
                GW_WARN_DETAIL("failed to record mapping cumodule and culibrary: %s", gw_retval_str(gw_retval));
                goto exit;
            }
        );

        GW_IF_FAILED(
            capsule->CUDA_record_mapping_cufunction_cumodule(linked_function, linked_module),
            gw_retval,
            {
                GW_WARN_DETAIL("failed to record mapping cufunction and cumodule: %s", gw_retval_str(gw_retval));
                goto exit;
            }
        );

        GW_IF_FAILED(
            capsule->CUDA_parse_cufunction(linked_function, linked_module, /* do_parse_entire_binary */ false),
            gw_retval,
            {
                GW_WARN_DETAIL("failed to parse cufunction: %s", gw_retval_str(gw_retval));
                goto exit;
            }
        );
    }
    /* ================ parse the CUfunction (lazyly) ================ */

    // if operation isn't success, we sync all previous messages to scheduler to be finished
    if(cudv_retval != CUDA_SUCCESS){
        capsule->sync_send_to_scheduler();
    }

exit:
    return cudv_retval;
}
