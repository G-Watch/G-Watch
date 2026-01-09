#include <iostream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "nlohmann/json.hpp"
#include "pybind11_json/pybind11_json.hpp"

#include "common/common.hpp"
#include "common/utils/exception.hpp"
#include "common/cuda_impl/binary/cubin.hpp"
#include "common/cuda_impl/binary/fatbin.hpp"
#include "common/cuda_impl/binary/ptx.hpp"
#include "common/cuda_impl/binary/utils.hpp"
#include "common/utils/string.hpp"


void __gw_pybind_cuda_init_binary_utilities_interface(pybind11::module_ &m){
    pybind11::class_<GWBinaryUtility_CUDA> binary_utility_cuda(m, "BinaryUtility");

    binary_utility_cuda.def(pybind11::init<>());

    binary_utility_cuda.def_static(
        "demangle",
        [](std::string mangled_name) -> std::string {
            return GWUtilString::demangle(mangled_name);
        },
        "demangle cuda kernel name"
    );

    binary_utility_cuda.def_static("parse_fatbin", [](
        std::string fatbin_file_path,
        std::string dump_cubin_path
    ){
        gw_retval_t retval = GW_SUCCESS;
        uint64_t i = 0;
        GWBinaryImageExt_CUDAFatbin *fatbin_ext = nullptr;
        GWBinaryImageExt_CUDACubin *cubin_ext = nullptr;
        GWBinaryImage *fatbin = nullptr, *cubin = nullptr;
        std::string fatbin_file_stem = std::filesystem::path(fatbin_file_path).stem();

        GW_CHECK_POINTER(fatbin_ext = GWBinaryImageExt_CUDAFatbin::create());
        GW_CHECK_POINTER(fatbin = fatbin_ext->get_base_ptr());

        // fill and parse fatbin
        GW_IF_FAILED(
            fatbin->fill(fatbin_file_path),
            retval,
            {
                throw GWException("failed to fill fatbin: %s", gw_retval_str(retval));
            }
        );
        GW_IF_FAILED(
            fatbin->parse(),
            retval,
            {
                throw GWException("failed to parse fatbin: %s", gw_retval_str(retval));
            }
        );

        // dump cubin, if requested
        if (dump_cubin_path!= ""){
            for (i = 0; i < fatbin_ext->params().list_cubin.size(); i++){
                GW_CHECK_POINTER(cubin = fatbin_ext->params().list_cubin[i]);
                cubin->dump(
                    dump_cubin_path 
                    + std::string("/") 
                    + fatbin_file_stem + std::string("_")
                    + std::to_string(i) 
                    + ".cubin"
                );
            }
        }

        return fatbin_ext;
    });

    binary_utility_cuda.def_static("parse_cubin", [](
        std::string cubin_file_path,
        std::vector<std::string> list_target_kernel,
        std::vector<gw_cuda_cubin_parse_content_t> list_export_content,
        std::string export_directory
    ){
        gw_retval_t retval;
        GWBinaryImageExt_CUDACubin *cubin_ext = nullptr;
        GWBinaryImage *cubin = nullptr;

        GW_CHECK_POINTER(cubin_ext = GWBinaryImageExt_CUDACubin::create());
        GW_CHECK_POINTER(cubin = cubin_ext->get_base_ptr());

        // fill and parse cubin
        GW_IF_FAILED(
            cubin->fill(cubin_file_path),
            retval,
            {
                throw GWException("failed to parse cubin: %s", gw_retval_str(retval));
            }
        );
        GW_IF_FAILED(
            cubin->parse(),
            retval,
            {
                throw GWException("failed to parse cubin: %s", gw_retval_str(retval));
            }
        );

        // export parse result
        GW_IF_FAILED(
            cubin_ext->export_parse_result(list_target_kernel, list_export_content, export_directory),
            retval,
            {
                throw GWException("failed to export parse result of cubin: %s", gw_retval_str(retval));
            }
        );

        return cubin_ext;
    });

    binary_utility_cuda.def_static("parse_ptx", [](
        std::string ptx_file_path,
        std::vector<std::string> list_target_kernel,
        std::string export_directory
    ){
        gw_retval_t retval;
        GWBinaryImageExt_CUDAPTX *ptx_ext = nullptr;
        GWBinaryImage *ptx = nullptr;

        GW_CHECK_POINTER(ptx_ext = GWBinaryImageExt_CUDAPTX::create());
        GW_CHECK_POINTER(ptx = ptx_ext->get_base_ptr());

        // fill and parse ptx
        GW_IF_FAILED(
            ptx->fill(ptx_file_path),
            retval,
            {
                throw GWException("failed to fill ptx: %s", gw_retval_str(retval));
            }
        );
        GW_IF_FAILED(
            ptx->parse(),
            retval,
            {
                throw GWException("failed to parse ptx: %s", gw_retval_str(retval));
            }
        );

        // export parse result
        // TODO:

        return ptx;
    });
}
