#pragma once

#include <iostream>
#include <vector>

#include "gwatch/common.hpp"
#include "gwatch/cuda/profiler.hpp"


namespace gwatch {

namespace cuda {


// forward declaration
class BinaryImage_Cubin;
class KernelDef;
class Kernel;


class KernelDef {
 public:
    ~KernelDef();

 private:
    friend class BinaryImage_Cubin;
    friend class Profiler;


    /*!
     *  \brief  constructor
     *  \note   the constructor is private, use BinaryImage_Cubin::get_kerneldef_by_name() to create a kernel definition
     *  \param  gw_kerneldef_cuda_handle  the handle of the kernel definition
     */
    KernelDef(void* gw_kerneldef_cuda_handle);
   

    // handle to the kernel definition
    void *_gw_kerneldef_cuda_handle = nullptr;
};


class Kernel {
 public:
    Kernel(KernelDef* def);
    ~Kernel();

    // Kernel is not copyable or movable
    Kernel(Kernel&&) = delete;
    Kernel& operator=(Kernel&&) = delete;


    /*!
     *  \brief  get kernel definition
     *  \return kernel definition
     */
    KernelDef* get_def() const { return _def; }


 private:
    // kernel definition
    KernelDef* _def = nullptr;

    // handle to the kernel
    void *_gw_kernel_cuda_handle = nullptr;
};


} // namespace cuda

} // namespace gwatch
