#pragma once
#include <iostream>
#include <string>

#include <stdint.h>


namespace gwatch {


/*!
 *  \brief  the error code of the trace context
 */
enum gwError : uint8_t {
    gwSuccess = 0,
    gwFailed,
};


} // namespace gwatch
