#pragma once

#include <cassert>
#include <string_view>

// branch acceleration
#ifdef __GNUC__
    #define likely(x)       __builtin_expect(!!(x), 1)
    #define unlikely(x)     __builtin_expect(!!(x), 0)
#else
    #define likely(x)       (x)
    #define unlikely(x)     (x)
#endif
#define _unused(x) ((void)(x))  // make production build happy


// runtime check
#define GW_ENABLE_RUNTIME_CHECK 1
#if GW_ENABLE_RUNTIME_CHECK
    #define GW_ASSERT(x)           assert(x);
    // #define GW_CHECK_POINTER(ptr)  assert((ptr) != nullptr);
    #define GW_CHECK_POINTER(ptr)  _unused (ptr);
#else
    #define GW_ASSERT(x)           _unused (x);
    #define GW_CHECK_POINTER(ptr)  _unused (ptr);
#endif

#define GW_STATIC_ASSERT(x)    static_assert(x);


constexpr std::string_view __gw_relative_path(std::string_view full_path) {
#ifdef GW_CODE_ROOT_PATH
    std::string_view root_path { GW_CODE_ROOT_PATH };

    if (full_path.starts_with(root_path)) {
        return full_path.substr(root_path.length());
    }
#endif

    return full_path;
}
#define __GW_RELATIVE_FILE__ __gw_relative_path(__FILE__)


// values
#define KB(x)   ((size_t) (x) << 10)
#define MB(x)   ((size_t) (x) << 20)
#define GB(x)   ((size_t) (x) << 30)


// visibility of APIs
#if defined(_WIN32)
    #if defined(GW_BUILDING)
        #define GW_EXPORT_API __declspec(dllexport)
    #else
        #define GW_EXPORT_API __declspec(dllimport)
    #endif
    #define GW_LOCAL_API
#elif defined(__GNUC__) || defined(__clang__)
    #define GW_EXPORT_API __attribute__((visibility("default")))
    #define GW_LOCAL_API __attribute__((visibility("hidden")))
#else
    #define GW_EXPORT_API
    #define GW_LOCAL_API
#endif


// return values
enum gw_retval_t {
    GW_SUCCESS = 0,
    GW_FAILED,
    GW_FAILED_HARDWARE,
    GW_FAILED_SDK,
    GW_FAILED_NOT_READY,
    GW_FAILED_NOT_EXIST,
    GW_FAILED_ALREADY_EXIST,
    GW_FAILED_NOT_IMPLEMENTAED,
    GW_FAILED_INVALID_INPUT
};
const char* gw_retval_str(gw_retval_t retval);


#define __GW_IF_FAILED(api_call, retval, callback, success_code)        \
    if (unlikely((retval = (api_call)) != success_code)){               \
        GW_WARN("failed to call %s: retval(%d)", #api_call, retval)     \
        {                                                               \
            callback                                                    \
        }                                                               \
    }

#define GW_IF_FAILED(api_call, retval, callback)                        \
        __GW_IF_FAILED(api_call, retval, callback, GW_SUCCESS)

#define GW_IF_CUDA_DRIVER_FAILED(api_call, retval, callback)            \
        __GW_IF_FAILED(api_call, retval, callback, CUDA_SUCCESS)

#define GW_IF_CUDA_DRIVER_FAILED(api_call, retval, callback)            \
        __GW_IF_FAILED(api_call, retval, callback, CUDA_SUCCESS)

#define GW_IF_CUDA_RUNTIME_FAILED(api_call, retval, callback)           \
        __GW_IF_FAILED(api_call, retval, callback, cudaSuccess)

#define GW_IF_CUPTI_FAILED(api_call, retval, callback)                  \
        __GW_IF_FAILED(api_call, retval, callback, CUPTI_SUCCESS)

#define GW_IF_CUPTI_UTIL_FAILED(api_call, retval, callback)             \
        __GW_IF_FAILED(api_call, retval, callback, CUPTI_UTIL_SUCCESS)

#define GW_IF_NVPW_FAILED(api_call, retval, callback)                   \
        __GW_IF_FAILED(api_call, retval, callback, NVPA_STATUS_SUCCESS)

#define GW_IF_NVML_FAILED(api_call, retval, callback)                   \
        __GW_IF_FAILED(api_call, retval, callback, NVML_SUCCESS)

#define GW_IF_CURL_FAILED(api_call, retval, callback)                   \
        __GW_IF_FAILED(api_call, retval, callback, CURLE_OK)

#define GW_IF_LIBC_FAILED(api_call, retval, callback)                   \
        __GW_IF_FAILED(api_call, retval, callback, 0)

#define GW_DEFAULT_LOG_PATH                 "/var/gwatch"
#define GW_SCHEDULER_IPV4                   "127.0.0.1"
#define GW_SCHEDULER_SERVE_WS_PORT          10322
#define GW_WEBSOCKET_CHUNK_SIZE             4096
#define GW_WEBSOCKET_MAX_MESSAGE_LENGTH     32*(1<<20)  // 32MB


#ifdef GW_BACKEND_CUDA
    #include <cuda.h>
    #include <cuda_runtime_api.h>

    #define GW_CUDA_VERSION         CUDA_VERSION
    #define GW_CUDA_VERSION_MAJOR   CUDA_VERSION / 1000
    #define GW_CUDA_VERSION_MINOR   (CUDA_VERSION % 1000) / 10
#endif // GW_BACKEND_CUDA


// for debug
// #define GW_CUDA_VERSION_MAJOR 12
// #define GW_CUDA_VERSION_MINOR 8
