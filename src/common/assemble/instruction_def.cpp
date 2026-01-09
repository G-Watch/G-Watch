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
#include "common/assemble/operand_def.hpp"
#include "common/assemble/instruction_def.hpp"


GWInstructionFieldAttr::GWInstructionFieldAttr()
{}


GWInstructionFieldAttr::~GWInstructionFieldAttr()
{}


GWInstructionDef::GWInstructionDef(uint32_t instruction_size_) : instruction_size(instruction_size_)
{}


GWInstructionDef::~GWInstructionDef()
{}


gw_retval_t GWInstructionDef::get_field_attr_by_value_str(std::string value_str, GWInstructionFieldAttr*& field_attr) const {
    gw_retval_t retval = GW_FAILED_NOT_EXIST;
    typename std::map<std::string, GWInstructionFieldAttr*>::const_iterator map_iter;

    for(map_iter = this->map_value_name_to_field_attrs.begin(); map_iter != this->map_value_name_to_field_attrs.end(); map_iter++){
        if(map_iter->first == value_str){
            field_attr = map_iter->second;
            retval = GW_SUCCESS;
            break;
        }
    }

exit:
    return retval;
}
