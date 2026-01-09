#pragma once
#if GW_BACKEND_CUDA

#include <iostream>
#include <map>

#include <cuda.h>
#include <cuda_runtime.h>

#include "common/common.hpp"
#include "common/log.hpp"


/*!
 *  \brief  represent a behaviour tree
 */
class GWUtilCUDA {
 public:
    /*!
     *  \brief  constructor
     */
    GWUtilCUDA(){}


    /*!
     *  \brief  destructor
     */
    ~GWUtilCUDA(){}


    /* ========== Context Management ========== */
    static gw_retval_t get_current_cucontext(CUcontext& cu_context){
        gw_retval_t retval = GW_SUCCESS;
        CUresult cudv_retval = CUDA_SUCCESS;

        GW_IF_CUDA_DRIVER_FAILED(
            cuCtxGetCurrent(&cu_context),
            cudv_retval,
            {
                GW_WARN(
                    "failed to obtain current CUcontext: retval(%d)",
                    cudv_retval
                );
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );

    exit:
        return retval;
    }


    static gw_retval_t get_arch_of_current_context(std::string& arch){
        gw_retval_t retval = GW_SUCCESS;
        CUresult cudv_retval = CUDA_SUCCESS;
        CUdevice cu_device;
        int arch_major = 0, arch_minor = 0;

        GW_IF_CUDA_DRIVER_FAILED(
            cuCtxGetDevice(&cu_device),
            cudv_retval,
            {
                GW_WARN("failed to obtain CUdevice of current context");
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );

        GW_IF_CUDA_DRIVER_FAILED(
            cuDeviceGetAttribute(&arch_major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cu_device),
            cudv_retval,
            {
                GW_WARN("failed to obtain major architecture of CUdevice of current context");
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );

        GW_IF_CUDA_DRIVER_FAILED(
            cuDeviceGetAttribute(&arch_minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cu_device),
            cudv_retval,
            {
                GW_WARN("failed to obtain minor architecture of CUdevice of current context");
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );

        // arch = static_cast<uint32_t>(arch_major * 10 + arch_minor);
        arch = std::to_string(arch_major) + std::to_string(arch_minor);

    exit:
        return retval;
    }
    /* ========== Context Management ========== */

    /* ========== Device Management ========== */
    static gw_retval_t get_current_device_id(int& id){
        gw_retval_t retval = GW_SUCCESS;
        CUresult cudv_retval = CUDA_SUCCESS;
        CUdevice cu_device;

        GW_IF_CUDA_DRIVER_FAILED(
            cuCtxGetDevice(&cu_device),
            cudv_retval,
            {
                GW_WARN("failed to obtain CUdevice of current context");
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );

        id = (int)(cu_device);

    exit:
        return retval;
    }


    static gw_retval_t get_current_device(CUdevice& cu_device){
        gw_retval_t retval = GW_SUCCESS;
        CUresult cudv_retval = CUDA_SUCCESS;

        GW_IF_CUDA_DRIVER_FAILED(
            cuCtxGetDevice(&cu_device),
            cudv_retval,
            {
                GW_WARN("failed to obtain CUdevice of current context");
                retval = GW_FAILED_SDK;
                goto exit;
            }
        );

    exit:
        return retval;
    }
    /* ========== Device Management ========== */

    /* ========== Error Handling ========== */
    static std::string get_driver_error_string(CUresult error){
        std::string error_string = "Unknown error (failed to get error string from CUDA driver)";
        CUresult cudv_retval = CUDA_SUCCESS;
        const char* str_ptr = nullptr;

        GW_IF_CUDA_DRIVER_FAILED(
            cuGetErrorString(error, &str_ptr),
            cudv_retval,
            {
                GW_WARN("failed to obtain error string of CUresult");
                goto exit;
            }
        );

        error_string = str_ptr;

    exit:
        return error_string;
    }
    /* ========== Error Handling ========== */


    /* ========== Function Management ========== */
    static gw_retval_t get_func_param_info(
        CUfunction func, std::vector<uint64_t>& list_offset, std::vector<uint64_t>& list_size, uint64_t& nb_params
    ){
        gw_retval_t retval = GW_SUCCESS;
        CUresult cudv_retval = CUDA_SUCCESS;
        int i = 0;
        size_t offset = 0, size = 0;

        list_offset.clear();
        list_size.clear();        
        nb_params = 0;

        while(true){
            cudv_retval = cuFuncGetParamInfo(func, i, &offset, &size);
            if(cudv_retval == CUDA_ERROR_INVALID_VALUE){
                break;
            }
            list_offset.push_back(static_cast<uint64_t>(offset));
            list_size.push_back(static_cast<uint64_t>(size));
            i++;
        }
        nb_params = i;

    exit:
        return retval;
    }
    /* ========== Function Management ========== */
};

#endif // GW_BACKEND_CUDA
