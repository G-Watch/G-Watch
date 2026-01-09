#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <cuda.h>
#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/binary.hpp"


// forward declaration
class GWBinaryImage_CUDACubin;
class GWBinaryImage_CUDAPTX;
class GWBinaryImage_CUDAFatbin;


class GW_EXPORT_API GWBinaryUtility_CUDA {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     *  \param  instruction_metadata_dir    directory of the instruction metadata
     */
    GWBinaryUtility_CUDA();


    /*!
     *  \brief  destructor
     */
    ~GWBinaryUtility_CUDA();


    /*!
     *  \brief  CUDA binary type
     */
    enum gw_cuda_binary_t : uint8_t {
        GW_CUDA_BINARY_UNKNOWN = 0,
        GW_CUDA_BINARY_FATBIN,
        GW_CUDA_BINARY_CUBIN,
        GW_CUDA_BINARY_PTX
    };


    /*!
     *  \brief  cast arch version to CUDA JIT target
     *  \param  arch_version    architecture version, e.g., 90, 90a
     *  \return CUDA JIT target
     */
    static CUjit_target arch_str_to_jit_target(std::string arch_version);


    /*!
     *  \brief  compare two arch version
     *  \param  arch_version_1          first architecture version  e.g., 90, 90a
     *  \param  arch_version_2          second architecture version e.g., 90, 90a
     *  \param  ignore_variant_suffix   whether ignore the variant suffix, e.g., 90, 90a
     *  \return whether two arch version are equal / greater / less
     */
    static bool is_arch_equal(std::string arch_version_1, std::string arch_version_2, bool ignore_variant_suffix=true);
    static bool is_arch_greater(std::string arch_version_1, std::string arch_version_2);
    static bool is_arch_greater_or_equal(std::string arch_version_1, std::string arch_version_2, bool ignore_variant_suffix=true);
    static bool is_arch_less(std::string arch_version_1, std::string arch_version_2);
    static bool is_arch_less_or_equal(std::string arch_version_1, std::string arch_version_2, bool ignore_variant_suffix=true);


    /*!
     *  \brief  obtain the type of CUDA binary
     *  \param  data    byte sequence of the CUDA binary
     *  \param  size    size of the byte sequence
     *  \param  type    the returned type (if succesfully parsed)
     *  \return GW_SUCCESS for successfully parse
     */
    static gw_retval_t get_binary_type(const uint8_t* data, uint64_t size, gw_cuda_binary_t& type);
    /* ==================== Common ==================== */
};
