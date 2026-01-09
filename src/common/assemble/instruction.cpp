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
#include "common/assemble/instruction.hpp"
#include "common/assemble/instruction_def.hpp"
#include "common/assemble/operand.hpp"


GWInstruction::GWInstruction(GWInstructionDef* def) : _def(def)
{}


GWInstruction::~GWInstruction()
{}


GWInstruction& GWInstruction::operator=(const GWInstruction& other) {
    std::map<GWOperand*, GWOperand*> _map_new_old_operand = {};
    typename std::map<std::string, std::set<GWOperand*>>::const_iterator map_iter;

    if (this != &other) {
        // clear old operands
        for (auto& pair : this->map_operands) {
            if(pair.second != nullptr){
                delete pair.second;
            }
        }
        this->map_operands.clear();
        
        // clear old modifiers
        for (auto& pair : this->map_modifier) {
            if(pair.second != nullptr){
                delete pair.second;
            }
        }
        this->map_modifier.clear();
        
        // clear old register operand recordings
        this->map_register_operands.clear();
        
        // copy basic
        this->bytes = other.bytes;
        this->map_register_set_IN = other.map_register_set_IN;
        this->map_register_set_OUT = other.map_register_set_OUT;
        this->_def = other._def;
        
        //  clone operand map
        for (const auto& pair : other.map_operands) {
            this->map_operands[pair.first] = this->__clone_operand(pair.second);
            _map_new_old_operand[pair.second] = this->map_operands[pair.first];
        }

        // rebuild register operand map
        for(map_iter = other.map_register_operands.begin(); map_iter != other.map_register_operands.end(); map_iter++){
            this->map_register_operands[map_iter->first] = {};
            for(auto op : map_iter->second){
                this->map_register_operands[map_iter->first].insert(_map_new_old_operand[op]);
            }
        }

        // clone modifier map
        for (const auto& pair : other.map_modifier) {
            this->map_modifier[pair.first] = this->__clone_operand(pair.second);
        }
    }

    return *this;
}


GWOperand* GWInstruction::__clone_operand(const GWOperand* operand) const {
    if (!operand) return nullptr;
    GWOperand* new_operand = new GWOperand(*operand);
    return new_operand;
}
