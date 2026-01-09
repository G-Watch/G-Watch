#pragma once

#include <iostream>

#include <nlohmann/json.hpp>

#include <cuda.h>
#include <cuda_runtime.h>

#include "common/common.hpp"
#include "common/log.hpp"


typedef struct gw_cuda_function_attribute {
    // function properties
    // CUfunction function;
    uint64_t function;
    std::string demangled_name;
    int ptx_version = 0;
    int sass_version = 0;
    int max_thread_per_block = 0;
    int static_smem_size = 0;
    int max_dynamic_smem_size = 0;
    int const_mem_size = 0;
    int local_mem_size = 0;
    int num_reg = 0;

    // mark whether this function has been instrumented
    bool has_instrumented = false;

    gw_cuda_function_attribute(
        CUfunction function_,
        std::string demangled_name_,
        int ptx_version_,
        int sass_version_,
        int max_thread_per_block_,
        int static_smem_size_,
        int max_dynamic_smem_size_,
        int const_mem_size_,
        int local_mem_size_,
        int num_reg_
    ){
        this->function = (uint64_t)(function_);
        this->demangled_name = demangled_name_;
        this->ptx_version = ptx_version_;
        this->sass_version = sass_version_;
        this->max_thread_per_block = max_thread_per_block_;
        this->static_smem_size = static_smem_size_;
        this->max_dynamic_smem_size = max_dynamic_smem_size_;
        this->const_mem_size = const_mem_size_;
        this->local_mem_size = local_mem_size_;
        this->num_reg = num_reg_;
    }

    gw_cuda_function_attribute(){}
    ~gw_cuda_function_attribute(){}
} gw_cuda_function_attribute_t;
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    gw_cuda_function_attribute_t, 
    function, demangled_name, ptx_version, sass_version, max_thread_per_block, static_smem_size,
    max_dynamic_smem_size, const_mem_size, local_mem_size, num_reg
);
