#include <iostream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "nlohmann/json.hpp"
#include "pybind11_json/pybind11_json.hpp"

#include "common/common.hpp"
#include "common/utils/exception.hpp"
#include "common/cuda_impl/binary/cubin.hpp"
#include "common/cuda_impl/assemble/kernel_def_sass.hpp"


void __gw_pybind_cuda_init_cubin_interface(pybind11::module_ &m){
    pybind11::class_<GWBinaryImageExt_CUDACubin_Params>(m, "CubinParams")
        .def(py::init<>())
        .def_readwrite("arch_version", &GWBinaryImageExt_CUDACubin_Params::arch_version)
        .def("reset", &GWBinaryImageExt_CUDACubin_Params::reset);

    pybind11::class_<GWBinaryImageExt_CUDACubin> cubin(m, "Cubin");

    cubin.def(py::init([]() {
        return GWBinaryImageExt_CUDACubin::create();
    }));

    cubin.def_property_readonly(
        "params",
        [](GWBinaryImageExt_CUDACubin& self) -> GWBinaryImageExt_CUDACubin_Params& {
            return self.params();
        },
        py::return_value_policy::reference_internal,
        "get parameters of this cubin"
    );

    cubin.def(
        "fill",
        [](GWBinaryImageExt_CUDACubin& self, std::string file_path) {
            gw_retval_t tmp_retval = GW_SUCCESS;
            GWBinaryImage* binary_image = nullptr;
            GW_CHECK_POINTER(binary_image = self.get_base_ptr());
            GW_IF_FAILED(
                binary_image->fill(file_path),
                tmp_retval,
                {
                    throw GWException("failed to fill cubin: file_path(%s)", file_path.c_str());
                }
            );
        },
        py::arg("file_path"),
        "fill this cubin with the binary data from a file"
    );

    cubin.def(
        "parse",
        [](GWBinaryImageExt_CUDACubin& self) {
            gw_retval_t tmp_retval = GW_SUCCESS;
            GWBinaryImage* binary_image = nullptr;
            GW_CHECK_POINTER(binary_image = self.get_base_ptr());
            GW_IF_FAILED(binary_image->parse(), tmp_retval, {
                throw GWException("failed to parse cubin");
            });
        },
        "parse this cubin"
    );

    cubin.def(
        "get_kerneldef_by_name",
        [](GWBinaryImageExt_CUDACubin* self, std::string kernel_name) -> GWKernelDefExt_CUDA_SASS* {
            gw_retval_t tmp_retval = GW_SUCCESS;
            GWKernelDefExt_CUDA_SASS *kernel_def_ext_sass = nullptr;
            GWKernelDef* kernel_def = nullptr;

            GW_IF_FAILED(self->get_kerneldef_by_name(kernel_name, kernel_def), tmp_retval, {
                throw GWException("failed to get kernel definition by name: kernel_name(%s)", kernel_name.c_str());
            });
            GW_CHECK_POINTER(kernel_def_ext_sass = dynamic_cast<GWKernelDefExt_CUDA_SASS*>(kernel_def));
            return kernel_def_ext_sass;
        },
        py::arg("kernel_name"),
        py::return_value_policy::reference_internal,
        "get kernel definition by name"
    );

    cubin.def(
        "get_map_kernel_def",
        [](GWBinaryImageExt_CUDACubin& self) -> std::map<std::string, GWKernelDefExt_CUDA_SASS*> {
            gw_retval_t tmp_retval = GW_SUCCESS;
            GWKernelDefExt_CUDA_SASS* kernel_def_ext_sass = nullptr;
            std::map<std::string, GWKernelDef*> map_kernel_def;
            std::map<std::string, GWKernelDefExt_CUDA_SASS*> map_kernel_def_ext_sass;
            map_kernel_def = self.get_map_kernel_def();
            for(auto& [kernel_name, kernel_def] : map_kernel_def){
                GW_CHECK_POINTER(kernel_def_ext_sass = GWKernelDefExt_CUDA_SASS::get_ext_ptr(kernel_def));
                map_kernel_def_ext_sass[kernel_name] = kernel_def_ext_sass;
            }
            return map_kernel_def_ext_sass;
        },
        "get map of kernels within this cubin"
    );

    cubin.def(
        "export_parse_result", 
        &GWBinaryImageExt_CUDACubin::export_parse_result,
        py::arg("list_target_kernel"),
        py::arg("list_export_content"),
        py::arg("export_directory"),
        "export static analysis of this cubin"
    );
}
