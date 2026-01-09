#include <iostream>
#include <map>
#include <thread>
#include <signal.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "profiler/context.hpp"
#include "profiler/profiler.hpp"


#if GW_BACKEND_CUDA
    #include "profiler/cuda_impl/context.hpp"
#else
    // TODO(zhuobin): support ROCm
#endif


static volatile bool do_exit = false;


void signal_handler(int signum) {
    if(signum == SIGTERM || signum == SIGKILL || signum == SIGINT || signum == SIGQUIT){
        do_exit = true;
    }
}


int main(int argc, char** argv) {
    gw_retval_t retval;

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);

    #if GW_BACKEND_CUDA
        GWProfileContext_CUDA context;
        GW_IF_FAILED(
            context.serve(),
            retval,
            {
                GW_WARN("failed to start the profiler context to serve");
                return -1;
            }
        );
    #else
        // TODO(zhuobin): support ROCm
        return -1;
    #endif

    while(!do_exit){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
