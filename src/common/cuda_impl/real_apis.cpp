#include <iostream>
#include <vector>
#include <fstream>


#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "capsule/hijack/cuda_impl/runtime.hpp"
#include "common/cuda_impl/real_apis.hpp"


// global variables
static thread_local void *lib_cuda_handle = nullptr;
thread_local dlsym_func_t *real_dlsym = nullptr;


/* ==================== CUDA driver APIs ==================== */
// ==========> Driver Entry Point Access <==========
thread_local cu_get_proc_address_func_t *real_cuGetProcAddress = nullptr;

// ==========> Execution Control <==========
thread_local cu_launch_cooperative_kernel_func_t *real_cuLaunchCooperativeKernel = nullptr;
thread_local cu_launch_cooperative_kernel_ptsz_func_t *real_cuLaunchCooperativeKernel_ptsz = nullptr;
thread_local cu_launch_host_func_t *real_cuLaunchHostFunc = nullptr;
thread_local cu_launch_host_func_ptsz_t *real_cuLaunchHostFunc_ptsz = nullptr;
thread_local cu_launch_kernel_ex_func_t *real_cuLaunchKernelEx = nullptr;
thread_local cu_launch_kernel_ex_ptsz_func_t *real_cuLaunchKernelEx_ptsz = nullptr;
thread_local cu_launch_kernel_func_t *real_cuLaunchKernel = nullptr;
thread_local cu_launch_kernel_ptsz_func_t *real_cuLaunchKernel_ptsz = nullptr;

// ==========> Module Management <==========
thread_local cu_module_load_func_t *real_cuModuleLoad = nullptr;
thread_local cu_module_load_fat_binary_func_t *real_cuModuleLoadFatBinary = nullptr;
thread_local cu_module_load_data_func_t *real_cuModuleLoadData = nullptr;
thread_local cu_module_load_data_ex_func_t *real_cuModuleLoadDataEx = nullptr;
thread_local cu_module_get_function_func_t *real_cuModuleGetFunction = nullptr;

// ==========> Library Management <==========
thread_local cu_library_load_data_func_t *real_cuLibraryLoadData = nullptr;
thread_local cu_library_get_module_func_t *real_cuLibraryGetModule = nullptr;
thread_local cu_library_get_kernel_func_t *real_cuLibraryGetKernel = nullptr;
thread_local cu_kernel_get_function_func_t *real_cuKernelGetFunction = nullptr;
thread_local cu_library_load_from_file_func_t *real_cuLibraryLoadFromFile = nullptr;

// ==========> Stream Management <==========
thread_local cu_stream_synchronize_func_t *real_cuStreamSynchronize = nullptr;
thread_local cu_stream_wait_event_func_t *real_cuStreamWaitEvent = nullptr;
/* ==================== CUDA driver APIs ==================== */


void __init_real_dlsym(){
    if(unlikely(real_dlsym == nullptr)){
        #if defined(__GLIBC__) && !defined(__UCLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ < 34
            // these GLIBC_PRIVATE functions are not exposed after glibc 2.34
            void *__libc_dlsym(void *map, const char *name);
            void *__libc_dlopen_mode(const char *name, int mode);
            real_dlsym = (dlsym_func_t*)__libc_dlsym(
                __libc_dlopen_mode("libdl.so.2", RTLD_LAZY), "dlsym"
            );
        #else
            real_dlsym = (dlsym_func_t*)dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
        #endif
        GW_CHECK_POINTER(real_dlsym);
    }
}


void __init_real_cuda_apis(){
    static thread_local bool is_real_cuda_api_init = false;

    // avoid duplicated initialization
    if(likely(is_real_cuda_api_init == true)){
        goto exit;
    }

    // init dlsym
    __init_real_dlsym();

    // load libcuda.so
    if(unlikely(lib_cuda_handle == nullptr)){
        GW_CHECK_POINTER(lib_cuda_handle = dlopen("libcuda.so.1", RTLD_NOW));
    }

    /* ==================== CUDA driver APIs ==================== */
    // ==========> Driver Entry Point Access <==========
    // cuGetProcAddress
    #if CUDA_VERSION >= 12030
        real_cuGetProcAddress
            = (cu_get_proc_address_func_t*)real_dlsym(lib_cuda_handle, "cuGetProcAddress_v2");
    #else
        real_cuGetProcAddress
            = (cu_get_proc_address_func_t*)real_dlsym(lib_cuda_handle, "cuGetProcAddress");
    #endif
    GW_CHECK_POINTER(real_cuGetProcAddress);

    // ==========> Execution Control <==========
    // cuLaunchHostFunc
    real_cuLaunchHostFunc
        = (cu_launch_host_func_t*)real_dlsym(lib_cuda_handle, "cuLaunchHostFunc");
    GW_CHECK_POINTER(real_cuLaunchHostFunc);

    // cuLaunchHostFunc_ptsz
    real_cuLaunchHostFunc_ptsz
        = (cu_launch_host_func_ptsz_t*)real_dlsym(lib_cuda_handle, "cuLaunchHostFunc_ptsz");
    GW_CHECK_POINTER(real_cuLaunchHostFunc_ptsz);

    // cuLaunchKernelEx
    real_cuLaunchKernelEx
        = (cu_launch_kernel_ex_func_t*)real_dlsym(lib_cuda_handle, "cuLaunchKernelEx");
    GW_CHECK_POINTER(real_cuLaunchKernelEx);

    // cuLaunchKernelEx_ptsz
    real_cuLaunchKernelEx_ptsz
        = (cu_launch_kernel_ex_ptsz_func_t*)real_dlsym(lib_cuda_handle, "cuLaunchKernelEx_ptsz");
    GW_CHECK_POINTER(real_cuLaunchKernelEx_ptsz);

    // cuLaunchKernel
    real_cuLaunchKernel
        = (cu_launch_kernel_func_t*)real_dlsym(lib_cuda_handle, "cuLaunchKernel");
    GW_CHECK_POINTER(real_cuLaunchKernel);

    // cuLaunchKernel_ptsz
    real_cuLaunchKernel_ptsz
        = (cu_launch_kernel_ptsz_func_t*)real_dlsym(lib_cuda_handle, "cuLaunchKernel_ptsz");
    GW_CHECK_POINTER(real_cuLaunchKernel_ptsz);

    // ==========> Module Management <==========
    // cuModuleLoad
    real_cuModuleLoad
        = (cu_module_load_func_t*)real_dlsym(lib_cuda_handle, "cuModuleLoad");
    GW_CHECK_POINTER(real_cuModuleLoad);

    // cuModuleLoadFatBinary
    real_cuModuleLoadFatBinary
        = (cu_module_load_fat_binary_func_t*)real_dlsym(lib_cuda_handle, "cuModuleLoadFatBinary");
    GW_CHECK_POINTER(real_cuModuleLoadFatBinary);

    // cuModuleLoadData
    real_cuModuleLoadData
        = (cu_module_load_data_func_t*)real_dlsym(lib_cuda_handle, "cuModuleLoadData");
    GW_CHECK_POINTER(real_cuModuleLoadData);

    // cuModuleLoadDataEx
    real_cuModuleLoadDataEx
        = (cu_module_load_data_ex_func_t*)real_dlsym(lib_cuda_handle, "cuModuleLoadDataEx");
    GW_CHECK_POINTER(real_cuModuleLoadDataEx);

    // cuModuleGetFunction
    real_cuModuleGetFunction
        = (cu_module_get_function_func_t*)real_dlsym(lib_cuda_handle, "cuModuleGetFunction");
    GW_CHECK_POINTER(real_cuModuleGetFunction);

    // ==========> Library Management <==========
    // cuLibraryLoadFromFile
    real_cuLibraryLoadFromFile
        = (cu_library_load_from_file_func_t*)real_dlsym(lib_cuda_handle, "cuLibraryLoadFromFile");
    GW_CHECK_POINTER(real_cuLibraryLoadFromFile);

    // cuLibraryLoadData
    real_cuLibraryLoadData
        = (cu_library_load_data_func_t*)real_dlsym(lib_cuda_handle, "cuLibraryLoadData");
    GW_CHECK_POINTER(real_cuLibraryLoadData);

    // cuLibraryGetModule
    real_cuLibraryGetModule
        = (cu_library_get_module_func_t*)real_dlsym(lib_cuda_handle, "cuLibraryGetModule");
    GW_CHECK_POINTER(real_cuLibraryGetModule);
    
    // cuLibraryGetKernel
    real_cuLibraryGetKernel
        = (cu_library_get_kernel_func_t*)real_dlsym(lib_cuda_handle, "cuLibraryGetKernel");
    GW_CHECK_POINTER(real_cuLibraryGetKernel);

    // cuKernelGetFunction
    real_cuKernelGetFunction
        = (cu_kernel_get_function_func_t*)real_dlsym(lib_cuda_handle, "cuKernelGetFunction");
    GW_CHECK_POINTER(real_cuKernelGetFunction);

    // ==========> Stream Management <==========
    // cuStreamSynchronize
    real_cuStreamSynchronize
        = (cu_stream_synchronize_func_t*)real_dlsym(lib_cuda_handle, "cuStreamSynchronize");
    GW_CHECK_POINTER(real_cuStreamSynchronize);

    // cuStreamWaitEvent
    real_cuStreamWaitEvent
        = (cu_stream_wait_event_func_t*)real_dlsym(lib_cuda_handle, "cuStreamWaitEvent");
    GW_CHECK_POINTER(real_cuStreamWaitEvent);
    /* ==================== CUDA driver APIs ==================== */


    is_real_cuda_api_init = true;

exit:
    ;
}
