#pragma once

#include <iostream>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/binary.hpp"
#include "common/instrument.hpp"
#include "common/assemble/kernel_def.hpp"


/*!
 *  \brief  parameters for GWBinaryImage extension of CUDA fatbin
 */
struct GWBinaryImageExt_CUDAFatbin_Params {
    std::vector<GWBinaryImage*> list_cubin;
    std::vector<GWBinaryImage*> list_ptx;

    void reset() {
        this->list_cubin.clear();
        this->list_ptx.clear();
    }
};


/*!
 *  \brief  extension of GWBinaryImage for CUDA fatbin
 */
class GW_EXPORT_API GWBinaryImageExt_CUDAFatbin {
 public:
    /*!
     *  \brief  create a fatbin binary image
     *  \return GW_SUCCESS for successfully create fatbin binary image
     */
    static GWBinaryImageExt_CUDAFatbin* create();


    /*!
     *  \brief  convert a base CUDA fatbin binary to GWBinaryImage extension
     *  \param  base    base CUDA fatbin binary
     *  \return GW_SUCCESS for successfully convert
     */
    static GWBinaryImageExt_CUDAFatbin* get_ext_ptr(GWBinaryImage* base);


    /*
     *  \brief  convert this CUDA fatbin binary extension to GWBinaryImage
     *  \return GW_SUCCESS for successfully convert
     */
    GWBinaryImage* get_base_ptr();


    /*!
     *  \brief  destructor
     */ 
    virtual ~GWBinaryImageExt_CUDAFatbin() = default;


    /*!
     *  \brief  parameter setter
     *  \return reference of parameters
     */
    virtual GWBinaryImageExt_CUDAFatbin_Params& params() = 0;
    
    
    /*!
     *  \brief  parameter getter
     *  \return reference of parameters
     */
    virtual const GWBinaryImageExt_CUDAFatbin_Params& params() const = 0;


    /*!
     *  \brief  extract fatbinary from raw byte sequence
     *  \param  data    pointer to the CUDA binary
     *  \param  bytes   extracted fatbinary
     *  \return GW_SUCCESS for successfully extract
     */
    static gw_retval_t extract_from_byte_sequence(const void *data, std::vector<uint8_t>& bytes);
};
