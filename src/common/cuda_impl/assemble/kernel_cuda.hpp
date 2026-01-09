#pragma once

#include <iostream>

#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/binary.hpp"
#include "common/instrument.hpp"
#include "common/assemble/kernel.hpp"
#include "common/assemble/kernel_def.hpp"


/*!
 *  \brief  parameters for GWKernelDef extension of CUDA SASS
 */
struct GWKernelExt_CUDA_Params {
    // launch parameters
    // NOTE(zhuobin): the lifecycle isn't ensured!
    void **params = nullptr;
    void **extra = nullptr;
    CUlaunchAttribute* attrs = nullptr;
    uint32_t num_attrs = 0;

    void reset() {
        this->params = nullptr;
        this->extra = nullptr;
        this->attrs = nullptr;
        this->num_attrs = 0;
    }
};


/*!
 *  \brief  extension of GWBinaryImage for CUDA cubin
 */
class GW_EXPORT_API GWKernelExt_CUDA {
 public:
    /*!
     *  \brief  create a kernel extension
     *  \return GW_SUCCESS for successfully create kernel extension
     */
    static GWKernelExt_CUDA* create(GWKernelDef* kernel_def);


    /*!
     *  \brief  convert a base CUDA kernel to GWKernel extension
     *  \param  base    base CUDA kernel
     *  \return GW_SUCCESS for successfully convert
     */
    static GWKernelExt_CUDA* get_ext_ptr(GWKernel* base);


    /*
     *  \brief  convert this CUDA kernel extension to GWKernel
     *  \return GW_SUCCESS for successfully convert
     */
    GWKernel* get_base_ptr();


    /*!
     *  \brief  destructor
     */ 
    virtual ~GWKernelExt_CUDA() = default;


    /*!
     *  \brief  parameter setter
     *  \return reference of parameters
     */
    virtual GWKernelExt_CUDA_Params& params() = 0;
    

    /*!
     *  \brief  parameter getter
     *  \return reference of parameters
     */
    virtual const GWKernelExt_CUDA_Params& params() const = 0;
};
