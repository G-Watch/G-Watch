#include <iostream>

#include "common/common.hpp"
#include "common/log.hpp"

extern "C" {

thread_local bool global_cuLaunchKernel_at_first_level = true;

} // extern "C"
