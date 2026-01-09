#include <iostream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "nlohmann/json.hpp"
#include "pybind11_json/pybind11_json.hpp"

#include "common/common.hpp"
#include "common/utils/exception.hpp"
#include "common/cuda_impl/binary/fatbin.hpp"


void __gw_pybind_cuda_init_fatbin_interface(pybind11::module_ &m){
    pybind11::class_<GWBinaryImageExt_CUDAFatbin_Params>(m, "FatbinParams")
        .def(py::init<>())
        .def_readwrite("list_cubin", &GWBinaryImageExt_CUDAFatbin_Params::list_cubin)
        .def_readwrite("list_ptx", &GWBinaryImageExt_CUDAFatbin_Params::list_ptx)
        .def("reset", &GWBinaryImageExt_CUDAFatbin_Params::reset);

    pybind11::class_<GWBinaryImageExt_CUDAFatbin> _class(m, "Fatbin");

    _class.def(py::init([]() {
        return GWBinaryImageExt_CUDAFatbin::create();
    }));

    _class.def_property_readonly(
        "params",
        [](GWBinaryImageExt_CUDAFatbin& self) -> GWBinaryImageExt_CUDAFatbin_Params& {
            return self.params();
        },
        py::return_value_policy::reference_internal,
        "get parameters of this fatbin"
    );

    _class.def(
        "fill",
        [](GWBinaryImageExt_CUDAFatbin& self, std::string file_path) {
            gw_retval_t tmp_retval = GW_SUCCESS;
            GWBinaryImage* binary_image = nullptr;
            GW_CHECK_POINTER(binary_image = self.get_base_ptr());
            GW_IF_FAILED(
                binary_image->fill(file_path),
                tmp_retval,
                {
                    throw GWException("failed to fill fatbin: file_path(%s)", file_path.c_str());
                }
            );
        },
        py::arg("file_path"),
        "fill this fatbin with the binary data from a file"
    );

    _class.def(
        "parse",
        [](GWBinaryImageExt_CUDAFatbin& self) {
            gw_retval_t tmp_retval = GW_SUCCESS;
            GWBinaryImage* binary_image = nullptr;
            GW_CHECK_POINTER(binary_image = self.get_base_ptr());
            GW_IF_FAILED(binary_image->parse(), tmp_retval, {
                throw GWException("failed to parse fatbin");
            });
        },
        "parse this fatbin"
    );
}
