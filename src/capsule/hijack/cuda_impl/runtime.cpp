#include <iostream>
#include <vector>
#include <fstream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>
#include <signal.h>

#include "yaml-cpp/yaml.h"

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/cuda.hpp"
#include "common/utils/system.hpp"
#include "capsule/hijack/cuda_impl/runtime.hpp"
#include "capsule/capsule.hpp"


// global capsule instance within hijack lib
GWCapsule *capsule = nullptr;


void signal_handler(int signum) {
    if(signum == SIGTERM || signum == SIGINT || signum == SIGQUIT){
        if(capsule != nullptr){
            delete capsule;
            capsule = nullptr;
        }
        exit(0);
    }
}


__attribute__((constructor))
void gwatch_capsule_init() {
    CUresult cudv_retval = CUDA_SUCCESS;
    gw_retval_t retval = GW_SUCCESS;
    std::string env_value = "", coredump_file_path = "";
    bool enable = false;
    uint64_t param_size = 0;
    int coredump_generation_flag = 0;

    std::vector<std::string> list_nocapsule_processes = {
        /* nvidia process */
        "ptxas", "nvcc", "nvidia-smi", "cudafe++",

        /* gcc process */
        "cc1plus", "gcc", "cicc",

        /* linux process */
        "ldconfig"
    };
    if(std::find(
        list_nocapsule_processes.begin(),
        list_nocapsule_processes.end(),
        GWUtilSystem::get_process_name_from_proc_comm()) != list_nocapsule_processes.end()
    ){
        GW_DEBUG("process %s, skip initialize capsule", GWUtilSystem::get_process_name_from_proc_comm().c_str());
        return;
    }

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);

    // initialize CUDA driver APIs
    cuInit(0);

    // initialize capsule
    GW_CHECK_POINTER(capsule = new GWCapsule());

    // enable coredump if enabled
    retval = GWUtilSystem::get_env_variable("GW_ENABLE_COREDUMP", env_value);
    if(likely(retval == GW_SUCCESS)){
        // set coredump globally before any context been created
        enable = true;
        param_size = sizeof(enable);
        GW_IF_CUDA_DRIVER_FAILED(
            cuCoredumpSetAttributeGlobal((CUcoredumpSettings)CU_COREDUMP_ENABLE_ON_EXCEPTION, &enable, &param_size),
            cudv_retval,
            {
                GW_WARN(
                    "failed to set coredump attribute CU_COREDUMP_ENABLE_ON_EXCEPTION, CUresult(%s)",
                    GWUtilCUDA::get_driver_error_string(cudv_retval).c_str()
                );
                goto _exit_set_coredump;
            }
        );

        // set coredump file path
        coredump_file_path = capsule->get_coredump_file_path();
        param_size = coredump_file_path.size();
        GW_IF_CUDA_DRIVER_FAILED(
            cuCoredumpSetAttributeGlobal(CU_COREDUMP_FILE, coredump_file_path.data(), &param_size),
            cudv_retval,
            {
                GW_WARN(
                    "failed to set coredump attribute: CU_COREDUMP_FILE, CUresult(%s)",
                    GWUtilCUDA::get_driver_error_string(cudv_retval).c_str()
                );
                goto _exit_set_coredump;
            }
        );

        // disable CPU-side abort()
        coredump_generation_flag = CU_COREDUMP_DEFAULT_FLAGS;
        coredump_generation_flag |= CU_COREDUMP_SKIP_ABORT;
        param_size = sizeof(coredump_generation_flag);
        GW_IF_CUDA_DRIVER_FAILED(
            cuCoredumpSetAttributeGlobal(CU_COREDUMP_GENERATION_FLAGS, &coredump_generation_flag, &param_size),
            cudv_retval,
            {
                GW_WARN(
                    "failed to set coredump attribute: CU_COREDUMP_GENERATION_FLAGS(CU_COREDUMP_SKIP_ABORT), "
                    "CUresult(%s)",
                    GWUtilCUDA::get_driver_error_string(cudv_retval).c_str()
                );
                goto _exit_set_coredump;
            }
        );

    _exit_set_coredump:
        ;
    }

    GW_DEBUG("capsule is ready, go!");
}


__attribute__((destructor))
void gwatch_capsule_deinit() {
    if(likely(capsule != nullptr)){
        delete capsule;
        capsule = nullptr;
    }
    GW_DEBUG("capsule exit");
}


void* __hook_cuda_driver_api(const char* symbol){
    /* ======== hijack module/function/var/library loading ======== */
    if(strcmp(symbol, "cuModuleLoad") == 0)
        return (void*)(cuModuleLoad);
    if(strcmp(symbol, "cuModuleLoadData") == 0)
        return (void*)(cuModuleLoadData);
    if(strcmp(symbol, "cuModuleLoadDataEx") == 0)
        return (void*)(cuModuleLoadDataEx);
    if(strcmp(symbol, "cuModuleLoadFatBinary") == 0)
        return (void*)(cuModuleLoadFatBinary);
    if(strcmp(symbol, "cuModuleGetFunction") == 0)
        return (void*)(cuModuleGetFunction);
    if(strcmp(symbol, "cuLibraryLoadData") == 0)
        return (void*)(cuLibraryLoadData);
    if(strcmp(symbol, "cuLibraryGetModule") == 0)
        return (void*)(cuLibraryGetModule);
    if(strcmp(symbol, "cuLibraryGetKernel") == 0)
        return (void*)(cuLibraryGetKernel);

    /* ======== hijack kernel launching ======== */
    if(strcmp(symbol, "cuLaunchKernel") == 0)
        return (void*)(cuLaunchKernel);
    if(strcmp(symbol, "cuLaunchKernel_ptsz") == 0)
        return (void*)(cuLaunchKernel_ptsz);
    if(strcmp(symbol, "cuLaunchKernelEx") == 0)
        return (void*)(cuLaunchKernelEx);
    if(strcmp(symbol, "cuLaunchKernelEx_ptsz") == 0)
        return (void*)(cuLaunchKernelEx_ptsz);
    if(strcmp(symbol, "cuLaunchHostFunc") == 0)
        return (void*)(cuLaunchHostFunc);
    if(strcmp(symbol, "cuLaunchHostFunc_ptsz") == 0)
        return (void*)(cuLaunchHostFunc_ptsz);
    if(strcmp(symbol, "cuLaunchCooperativeKernel") == 0)
        return (void*)(cuLaunchCooperativeKernel);
    if(strcmp(symbol, "cuLaunchCooperativeKernel_ptsz") == 0)
        return (void*)(cuLaunchCooperativeKernel_ptsz);

    /* ======== hijack nvtx operation ======== */
    


    /* ======== hijack graph operation ======== */
    

    /* ======== hijack memory operation ======== */


    /* ======== hijack stream operation ======== */
    if(strcmp(symbol, "cuStreamSynchronize") == 0)
        return (void*)(cuStreamSynchronize);
    if(strcmp(symbol, "cuStreamWaitEvent") == 0)
        return (void*)(cuStreamWaitEvent);
    
    return nullptr;
}
