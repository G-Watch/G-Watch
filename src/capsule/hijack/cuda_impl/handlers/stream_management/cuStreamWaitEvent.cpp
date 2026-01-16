#include <iostream>

#include <dlfcn.h>
#include <string.h>
#include <cuda.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "capsule/capsule.hpp"
#include "capsule/hijack/cuda_impl/runtime.hpp"
#include "common/cuda_impl/real_apis.hpp"


extern "C" {

CUresult cuStreamWaitEvent( CUstream hStream, CUevent hEvent, unsigned int  Flags ){
    CUresult cudv_retval = CUDA_SUCCESS;

    GW_CHECK_POINTER(capsule);

    // obtain the real cuda apis
    __init_real_cuda_apis();

    cudv_retval = real_cuStreamWaitEvent(hStream, hEvent, Flags);
    GW_DEBUG("called cuStreamWaitEvent");

    // if operation isn't success, we sync all previous messages to scheduler to be finished
    if(cudv_retval != CUDA_SUCCESS){
        capsule->sync_send_to_scheduler();
    }

exit:
    return cudv_retval;
}

} // extern "C"
