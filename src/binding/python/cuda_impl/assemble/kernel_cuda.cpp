#include <iostream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "nlohmann/json.hpp"
#include "pybind11_json/pybind11_json.hpp"

#include "common/common.hpp"
#include "common/binary.hpp"
#include "common/utils/exception.hpp"
#include "common/assemble/kernel.hpp"
#include "common/assemble/kernel_def.hpp"
#include "common/cuda_impl/assemble/kernel_cuda.hpp"
#include "common/cuda_impl/assemble/kernel_def_sass.hpp"


void __gw_pybind_cuda_init_kernel_cuda_interface(pybind11::module_ &m){
    pybind11::class_<GWKernelExt_CUDA> kernel_cuda(m, "KernelCUDA");

    kernel_cuda.def_property(
        "grid_dim_x",
        [](GWKernelExt_CUDA &self) { return self.get_base_ptr()->grid_dim_x; },
        [](GWKernelExt_CUDA &self, uint32_t val) { self.get_base_ptr()->grid_dim_x = val; }
    );

    kernel_cuda.def_property(
        "grid_dim_y",
        [](GWKernelExt_CUDA &self) { return self.get_base_ptr()->grid_dim_y; },
        [](GWKernelExt_CUDA &self, uint32_t val) { self.get_base_ptr()->grid_dim_y = val; }
    );

    kernel_cuda.def_property(
        "grid_dim_z",
        [](GWKernelExt_CUDA &self) { return self.get_base_ptr()->grid_dim_z; },
        [](GWKernelExt_CUDA &self, uint32_t val) { self.get_base_ptr()->grid_dim_z = val; }
    );

    kernel_cuda.def_property(
        "block_dim_x",
        [](GWKernelExt_CUDA &self) { return self.get_base_ptr()->block_dim_x; },
        [](GWKernelExt_CUDA &self, uint32_t val) { self.get_base_ptr()->block_dim_x = val; }
    );

    kernel_cuda.def_property(
        "block_dim_y",
        [](GWKernelExt_CUDA &self) { return self.get_base_ptr()->block_dim_y; },
        [](GWKernelExt_CUDA &self, uint32_t val) { self.get_base_ptr()->block_dim_y = val; }
    );

    kernel_cuda.def_property(
        "block_dim_z",
        [](GWKernelExt_CUDA &self) { return self.get_base_ptr()->block_dim_z; },
        [](GWKernelExt_CUDA &self, uint32_t val) { self.get_base_ptr()->block_dim_z = val; }
    );

    kernel_cuda.def_property(
        "shared_mem_bytes",
        [](GWKernelExt_CUDA &self) { return self.get_base_ptr()->shared_mem_bytes; },
        [](GWKernelExt_CUDA &self, uint32_t val) { self.get_base_ptr()->shared_mem_bytes = val; }
    );

    kernel_cuda.def_property(
        "stream",
        [](GWKernelExt_CUDA &self) { return self.get_base_ptr()->stream; },
        [](GWKernelExt_CUDA &self, uint64_t val) { self.get_base_ptr()->stream = val; }
    );

    kernel_cuda.def_property_readonly(
        "def",
        [](GWKernelExt_CUDA &self) -> GWKernelDefExt_CUDA_SASS* {
            GWKernel* kernel = self.get_base_ptr();
            GWKernelDef* def = kernel->get_def();
            return GWKernelDefExt_CUDA_SASS::get_ext_ptr(def);
        },
        py::return_value_policy::reference,
        "get the kernel definition extension"
    );
}
