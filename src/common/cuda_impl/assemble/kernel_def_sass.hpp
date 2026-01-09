#pragma once

#include <iostream>

#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/binary.hpp"
#include "common/instrument.hpp"
#include "common/assemble/kernel_def.hpp"


/*!
 *  \brief  parameters for GWKernelDef extension of CUDA SASS
 */
struct GWKernelDefExt_CUDA_SASS_Params {
    // CUfunction handle this kernel def points to
    CUfunction cu_function = (CUfunction)0;

    // binary image of this kernel definition
    GWBinaryImage *binary_cuda_cubin = nullptr;

    // architecture generation of this kernel
    std::string arch_version = "";

    void reset() {
        this->cu_function = (CUfunction)0;
        this->binary_cuda_cubin = nullptr;
        this->arch_version = "";
    }
};


/*!
 *  \brief  extension of GWBinaryImage for CUDA cubin
 */
class GW_EXPORT_API GWKernelDefExt_CUDA_SASS {
 public:
    /*!
     *  \brief  destructor
     */ 
    virtual ~GWKernelDefExt_CUDA_SASS() = default;


    /*!
     *  \brief  convert a base CUDA kerneldef to GWKernelDefExt_CUDA_SASS extension
     *  \param  base    base CUDA kerneldef
     *  \return GW_SUCCESS for successfully convert
     */
    static GWKernelDefExt_CUDA_SASS* get_ext_ptr(GWKernelDef* base);

    /*
     *  \brief  convert this CUDA kerneldef extension to GWKernelDef
     *  \return GW_SUCCESS for successfully convert
     */
    static GWKernelDef* get_base_ptr(GWKernelDefExt_CUDA_SASS* kernel_def_ext);


    /*!
     *  \brief  parameter setter
     *  \return reference of parameters
     */
    virtual GWKernelDefExt_CUDA_SASS_Params& params() = 0;
    

    /*!
     *  \brief  parameter getter
     *  \return reference of parameters
     */
    virtual const GWKernelDefExt_CUDA_SASS_Params& params() const = 0;
};
