#include <iostream>
#include <string>

#include <pthread.h>
#include <stdint.h>
#include <dlfcn.h>

#include "nlohmann/json.hpp"

#include "common/common.hpp"
#include "common/utils/exception.hpp"
#include "capsule/event.hpp"
#include "capsule/trace.hpp"
#include "binding/runtime_control.hpp"

// public header
#include "gwatch/binary.hpp"


namespace gwatch {


BinaryImage::BinaryImage()
{}


BinaryImage::~BinaryImage()
{}


} // namespace gwatch
