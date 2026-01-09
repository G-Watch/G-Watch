#include <iostream>
#include <string>

#include <pthread.h>
#include <stdint.h>
#include <dlfcn.h>

#include "nlohmann/json.hpp"

#include "common/common.hpp"


// public header
#include "gwatch/cuda/kernel.hpp"


namespace gwatch {

namespace cuda {


KernelDef::KernelDef(void* gw_kerneldef_cuda_handle)
    : _gw_kerneldef_cuda_handle(gw_kerneldef_cuda_handle)
{}


KernelDef::~KernelDef()
{}


Kernel::Kernel(KernelDef* def)
    : _def(def)
{}


Kernel::~Kernel() {}


} // namespace cuda

} // namespace gwatch
