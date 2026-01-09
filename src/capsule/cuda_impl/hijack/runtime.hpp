#pragma once
#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"


// dysym function
using dlsym_func_t = void*(void *, const char *);
extern thread_local dlsym_func_t *real_dlsym;


// global variables
extern GWCapsule *capsule;


extern "C" {

/*!
 *  \note   the function would not appear inside cuda.h,
 *          but they would indeed invoke by CUDA program
 *          when per-thread default stream is enabled, so
 *          we provide their definition here
 */
CUresult cuLaunchCooperativeKernel_ptsz (
    CUfunction f,
    unsigned int gridDimX,
    unsigned int gridDimY,
    unsigned int gridDimZ,
    unsigned int blockDimX,
    unsigned int blockDimY,
    unsigned int blockDimZ,
    unsigned int sharedMemBytes,
    CUstream hStream,
    void** kernelParams
);


/*!
 *  \note   the function would not appear inside cuda.h,
 *          but they would indeed invoke by CUDA program
 *          when per-thread default stream is enabled, so
 *          we provide their definition here
 */
CUresult cuLaunchHostFunc_ptsz(
    CUstream hStream,
    CUhostFn fn,
    void* userData
);


/*!
 *  \note   the function would not appear inside cuda.h,
 *          but they would indeed invoke by CUDA program
 *          when per-thread default stream is enabled, so
 *          we provide their definition here
 */
CUresult cuLaunchKernel_ptsz (
    CUfunction f,
    unsigned int gridDimX,
    unsigned int gridDimY,
    unsigned int gridDimZ,
    unsigned int blockDimX,
    unsigned int blockDimY,
    unsigned int blockDimZ,
    unsigned int sharedMemBytes,
    CUstream hStream,
    void** kernelParams,
    void** extra
);


/*!
 *  \note   the function would not appear inside cuda.h,
 *          but they would indeed invoke by CUDa program
 *          when per-thread default stream is enabled, so
 *          we provide their definition here
 */
CUresult cuLaunchKernelEx_ptsz (
    const CUlaunchConfig* config,
    CUfunction f,
    void** kernelParams,
    void** extra
);

} // extern "C"


/*!
 *  \brief  initialize real dysymbol function pointer
 */
void __init_real_dlsym();


/*!
 *  \brief  dispatching hooked CUDA driver API
 *  \param  symbol  symbol name of the CUDA driver API
 *  \return pointer to the hooked CUDA driver API; nullptr if not hooked
 */
void* __hook_cuda_driver_api(const char* symbol);
