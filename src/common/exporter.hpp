#pragma once

#include <iostream>
#include <string.h>
#include "common/common.hpp"


#define GW_GETSETTER_DECL_PRIM(PREFIX, BASE_TYPE, TYPE, NAME) \
    GW_EXPORT_API void PREFIX##_set_##NAME(BASE_TYPE* handle, TYPE val); \
    GW_EXPORT_API gw_retval_t PREFIX##_get_##NAME(BASE_TYPE* handle, TYPE* val);


#define GW_GETSETTER_DECL_STR(PREFIX, BASE_TYPE, NAME) \
    GW_EXPORT_API void PREFIX##_set_##NAME(BASE_TYPE* handle, const char* val); \
    GW_EXPORT_API gw_retval_t PREFIX##_get_##NAME(BASE_TYPE* handle, char* buf, size_t size);


#define GW_GETSETTER_IMPL_PRIM(PREFIX, BASE_TYPE, DERIVED_TYPE, TYPE, NAME) \
    extern "C" void PREFIX##_set_##NAME(BASE_TYPE* handle, TYPE val) { \
        GW_CHECK_POINTER(handle); \
        auto* obj = static_cast<DERIVED_TYPE*>(handle); \
        obj->NAME = val; \
    } \
    extern "C" gw_retval_t PREFIX##_get_##NAME(BASE_TYPE* handle, TYPE* val) { \
        GW_CHECK_POINTER(handle); \
        GW_CHECK_POINTER(val); \
        auto* obj = static_cast<DERIVED_TYPE*>(handle); \
        *val = obj->NAME; \
        return GW_SUCCESS; \
    }


#define GW_GETSETTER_IMPL_STR(PREFIX, BASE_TYPE, DERIVED_TYPE, NAME) \
    extern "C" void PREFIX##_set_##NAME(BASE_TYPE* handle, const char* val) { \
        GW_CHECK_POINTER(handle); \
        if (!val) return; \
        auto* obj = static_cast<DERIVED_TYPE*>(handle); \
        obj->NAME = std::string(val); \
    } \
    extern "C" gw_retval_t PREFIX##_get_##NAME(BASE_TYPE* handle, char* buf, size_t size) { \
        GW_CHECK_POINTER(handle); \
        GW_CHECK_POINTER(buf); \
        auto* obj = static_cast<DERIVED_TYPE*>(handle); \
        const std::string& src = obj->NAME; \
        if (src.length() + 1 > size) { \
            return GW_FAILED_INVALID_INPUT; \
        } \
        memcpy(buf, src.c_str(), src.length()); \
        buf[src.length()] = '\0'; \
        return GW_SUCCESS; \
    }
