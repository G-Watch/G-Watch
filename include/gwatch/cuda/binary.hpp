#pragma once

#include <iostream>
#include <vector>
#include <map>

#include "gwatch/common.hpp"
#include "gwatch/binary.hpp"



namespace gwatch {

namespace cuda {


// forward declaration
class BinaryUtility;
class KernelDef;


/*!
 *  \brief  parsing options
 */
enum ParseOption {
    // SASS instruction details
    PARSE_OPTION_INSTRUCTION = 0,

    // control flow graph of the kernel
    PARSE_OPTION_CFG,

    // liveness of each register
    PARSE_OPTION_REGISTER_LIVENESS,
};


/*!
 *  \brief  binary image for fatbin
 */
class BinaryImage_Fatbin : public BinaryImage {
 public:
    BinaryImage_Fatbin();
    ~BinaryImage_Fatbin();

 private:
    friend class BinaryUtility;
    void *_gw_binaryimage_cuda_fatbin_handle = nullptr;
};


/*!
 *  \brief  binary image for cubin
 */
class BinaryImage_Cubin : public BinaryImage {
 public:
    BinaryImage_Cubin();
    ~BinaryImage_Cubin();


    /*!
     *  \brief  fill in the binary data
     *  \param  data        pointer to the binary data
     *  \param  size        size of the binary data, if the size if 0,
     *                      then the binary data will be parsed based
     *                      on the given binary data
     *  \return gwSuccess for successfully fill
     */
    gwError fill(const void *data, uint64_t size) override;


    /*!
     *  \brief  parse the binary image
     *  \note   this function should be called after calling fill
     *  \return gwSuccess for successfully parse
     */
    gwError parse() override;


    /*!
     *  \brief  get the kernel definition by name
     *  \param  kernel_name  the name of the kernel
     *  \param  kernel_def  the kernel definition
     *  \return the error code of the operation
     */
    gwError get_kerneldef_by_name(std::string kernel_name, KernelDef*& kernel_def);


 private:
    friend class BinaryUtility;

    // map of kernel definitions (extracted from libcgwatch)
    std::map<std::string, KernelDef*> _map_kernel_defs = {};

    // handle of the binary image for cubin
    void *_gw_binaryimage_cuda_cubin_handle = nullptr;
};


/*!
 *  \brief  binary image for PTX
 */
class BinaryImage_PTX : public BinaryImage {
 public:
    BinaryImage_PTX();
    ~BinaryImage_PTX();

 private:
    friend class BinaryUtility;
    void *_gw_binaryimage_cuda_ptx_handle = nullptr;
};


/*!
 *  \brief  binary utilities for CUDA
 */
class BinaryUtility {
 public:
    /*!
     *  \brief  parse a fatbin file
     *  \param  fatbin_file_path  the path of the fatbin file
     *  \param  dump_cubin_path  the path of the dump cubin files
     *  \return  the error code of the operation
     */
    static gwError parse_fatbin(
        std::string fatbin_file_path, BinaryImage_Fatbin& fatbin_image
    );


    /*!
     *  \brief  parse a cubin file
     *  \param  cubin_file_path  the path of the cubin file
     *  \param  cubin_image  the binary image of the cubin file
     *  \return  the error code of the operation
     */
    static gwError parse_cubin(
        std::string cubin_file_path, BinaryImage_Cubin& cubin_image
    );


    /*!
     *  \brief  parse a ptx file
     *  \param  ptx_file_path  the path of the ptx file
     *  \param  ptx_image  the binary image of the ptx file
     *  \return  the error code of the operation
     */
    static gwError parse_ptx(
        std::string ptx_file_path, BinaryImage_PTX& ptx_image
    );


 private:
    BinaryUtility() = default;
    ~BinaryUtility() = default;
};


} // namespace cuda

} // namespace gwatch
