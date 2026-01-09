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
#include "common/assemble/instruction_set.hpp"
#include "common/assemble/instruction_def.hpp"


GWInstructionSet::GWInstructionSet()
{}


GWInstructionSet::~GWInstructionSet() {
    for(auto instruction : this->_list_instructions){
        GW_CHECK_POINTER(instruction);
        delete instruction;
    }
}


gw_retval_t GWInstructionSet::create_instruction_shell_by_instruction_name(
    std::string instr_name, GWInstruction*& instr_instance
){
    gw_retval_t retval = GW_SUCCESS;
    GWInstructionDef *instruction_def = nullptr;

    if(unlikely(this->_map_name_to_instruction_def.find(instr_name) == this->_map_name_to_instruction_def.end())){
        GW_WARN_C(
            "failed to create instruction shell, instruction name invalid: "
            "instr_name: %s",
            instr_name.c_str()
        );
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }
    GW_CHECK_POINTER(instruction_def = this->_map_name_to_instruction_def[instr_name]);
    
    retval = instruction_def->create_instruction_shell(instr_instance);

exit:
    return retval;
}


gw_retval_t GWInstructionSet::create_instruction(
    std::string instr_name,
    std::map<std::tuple<std::string, std::string, std::string>, uint64_t> map_unsigned_operand,
    std::map<std::tuple<std::string, std::string, std::string>, int64_t> map_signed_operand,
    std::map<std::tuple<std::string, std::string, std::string, std::string>, uint64_t> map_unsigned_constrain,
    std::map<std::tuple<std::string, std::string, std::string, std::string>, int64_t> map_signed_constrain,
    std::map<std::string, uint64_t> map_modifier,
    GWInstruction*& instr_instance
){
    gw_retval_t retval = GW_SUCCESS;
    GWInstructionDef *instruction_def = nullptr;

    // verify that the instruction is existed
    if(unlikely(this->_map_name_to_instruction_def.find(instr_name) == this->_map_name_to_instruction_def.end())){
        GW_WARN_C(
            "failed to create instruction shell, instruction name invalid: "
            "instr_name: %s",
            instr_name.c_str()
        );
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }
    GW_CHECK_POINTER(instruction_def = this->_map_name_to_instruction_def[instr_name]);

    // create instruction shell
    GW_IF_FAILED(
        instruction_def->create_instruction_shell(instr_instance),
        retval,
        {
            GW_WARN_C(
                "failed to create instruction shell: "
                "instr_name(%s), error(%s)",
                instr_name.c_str(), gw_retval_str(retval)
            );
        }
    );

    // assign operand values
    for(auto [subop_info, value] : map_unsigned_operand){
        GW_IF_FAILED(
            instr_instance->set_operand_unsigned(
                /* opname */ std::get<0>(subop_info),
                /* suboperand_type */ std::get<1>(subop_info),
                /* suboperand_name */ std::get<2>(subop_info),
                /* value */ value
            ),
            retval,
            {
                GW_WARN_C(
                    "failed to set unsigned suboperand value: "
                    "instr_name(%s), opname(%s), subop_type(%s), subop_name(%s), value(%lu), error(%s)",
                    instr_name.c_str(),
                    std::get<0>(subop_info).c_str(),
                    std::get<1>(subop_info).c_str(),
                    std::get<2>(subop_info).c_str(),
                    value,
                    gw_retval_str(retval)
                );
                goto exit;
            }
        );
    }
    for(auto [subop_info, value] : map_signed_operand){
        GW_IF_FAILED(
            instr_instance->set_operand_signed(
                /* opname */ std::get<0>(subop_info),
                /* suboperand_type */ std::get<1>(subop_info),
                /* suboperand_name */ std::get<2>(subop_info),
                /* value */ value
            ),
            retval,
            {
                GW_WARN_C(
                    "failed to set signed suboperand value: "
                    "instr_name(%s), opname(%s), subop_type(%s), subop_name(%s), value(%ld), error(%s)",
                    instr_name.c_str(),
                    std::get<0>(subop_info).c_str(),
                    std::get<1>(subop_info).c_str(),
                    std::get<2>(subop_info).c_str(),
                    value,
                    gw_retval_str(retval)
                );
                goto exit;
            }
        );
    }

    // assign constrain values
    for(auto [constrain_info, value] : map_unsigned_constrain){
        GW_IF_FAILED(
            instr_instance->set_constrain_unsigned(
                /* opname */ std::get<0>(constrain_info),
                /* suboperand_type */ std::get<1>(constrain_info),
                /* suboperand_name */ std::get<2>(constrain_info),
                /* constrain_name */ std::get<3>(constrain_info),
                /* value */ value
            ),
            retval,
            {
                GW_WARN_C(
                    "failed to set unsigned constrain value: "
                    "instr_name(%s), opname(%s), subop_type(%s), subop_name(%s), constrain(%s), value(%lu), error(%s)",
                    instr_name.c_str(),
                    std::get<0>(constrain_info).c_str(),
                    std::get<1>(constrain_info).c_str(),
                    std::get<2>(constrain_info).c_str(),
                    std::get<3>(constrain_info).c_str(),
                    value,
                    gw_retval_str(retval)
                );
                goto exit;
            }
        );
    }
    for(auto [constrain_info, value] : map_signed_constrain){
        GW_IF_FAILED(
            instr_instance->set_constrain_signed(
                /* opname */ std::get<0>(constrain_info),
                /* suboperand_type */ std::get<1>(constrain_info),
                /* suboperand_name */ std::get<2>(constrain_info),
                /* constrain_name */ std::get<3>(constrain_info),
                /* value */ value
            ),
            retval,
            {
                GW_WARN_C(
                    "failed to set signed constrain value: "
                    "instr_name(%s), opname(%s), subop_type(%s), subop_name(%s), constrain(%s), value(%ld), error(%s)",
                    instr_name.c_str(),
                    std::get<0>(constrain_info).c_str(),
                    std::get<1>(constrain_info).c_str(),
                    std::get<2>(constrain_info).c_str(),
                    std::get<3>(constrain_info).c_str(),
                    value,
                    gw_retval_str(retval)
                );
                goto exit;
            }
        );
    }

    // assign modifiers
    for(auto [mfname, value] : map_modifier){
        GW_IF_FAILED(
            instr_instance->set_modifier(mfname, value),
            retval,
            {
                GW_WARN_C(
                    "failed to set modifier value: "
                    "instr_name(%s), modifier(%s), value(%ld), error(%s)",
                    instr_name.c_str(),
                    mfname.c_str(),
                    value,
                    gw_retval_str(retval)
                );
                goto exit;
            }
        );
    }

exit:
    return retval;
}
