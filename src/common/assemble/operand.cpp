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
#include "common/assemble/operand.hpp"
#include "common/assemble/operand_def.hpp"


GWOperand::GWOperand(const GWOperandDef* def) :_def(def)
{
    this->value.u64 = 0;
}


GWOperand::~GWOperand()
{}
