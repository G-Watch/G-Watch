#include <iostream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "nlohmann/json.hpp"
#include "pybind11_json/pybind11_json.hpp"

#include "common/common.hpp"
#include "common/binary.hpp"
#include "common/assemble/instruction.hpp"
#include "common/utils/exception.hpp"
#include "common/cuda_impl/assemble/kernel_def_sass.hpp"


void __gw_pybind_cuda_init_kernel_def_sass_interface(pybind11::module_ &m){
    pybind11::class_<GWKernelDefExt_CUDA_SASS_Params>(m, "KernelDefSASSParams")
        .def(py::init<>())
        .def_readwrite("binary_cuda_cubin", &GWKernelDefExt_CUDA_SASS_Params::binary_cuda_cubin)
        .def_readwrite("arch_version", &GWKernelDefExt_CUDA_SASS_Params::arch_version)
        .def("reset", &GWKernelDefExt_CUDA_SASS_Params::reset);

    pybind11::class_<GWKernelDefExt_CUDA_SASS> kernel_def(m, "KernelDefSASS");

    kernel_def.def_property_readonly(
        "params",
        [](GWKernelDefExt_CUDA_SASS& self) -> GWKernelDefExt_CUDA_SASS_Params& {
            return self.params();
        },
        py::return_value_policy::reference_internal,
        "get parameters of this kernel def sass"
    );

    kernel_def.def(
        "get_list_instruction",
        [](GWKernelDefExt_CUDA_SASS& self) -> std::vector<GWInstruction*> {
            GWKernelDef* kernel_def = nullptr;
            GW_CHECK_POINTER(kernel_def = GWKernelDefExt_CUDA_SASS::get_base_ptr(&self));
            return kernel_def->list_instructions;
        },
        py::return_value_policy::reference_internal,
        "get list of instructions"
    );
}
