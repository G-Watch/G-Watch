#include <iostream>
#include <string>

#include <dlfcn.h>
#include <pthread.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "binding/internal_interface.hpp"


GWCapsule *capsule = nullptr;


bool __init_capsule(){
    bool retval = false;
    static void* handle = nullptr;
    
    if(unlikely(capsule == nullptr)){
        handle = dlopen("libgwatch_capsule_hijack.so", RTLD_LAZY | RTLD_NOLOAD);
        if (handle) {
            capsule = (GWCapsule*)dlsym(handle, "capsule");
            GW_CHECK_POINTER(capsule);
            retval = true;
        } else {
            retval = false;
        }
    } else {
        retval = true;
    }

exit:
    return retval;
}
