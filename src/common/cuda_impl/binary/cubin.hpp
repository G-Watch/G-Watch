#pragma once

#include <iostream>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/binary.hpp"
#include "common/instrument.hpp"
#include "common/assemble/kernel_def.hpp"


// static analysis content
enum gw_cuda_cubin_parse_content_t : uint8_t {
    GwCudaCubinParseContent_RegisterLiveness = 0,
    GwCudaCubinParseContent_RegisterOperationTrace,
    GwCudaCubinParseContent_Instruction,
    GwCudaCubinParseContent_CFG,
    GwCudaCubinParseContent_DebugInfo,
};


/*!
 *  \brief  parameters for GWBinaryImage extension of CUDA cubin
 */
struct GWBinaryImageExt_CUDACubin_Params {
    std::string arch_version = "";

    void reset() {
        this->arch_version = "";
    }
};


/*!
 *  \brief  extension of GWBinaryImage for CUDA cubin
 */
class GW_EXPORT_API GWBinaryImageExt_CUDACubin {
 public:
    /*!
     *  \brief  create a cubin binary extension
     *  \return GW_SUCCESS for successfully create cubin binary extension
     */
    static GWBinaryImageExt_CUDACubin* create();


    /*!
     *  \brief  convert a base CUDA cubin binary to GWBinaryImage extension
     *  \param  base    base CUDA cubin binary
     *  \return GW_SUCCESS for successfully convert
     */
    static GWBinaryImageExt_CUDACubin* get_ext_ptr(GWBinaryImage* base);


    /*
     *  \brief  convert this CUDA cubin binary extension to GWBinaryImage
     *  \return GW_SUCCESS for successfully convert
     */
    GWBinaryImage* get_base_ptr();
 

    /*!
     *  \brief  destructor
     */ 
    virtual ~GWBinaryImageExt_CUDACubin() = default;


    /*!
     *  \brief  get kernel definition by name
     *  \param  kernel_name     name of the kernel
     *  \param  kernel_def      kernel definition
     *  \return GW_SUCCESS for successfully get kernel definition
     */
    virtual gw_retval_t get_kerneldef_by_name(
        std::string kernel_name, GWKernelDef* &kernel_def
    ) = 0;


    /*!
     *  \brief  get kernel definition from CUBIN according to mangled prototype
     *  \param  mangled_prototype   mangled prototype of the kernel definition
     *  \param  kernel_def          parsed kernel definition
     *  \return GW_SUCCESS for successfully get
     */
    virtual gw_retval_t extract_kerneldef_from_byte_sequence(
        std::string mangled_prototype, GWKernelDef*& kernel_def
    ) = 0;


    /*!
     *  \brief  get architecture version of this cubin
     *  \param  arch_version    architecture version of this cubin
     *  \return GW_SUCCESS for successfully get
     */
    virtual gw_retval_t get_arch_version_from_byte_sequence(std::string& arch_version) = 0;


    /*!
     *  \brief  export static analysis of this cubin
     *  \param  list_target_kernel      list of target kernel to be exported
     *  \param  list_export_content     list of content to be exported
     *  \param  export_directory        directory to export
     *  \return GW_SUCCESS for successfully export
     */
    virtual gw_retval_t export_parse_result(
        std::vector<std::string> list_target_kernel,
        std::vector<gw_cuda_cubin_parse_content_t> list_export_content,
        std::string export_directory
    ) = 0;


    /*!
     *  \brief  instrument specific kernel instance with specified operator,
     *          i.e., dynamic instrumenation
     *  \param  instrument_cxt  instrument context to be used
     *  \return GW_SUCCESS for successfully instrument
     */
    virtual gw_retval_t dynamic_instrument(
        GWInstrumentCxt* instrument_cxt
    ) = 0;


    /*!
     *  \brief  obtain the map of kernels within this cubin
     *  \return map of kernels within this cubin
     */
    virtual std::map<std::string, GWKernelDef*> get_map_kernel_def() = 0;


    /*!
     *  \brief  parameter setter
     *  \return reference of parameters
     */
    virtual GWBinaryImageExt_CUDACubin_Params& params() = 0;
    
    
    /*!
     *  \brief  parameter getter
     *  \return reference of parameters
     */
    virtual const GWBinaryImageExt_CUDACubin_Params& params() const = 0;
};
