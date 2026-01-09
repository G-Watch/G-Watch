#include <iostream>
#include <vector>
#include <string>
#include <filesystem>

#include "common/common.hpp"
#include "common/log.hpp"
#include "profiler/device.hpp"
#include "common/utils/exception.hpp"


GWProfileDevice::GWProfileDevice(int device_id) : _device_id(device_id) {}


GWProfileDevice::~GWProfileDevice() {}
