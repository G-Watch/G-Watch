#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <regex>
#include <fstream>
#include <filesystem>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/string.hpp"
#include "common/utils/exception.hpp"
#include "common/assemble/kernel.hpp"
#include "common/assemble/kernel_def.hpp"


GWKernel::GWKernel(GWKernelDef* def) : _def(def)
{}


GWKernel::~GWKernel()
{}
