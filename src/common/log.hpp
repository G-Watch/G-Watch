#pragma once

#include <stdio.h>
#include <iostream>
#include <string>

#define GWATCH_PRINT_ERROR         1
#define GWATCH_PRINT_WARN          1
#define GWATCH_PRINT_LOG           1
#define GWATCH_PRINT_DEBUG         1
#define GWATCH_PRINT_WITH_COLOR    0

#define _GWATCH_PRINT_POSITION(stream)\
{\
fprintf(stream, "\
\033[33mfile:\033[0m       %s;\n\
\033[33mfunction:\033[0m   %s;\n\
\033[33mline:\033[0m       %d;\n", __FILE__, __PRETTY_FUNCTION__, __LINE__);\
}

#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>

static __attribute__((unused)) std::string demangle(const char* name) {

    int status = -4; // some arbitrary value to eliminate the compiler warning

    // enable c++11 by passing the flag -std=c++11 to g++
    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(name, NULL, NULL, &status),
        std::free
    };

    return (status==0) ? res.get() : name;
}

#else

// does nothing if not g++
static __attribute__((unused)) std::string demangle(const char* name) {
    return name;
}

#endif

/**************************** GW_ERROR ****************************/

#if GWATCH_PRINT_ERROR
/*!
 * \brief   print error message (internal-used)
 */
#define _GW_ERROR(...)                                         \
{                                                               \
if constexpr(GWATCH_PRINT_WITH_COLOR)                              \
    fprintf(stdout, "\033[101m\033[97m GWATCH Error \033[0m ");    \
else                                                            \
    fprintf(stdout, "|-GWATCH Error-| ");                          \
fprintf(stdout, __VA_ARGS__);                                   \
fprintf(stdout, "\n");                                          \
}

/*!
 * \brief   print error message with class info (internal-used)
 * \note    this macro should only be expanded within c++ class
 */
#define _GW_ERROR_C(...)                                   \
{                                                           \
if constexpr(GWATCH_PRINT_WITH_COLOR)                          \
    fprintf(stdout,                                         \
        "\033[101m\033[97m GWATCH Error \033[0m [%s @ %p]  ",  \
        demangle(typeid(*this).name()).c_str(), this        \
    );                                                      \
else                                                        \
    fprintf(stdout,                                         \
        "|-GWATCH Error-| [%s @ %p]  ",                        \
        demangle(typeid(*this).name()).c_str(), this        \
    );                                                      \
fprintf(stdout, __VA_ARGS__);                               \
fprintf(stdout, "\n");                                      \
}

/*!
    * \brief   print error message, then fatally quit
    */
#define GW_ERROR(...)               \
{                                   \
_GW_ERROR(__VA_ARGS__)              \
fflush(0);                          \
exit(1);                            \
}

/*!
    * \brief   print error message with class info, then fatally quit
    * \note    this macro should only be expanded within c++ class
    */
#define GW_ERROR_C(...)         \
{                               \
_GW_ERROR_C(__VA_ARGS__)        \
fflush(0);                      \
exit(1);                        \
}

/*!
    * \brief   print error message with specific code position, then fatally quit
    */
#define GW_ERROR_DETAIL(...)    \
{                               \
_GW_ERROR(__VA_ARGS__)          \
_GWATCH_PRINT_POSITION(stdout)  \
fflush(0);                      \
exit(1);                        \
}

/*!
    * \brief   print error message with class info and specific code position,
    *          then fatally quit
    */
#define GW_ERROR_C_DETAIL(...)  \
{                               \
_GW_ERROR_C(__VA_ARGS__)        \
_GWATCH_PRINT_POSITION(stdout)  \
fflush(0);                      \
exit(1);                        \
}
#else
#define GW_ERROR(...)
#define GW_ERROR_C(...)
#define GW_ERROR_DETAIL(...)
#define GW_ERROR_C_DETAIL(...)
#endif

/**************************** GW_WARN *******************************/

#if GWATCH_PRINT_WARN
/*!
    * \brief   print warn message
    */
#define GW_WARN(...)                                           \
{                                                               \
if constexpr(GWATCH_PRINT_WITH_COLOR)                              \
    fprintf(stdout, "\033[103m\033[97m GWATCH Warn \033[0m ");     \
else                                                            \
    fprintf(stdout, "|-GWATCH Warn-| ");                           \
fprintf(stdout, __VA_ARGS__);                                   \
fprintf(stdout, "\n");                                          \
}

/*!
    * \brief   print warn message with class info
    * \note    this macro should only be expanded within c++ class
    */
#define GW_WARN_C(...)                                     \
{                                                           \
if constexpr(GWATCH_PRINT_WITH_COLOR)                          \
    fprintf(stdout,                                         \
        "\033[103m\033[97m GWATCH Warn \033[0m [%s @ %p]  ",   \
        demangle(typeid(*this).name()).c_str(), this        \
    );                                                      \
else                                                        \
    fprintf(stdout,                                         \
        "|-GWATCH Warn-| [%s @ %p]  ",                         \
        demangle(typeid(*this).name()).c_str(), this        \
    );                                                      \
fprintf(stdout, __VA_ARGS__);                               \
fprintf(stdout, "\n");                                      \
}

/*!
    * \brief   print warn message with class info and specific code position
    */
#define GW_WARN_DETAIL(...)    \
{                               \
GW_WARN(__VA_ARGS__)           \
_GWATCH_PRINT_POSITION(stdout)     \
}

/*!
    * \brief   print warn message with class info and specific code position
    * \note    this macro should only be expanded within c++ class
    */
#define GW_WARN_C_DETAIL(...)  \
{                               \
GW_WARN_C(__VA_ARGS__)         \
_GWATCH_PRINT_POSITION(stdout)     \
}
#else
#define GW_WARN(...)
#define GW_WARN_C(...)
#define GW_WARN_DETAIL(...)
#define GW_WARN_C_DETAIL(...)
#endif

/***************************** GW_LOG *****************************/

#if GWATCH_PRINT_LOG
/*!
    * \brief   print log message
    */
#define GW_LOG(...)                                        \
{                                                           \
if constexpr(GWATCH_PRINT_WITH_COLOR)                          \
    fprintf(stdout, "\033[104m\033[97m GWATCH Log \033[0m ");  \
else                                                        \
    fprintf(stdout, "|-GWATCH Log-| ");                        \
fprintf(stdout, __VA_ARGS__);                               \
fprintf(stdout, "\n");                                      \
}

/*!
    * \brief   print log message with class info
    * \note    this macro should only be expanded within c++ class
    */
#define GW_LOG_C(...)                                      \
{                                                           \
if constexpr(GWATCH_PRINT_WITH_COLOR)                          \
    fprintf(stdout,                                         \
        "\033[104m\033[97m GWATCH Log \033[0m [%s @ %p]  ",    \
        demangle(typeid(*this).name()).c_str(), this        \
    );                                                      \
else                                                        \
    fprintf(stdout,                                         \
        "|-GWATCH Log-| [%s @ %p]  ",                          \
        demangle(typeid(*this).name()).c_str(), this        \
    );                                                      \
fprintf(stdout, __VA_ARGS__);                               \
fprintf(stdout, "\n");                                      \
}

/*!
    * \brief   print log message with class info and specific code position
    */
#define GW_LOG_DETAIL(...)     \
{                               \
GW_LOG(__VA_ARGS__)            \
_GWATCH_PRINT_POSITION(stdout)     \
}

/*!
    * \brief   print log message with class info and specific code position
    * \note    this macro should only be expanded within c++ class
    */
#define GW_LOG_C_DETAIL(...)   \
{                               \
GW_LOG_C(__VA_ARGS__)          \
_GWATCH_PRINT_POSITION(stdout)     \
}
#else
#define GW_LOG(...)
#define GW_LOG_C(...)
#define GW_LOG_DETAIL(...)
#define GW_LOG_C_DETAIL(...)
#endif

/**************************** GW_DEBUG ****************************/

#if GWATCH_PRINT_DEBUG
/*!
    * \brief   print debug message
    */
#define GW_DEBUG(...)                                          \
{                                                               \
if constexpr(GWATCH_PRINT_WITH_COLOR)                              \
    fprintf(stdout, "\033[104m\033[42m GWATCH Debug \033[0m ");    \
else                                                            \
    fprintf(stdout, "|-GWATCH Debug-| ");                          \
fprintf(stdout, __VA_ARGS__);                                   \
fprintf(stdout, "\n");                                          \
}

/*!
    * \brief   print debug message with class info
    * \note    this macro should only be expanded within c++ class
    */
#define GW_DEBUG_C(...)                                    \
{                                                           \
if constexpr(GWATCH_PRINT_WITH_COLOR)                          \
    fprintf(stdout,                                         \
        "\033[104m\033[42m GWATCH Debug \033[0m [%s @ %p]  ",  \
        demangle(typeid(*this).name()).c_str(), this        \
    );                                                      \
else                                                        \
    fprintf(stdout,                                         \
        "|-GWATCH Debug-| [%s @ %p]  ",                        \
        demangle(typeid(*this).name()).c_str(), this        \
    );                                                      \
fprintf(stdout, __VA_ARGS__);                               \
fprintf(stdout, "\n");                                      \
}

/*!
    * \brief   print debug message with class info and specific code position
    */
#define GW_DEBUG_DETAIL(...)   \
{                               \
GW_DEBUG(__VA_ARGS__)          \
_GWATCH_PRINT_POSITION(stdout)     \
}

/*!
    * \brief   print debug message with class info and specific code position
    * \note    this macro should only be expanded within c++ class
    */
#define GW_DEBUG_C_DETAIL(...) \
{                               \
GW_DEBUG_C(__VA_ARGS__)        \
_GWATCH_PRINT_POSITION(stdout)     \
}
#else
#define GW_DEBUG(...)
#define GW_DEBUG_C(...)
#define GW_DEBUG_DETAIL(...)
#define GW_DEBUG_C_DETAIL(...)
#endif

#define GW_BACK_LINE
// #define GW_BACK_LINE   fprintf(stdout, "\033[F\033[K");
