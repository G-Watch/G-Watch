#include <iostream>
#include <string>
#include <vector>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/assemble/kernel.hpp"
#include "capsule/capsule.hpp"
#include "capsule/event.hpp"
#include "binding/internal_interface.hpp"


void GW_INTERNAL_start_capture_kernel_launch(){
    static bool is_hijacklib_loaded = __init_capsule();

    if(is_hijacklib_loaded){

    }
}


std::vector<GWKernel> GW_INTERNAL_stop_capture_kernel_launch(){
    std::vector<GWKernel> list_kernel = {};
    static bool is_hijacklib_loaded = __init_capsule();

    if(is_hijacklib_loaded){

    }

    return list_kernel;
}
