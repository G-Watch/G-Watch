#include <iostream>
#include <string>

#include <pthread.h>
#include <stdint.h>
#include <dlfcn.h>

#include "nlohmann/json.hpp"

#include "common/common.hpp"
#include "common/binary.hpp"
#include "common/cuda_impl/binary/cubin.hpp"
#include "common/cuda_impl/assemble/kernel_def_sass.hpp"


// public header
#include "gwatch/cuda/binary.hpp"
#include "gwatch/cuda/kernel.hpp"


namespace gwatch {

namespace cuda {


BinaryImage_Fatbin::BinaryImage_Fatbin()
{}


BinaryImage_Fatbin::~BinaryImage_Fatbin()
{}


BinaryImage_Cubin::BinaryImage_Cubin(){
    this->_gw_binaryimage_cuda_cubin_handle = GWBinaryImageExt_CUDACubin::create();
}


BinaryImage_Cubin::~BinaryImage_Cubin(){
    GWBinaryImage *kerneldef_ext_cuda_sass = nullptr;

    kerneldef_ext_cuda_sass = static_cast<GWBinaryImage*>(this->_gw_binaryimage_cuda_cubin_handle);
    GW_ASSERT(kerneldef_ext_cuda_sass != nullptr);

    delete kerneldef_ext_cuda_sass;
}


gwError BinaryImage_Cubin::fill(const void *data, uint64_t size){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWBinaryImage *binimage = nullptr;
    GWBinaryImageExt_CUDACubin *cubin_ext = nullptr;

    // type casting
    GW_ASSERT(this->_gw_binaryimage_cuda_cubin_handle != nullptr);
    cubin_ext = static_cast<GWBinaryImageExt_CUDACubin*>(this->_gw_binaryimage_cuda_cubin_handle);
    GW_CHECK_POINTER(cubin_ext);
    binimage = cubin_ext->get_base_ptr();
    GW_CHECK_POINTER(binimage);

    GW_IF_FAILED(
        binimage->fill(data, size),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    );

exit:
    return retval;
}


gwError BinaryImage_Cubin::parse(){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWBinaryImage *binimage = nullptr;
    GWBinaryImageExt_CUDACubin *cubin_ext = nullptr;

    // type casting
    GW_ASSERT(this->_gw_binaryimage_cuda_cubin_handle != nullptr);
    cubin_ext = static_cast<GWBinaryImageExt_CUDACubin*>(this->_gw_binaryimage_cuda_cubin_handle);
    GW_CHECK_POINTER(cubin_ext);
    binimage = cubin_ext->get_base_ptr();
    GW_CHECK_POINTER(binimage);

    GW_IF_FAILED(
        binimage->parse(),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    );

exit:
    return retval;
}


   
gwError BinaryImage_Cubin::get_kerneldef_by_name(std::string kernel_name, KernelDef*& kernel_def){
    gwError retval = gwSuccess;
    gw_retval_t gw_retval = GW_SUCCESS;
    GWKernelDef* _gw_kernel_def = nullptr;
    GWBinaryImageExt_CUDACubin *cubin_ext = nullptr;

    // check if the kernel definition is already obtained
    if (this->_map_kernel_defs.find(kernel_name) != this->_map_kernel_defs.end()) {
        kernel_def = this->_map_kernel_defs[kernel_name];
        goto exit;
    }

    // type casting
    cubin_ext = static_cast<GWBinaryImageExt_CUDACubin*>(this->_gw_binaryimage_cuda_cubin_handle);
    GW_CHECK_POINTER(cubin_ext);

    // obtain the kernel definition
    GW_IF_FAILED(
        cubin_ext->get_kerneldef_by_name(kernel_name, _gw_kernel_def),
        gw_retval,
        {
            retval = gwFailed;
            goto exit;
        }
    );
    GW_CHECK_POINTER(_gw_kernel_def);

    // create the kernel definition
    GW_CHECK_POINTER(kernel_def = new KernelDef(_gw_kernel_def));
    this->_map_kernel_defs[kernel_name] = kernel_def;

 exit:
    return retval;
}


BinaryImage_PTX::BinaryImage_PTX()
{}


BinaryImage_PTX::~BinaryImage_PTX()
{}


gwError BinaryUtility::parse_fatbin(
    std::string fatbin_file_path, BinaryImage_Fatbin& fatbin_image
){
    gwError retval = gwSuccess;

exit:
    return retval;
}


gwError BinaryUtility::parse_cubin(
    std::string cubin_file_path, BinaryImage_Cubin& cubin_image
){
    gwError retval = gwSuccess;

exit:
    return retval;
}


gwError BinaryUtility::parse_ptx(
    std::string ptx_file_path, BinaryImage_PTX& ptx_image
){
    gwError retval = gwSuccess;

exit:
    return retval;
}

    
} // namespace cuda

} // namespace gwatch
