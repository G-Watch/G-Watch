#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <mutex>

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cupti_target.h>
#include <cupti_profiler_target.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "profiler/profile_image.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/hash.hpp"

#include "profiler/cuda_impl/_profile_image/range_profile.hpp"

#if GW_CUDA_VERSION_MAJOR >= 12 && GW_CUDA_VERSION_MINOR >= 6
    #include "profiler/cuda_impl/_profile_image/pm_sampling.hpp"
#endif
