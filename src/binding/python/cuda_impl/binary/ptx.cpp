#include <iostream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "nlohmann/json.hpp"
#include "pybind11_json/pybind11_json.hpp"

#include "common/common.hpp"
#include "common/utils/exception.hpp"
#include "common/cuda_impl/binary/ptx.hpp"


void __gw_pybind_cuda_init_ptx_interface(pybind11::module_ &m){
    pybind11::class_<GWBinaryImageExt_CUDAPTX_Params>(m, "PTXParams")
        .def(py::init<>())
        .def_readwrite("version", &GWBinaryImageExt_CUDAPTX_Params::version)
        .def_readwrite("target", &GWBinaryImageExt_CUDAPTX_Params::target)
        .def_readwrite("address_len", &GWBinaryImageExt_CUDAPTX_Params::address_len)
        .def_readwrite("list_kernel_name", &GWBinaryImageExt_CUDAPTX_Params::list_kernel_name)
        .def_readwrite("map_kernel_ptx_line", &GWBinaryImageExt_CUDAPTX_Params::map_kernel_ptx_line)
        .def_readwrite("map_kernel_def_ptx", &GWBinaryImageExt_CUDAPTX_Params::map_kernel_def_ptx)
        .def("reset", &GWBinaryImageExt_CUDAPTX_Params::reset);

    pybind11::class_<GWBinaryImageExt_CUDAPTX> _class(m, "PTX");

    _class.def(py::init([]() {
        return GWBinaryImageExt_CUDAPTX::create();
    }));

    _class.def_property_readonly(
        "params",
        [](GWBinaryImageExt_CUDAPTX& self) -> GWBinaryImageExt_CUDAPTX_Params& {
            return self.params();
        },
        py::return_value_policy::reference_internal,
        "get parameters of this ptx"
    );

    _class.def(
        "fill",
        [](GWBinaryImageExt_CUDAPTX& self, std::string file_path) {
            gw_retval_t tmp_retval = GW_SUCCESS;
            GWBinaryImage* binary_image = nullptr;
            GW_CHECK_POINTER(binary_image = self.get_base_ptr());
            GW_IF_FAILED(
                binary_image->fill(file_path),
                tmp_retval,
                {
                    throw GWException("failed to fill ptx: file_path(%s)", file_path.c_str());
                }
            );
        },
        py::arg("file_path"),
        "fill this ptx with the binary data from a file"
    );

    _class.def(
        "parse",
        [](GWBinaryImageExt_CUDAPTX& self) {
            gw_retval_t tmp_retval = GW_SUCCESS;
            GWBinaryImage* binary_image = nullptr;
            GW_CHECK_POINTER(binary_image = self.get_base_ptr());
            GW_IF_FAILED(binary_image->parse(), tmp_retval, {
                throw GWException("failed to parse ptx");
            });
        },
        "parse this ptx"
    );
}
