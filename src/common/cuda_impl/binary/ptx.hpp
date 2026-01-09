#pragma once

#include <iostream>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/binary.hpp"
#include "common/instrument.hpp"
#include "common/assemble/kernel_def.hpp"


/*!
 *  \brief  parameters for GWBinaryImage extension of CUDA PTX
 */
struct GWBinaryImageExt_CUDAPTX_Params {
    // PTX version, e.g., .version 8.7
    std::string version;
    
    // PTX backend target, e.g., .target sm_90
    std::string target = "";

    // PTX address length, e.g., .address_size 64
    uint32_t address_len = 0;

    // names of kernels included, e.g., .visible .entry _Z8kernel_1Pi(.param .u64 _Z8kernel_1Pi_param_0)
    std::vector<std::string> list_kernel_name = {};
    std::map<std::string, std::vector<std::string>> map_kernel_ptx_line = {};

    // map of kernels conatins in this ptx
    std::map<std::string, GWKernelDef*> map_kernel_def_ptx;

    void reset() {
        this->version.clear();
        this->target.clear();
        this->address_len = 0;
        this->list_kernel_name.clear();
        this->map_kernel_ptx_line.clear();
        this->map_kernel_def_ptx.clear();
    }
};


/*!
 *  \brief  extension of GWBinaryImage for CUDA PTX
 */
class GW_EXPORT_API GWBinaryImageExt_CUDAPTX {
 public:
    /*!
     *  \brief  create a ptx binary image
     *  \return GW_SUCCESS for successfully create ptx binary image
     */
    static GWBinaryImageExt_CUDAPTX* create();


    /*!
     *  \brief  convert a base CUDA fatbin binary to GWBinaryImage extension
     *  \param  base    base CUDA fatbin binary
     *  \return GW_SUCCESS for successfully convert
     */
    static GWBinaryImageExt_CUDAPTX* get_ext_ptr(GWBinaryImage* base);


    /*
     *  \brief  convert this CUDA PTX binary extension to GWBinaryImage
     *  \return GW_SUCCESS for successfully convert
     */
    GWBinaryImage* get_base_ptr();


    /*!
     *  \brief  destructor
     */ 
    virtual ~GWBinaryImageExt_CUDAPTX() = default;


     /*!
      *  \brief  parameter setter
      *  \return reference of parameters
      */
    virtual GWBinaryImageExt_CUDAPTX_Params& params() = 0;


     /*!
      *  \brief  parameter getter
      *  \return reference of parameters
      */
    virtual const GWBinaryImageExt_CUDAPTX_Params& params() const = 0;
};
