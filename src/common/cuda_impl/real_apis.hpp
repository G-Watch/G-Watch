#pragma once
#include <iostream>

#include <cuda.h>
#include <cuda_runtime.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"


/* ==================== System APIs ==================== */
/*!
 *  \brief  initialize real dysymbol function pointer
 */
void __init_real_dlsym();


// dysym function
using dlsym_func_t = void*(void *, const char *);
extern thread_local dlsym_func_t *real_dlsym;
/* ==================== System APIs ==================== */


/* ==================== CUDA driver APIs ==================== */
/*!
 *  \brief  initialize real CUDA APIs
 */
void __init_real_cuda_apis();


// ==========> Driver Entry Point Access <==========
// cuGetProcAddress
using cu_get_proc_address_func_t 
    = CUresult(const char *, void **, int, cuuint64_t, CUdriverProcAddressQueryResult*);
extern thread_local cu_get_proc_address_func_t *real_cuGetProcAddress;


// ==========> Execution Control <==========
// cuLaunchCooperativeKernel
using cu_launch_cooperative_kernel_func_t
    = CUresult(
        CUfunction, unsigned int, unsigned int, unsigned int,
        unsigned int, unsigned int, unsigned int, unsigned int,
        CUstream, void**
    );
extern thread_local cu_launch_cooperative_kernel_func_t *real_cuLaunchCooperativeKernel;


// cuLaunchCooperativeKernel_ptsz
using cu_launch_cooperative_kernel_ptsz_func_t 
    = CUresult(
        CUfunction, unsigned int, unsigned int, unsigned int,
        unsigned int, unsigned int, unsigned int, unsigned int,
        CUstream, void**
    );
extern thread_local cu_launch_cooperative_kernel_ptsz_func_t *real_cuLaunchCooperativeKernel_ptsz;


// cuLaunchHostFunc
using cu_launch_host_func_t = CUresult(CUstream, CUhostFn, void*);
extern thread_local cu_launch_host_func_t *real_cuLaunchHostFunc;


// cuLaunchHostFunc_ptsz
using cu_launch_host_func_ptsz_t = CUresult(CUstream, CUhostFn, void*);
extern thread_local cu_launch_host_func_ptsz_t *real_cuLaunchHostFunc_ptsz;


// cuLaunchKernelEx
using cu_launch_kernel_ex_func_t = CUresult(const CUlaunchConfig*, CUfunction, void**, void**);
extern thread_local cu_launch_kernel_ex_func_t *real_cuLaunchKernelEx;


// cuLaunchKernelEx_ptsz
using cu_launch_kernel_ex_ptsz_func_t = CUresult(const CUlaunchConfig*, CUfunction, void**, void**);
extern thread_local cu_launch_kernel_ex_ptsz_func_t *real_cuLaunchKernelEx_ptsz;


// cuLaunchKernel
using cu_launch_kernel_func_t 
    = CUresult(
        CUfunction, unsigned int, unsigned int, unsigned int,
        unsigned int, unsigned int, unsigned int, unsigned int,
        CUstream, void**, void**
    );
extern thread_local cu_launch_kernel_func_t *real_cuLaunchKernel;


// cuLaunchKernel_ptsz
using cu_launch_kernel_ptsz_func_t 
    = CUresult(
        CUfunction, unsigned int, unsigned int, unsigned int,
        unsigned int, unsigned int, unsigned int, unsigned int,
        CUstream, void**, void**
    );
extern thread_local cu_launch_kernel_ptsz_func_t *real_cuLaunchKernel_ptsz;


// ==========> Module Management <==========
// cuModuleLoad
using cu_module_load_func_t = CUresult(CUmodule*, const char*);
extern thread_local cu_module_load_func_t *real_cuModuleLoad;


// cuModuleLoadFatBinary
using cu_module_load_fat_binary_func_t = CUresult(CUmodule*, const void*);
extern thread_local cu_module_load_fat_binary_func_t *real_cuModuleLoadFatBinary;


// cuModuleLoadData
using cu_module_load_data_func_t = CUresult(CUmodule*, const void*);
extern thread_local cu_module_load_data_func_t *real_cuModuleLoadData;


// cuModuleLoadDataEx
using cu_module_load_data_ex_func_t = CUresult(CUmodule* module, const void*, unsigned int, CUjit_option*, void**);
extern thread_local cu_module_load_data_ex_func_t *real_cuModuleLoadDataEx;


// cuModuleGetFunction
using cu_module_get_function_func_t = CUresult(CUfunction*, CUmodule, const char*);
extern thread_local cu_module_get_function_func_t *real_cuModuleGetFunction;


// ==========> Library Management <==========
// cuLibraryLoadData
using cu_library_load_data_func_t 
    = CUresult(
        CUlibrary*, const void*, CUjit_option*, void**,
        unsigned int, CUlibraryOption*, void**, unsigned int
    );
extern thread_local cu_library_load_data_func_t *real_cuLibraryLoadData;


// cuLibraryGetModule
using cu_library_get_module_func_t = CUresult(CUmodule*, CUlibrary);
extern thread_local cu_library_get_module_func_t *real_cuLibraryGetModule;


// cuLibraryGetKernel
using cu_library_get_kernel_func_t = CUresult(CUkernel*, CUlibrary, const char*);
extern thread_local cu_library_get_kernel_func_t *real_cuLibraryGetKernel;


// cuKernelGetFunction
using cu_kernel_get_function_func_t = CUresult(CUfunction*, CUkernel);
extern thread_local cu_kernel_get_function_func_t *real_cuKernelGetFunction;


// cuLibraryLoadFromFile
using cu_library_load_from_file_func_t 
    = CUresult(
        CUlibrary*, const char*, CUjit_option*,
        void**, unsigned int, CUlibraryOption*,
        void**, unsigned int
    );
extern thread_local cu_library_load_from_file_func_t *real_cuLibraryLoadFromFile;


// ==========> Stream Management <==========
// cuStreamSynchronize
using cu_stream_synchronize_func_t = CUresult(CUstream);
extern thread_local cu_stream_synchronize_func_t *real_cuStreamSynchronize;


// cuStreamWaitEvent
using cu_stream_wait_event_func_t = CUresult(CUstream, CUevent, unsigned int);
extern thread_local cu_stream_wait_event_func_t *real_cuStreamWaitEvent;
/* ==================== CUDA driver APIs ==================== */
