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
#include "common/assemble/kernel_def.hpp"
#include "common/assemble/operand.hpp"
#include "common/assemble/operand_def.hpp"
#include "common/assemble/instruction.hpp"
#include "common/assemble/instruction_def.hpp"


GWBasicBlock::GWBasicBlock(uint64_t instruction_size)
    : _instruction_size(instruction_size)
{}


GWBasicBlock::~GWBasicBlock()
{}


nlohmann::json GWBasicBlock::serialize(){
    uint64_t i = 0, current_pc = 0;
    nlohmann::json bb_json_obj, tmp_json_obj;
    GWInstruction* inst = nullptr;

    bb_json_obj = nlohmann::json::object();
    tmp_json_obj = nlohmann::json::object();

    // id
    bb_json_obj["id"] = this->id;

    // instructions
    bb_json_obj["instructions"] = nlohmann::json::array();
    for (i = 0; i < this->list_instructions.size(); i++) {
        GW_CHECK_POINTER(inst = this->list_instructions[i]);
        current_pc = this->base_pc + i * inst->get_def()->instruction_size;
        tmp_json_obj.clear();
        tmp_json_obj["pc"] = current_pc;
        tmp_json_obj["decode"] = inst->str(/* flatten */ true, /* simply */ true);
        bb_json_obj["instructions"].push_back(tmp_json_obj);
    }

    // incoming edges
    bb_json_obj["incoming_edges"] = nlohmann::json::array();
    for (auto& [from_bb, pc_pair] : this->map_in_bb) {
        tmp_json_obj.clear();
        tmp_json_obj["from_bb_id"] = from_bb->id;
        tmp_json_obj["from_pc"] = pc_pair.first;
        tmp_json_obj["to_pc"] = pc_pair.second;
        bb_json_obj["incoming_edges"].push_back(tmp_json_obj);
    }

    // outgoing edges
    bb_json_obj["outgoing_edges"] = nlohmann::json::array();
    for (auto& [to_bb, pc_pair] : this->map_out_bb) {
        tmp_json_obj.clear();
        tmp_json_obj["to_bb_id"] = to_bb->id;
        tmp_json_obj["from_pc"] = pc_pair.first;
        tmp_json_obj["to_pc"] = pc_pair.second;
        bb_json_obj["outgoing_edges"].push_back(tmp_json_obj);
    }

    // register liveness
    bb_json_obj["map_registers_in"] = this->map_registers_in;
    bb_json_obj["map_registers_out"] = this->map_registers_out;
    
    for(auto& [reg_type, list_record] : this->_map_registers_define_use_record){
        bb_json_obj["map_registers_define"][reg_type] = nlohmann::json::array();
        for(auto& reg_define_use_record : list_record){
            tmp_json_obj.clear();
            tmp_json_obj["base_pc"] = reg_define_use_record.base_pc;
            tmp_json_obj["set_reg_idx"] = reg_define_use_record.set_define_reg_idx;
            bb_json_obj["map_registers_define"][reg_type].push_back(tmp_json_obj);
        }

        bb_json_obj["map_registers_use"][reg_type] = nlohmann::json::array();
        for(auto& reg_define_use_record : list_record){
            tmp_json_obj.clear();
            tmp_json_obj["base_pc"] = reg_define_use_record.base_pc;
            tmp_json_obj["set_reg_idx"] = reg_define_use_record.set_use_reg_idx;
            bb_json_obj["map_registers_use"][reg_type].push_back(tmp_json_obj);
        }
    }

    return bb_json_obj;
}


gw_retval_t GWBasicBlock::get_registers_define_set(
    std::string reg_type, uint64_t base_pc, std::set<uint64_t>& set_reg_idx
){
    gw_retval_t retval = GW_SUCCESS;

    // lazely parse
    GW_IF_FAILED(
        this->__parse_register_use_define_set(reg_type, base_pc),
        retval,
        {
            GW_WARN_C("failed to parse register define/use set, reg_type(%s), base_pc(%lu), error(%s)", reg_type.c_str(), base_pc, gw_retval_str(retval));
            goto exit;
        }
    );

    // find the record
    for(auto& reg_define_use_record : this->_map_registers_define_use_record[reg_type]){
        if(reg_define_use_record.base_pc == base_pc){
            set_reg_idx = reg_define_use_record.set_define_reg_idx;
            goto exit;
        }
    }

    // not found
    GW_WARN_C("register define record not exist, reg_type(%s), base_pc(%lu)", reg_type.c_str(), base_pc);
    retval = GW_FAILED_NOT_EXIST;

exit:
    return retval;
}


gw_retval_t GWBasicBlock::get_registers_use_set(
    std::string reg_type, uint64_t base_pc, std::set<uint64_t>& set_reg_idx
){
    gw_retval_t retval = GW_SUCCESS;

    // lazely parse
    GW_IF_FAILED(
        this->__parse_register_use_define_set(reg_type, base_pc),
        retval,
        {
            GW_WARN_C("failed to parse register define/use set, reg_type(%s), base_pc(%lu), error(%s)", reg_type.c_str(), base_pc, gw_retval_str(retval));
            goto exit;
        }
    );

    // find the record
    for(auto& reg_define_use_record : this->_map_registers_define_use_record[reg_type]){
        if(reg_define_use_record.base_pc == base_pc){
            set_reg_idx = reg_define_use_record.set_use_reg_idx;
            goto exit;
        }
    }

    // not found
    GW_WARN_C("register use record not exist, reg_type(%s), base_pc(%lu)", reg_type.c_str(), base_pc);
    retval = GW_FAILED_NOT_EXIST;

exit:
    return retval;
}


gw_retval_t GWBasicBlock::__parse_register_use_define_set(std::string reg_type, uint64_t base_pc){
    gw_retval_t retval = GW_SUCCESS;
    uint64_t i = 0, instr_index = 0;
    GWInstruction* instruction = nullptr;
    const GWOperandDef* operand_def = nullptr;
    reg_define_use_record_t reg_define_use_record;
    
    // check duplication
    for(auto& previous_set : this->_map_registers_define_use_record[reg_type]){
        if(previous_set.base_pc == base_pc){ goto exit; }
    }

    // iterate over instructions
    GW_ASSERT(base_pc % this->_instruction_size == 0);
    // GW_LOG_C("bb: %p, given_base_pc: %lu, base_pc: %lu, list_instructions_size: %lu", this, base_pc, this->base_pc, this->list_instructions.size());
    for(i = (base_pc - this->base_pc) / this->_instruction_size; i < this->list_instructions.size(); i++){
        // obtain instruction
        GW_CHECK_POINTER(instruction = this->list_instructions[i]);
        // GW_LOG_C("i: %lu, instr: %p", i, instruction);

        for(GWOperand* operand : instruction->map_register_operands[reg_type]){
            GW_CHECK_POINTER(operand_def = operand->get_def());
            if(operand_def->optype == "w"){
                // conduct write to the register, i.e., define
                reg_define_use_record.set_define_reg_idx.insert(operand->value.u64);
            } else if (
                operand_def->optype == "r"
                and reg_define_use_record.set_define_reg_idx.count(operand->value.u64) == 0
            ){
                // conduct read after no previous write, i.e., use
                reg_define_use_record.set_use_reg_idx.insert(operand->value.u64);
            }
        }   
    }

    // add the record
    reg_define_use_record.base_pc = base_pc;
    this->_map_registers_define_use_record[reg_type].push_back(reg_define_use_record);

exit:
    return retval;
}


GWKernelDef::GWKernelDef()
{}


GWKernelDef::~GWKernelDef()
{}


gw_retval_t GWKernelDef::set_debug_info(
    std::map<uint64_t, std::tuple<std::string, uint64_t>> map_address_to_line,
    std::map<std::tuple<std::string, uint64_t>, bool> map_line_is_stmt
){
    using _file_line_t = std::tuple<std::string,uint64_t>;

    gw_retval_t retval = GW_SUCCESS;
    gw_dwarf_line_metadata_t* line_metadata = nullptr;

    auto __is_addr_contiguous = [](uint64_t prev, uint64_t next)->bool {
        return next == prev + 1;
    };

    // set filepaths set
    std::set<std::string> filepaths_set;
    for (const auto &kv : map_address_to_line) {
        const _file_line_t &line = kv.second;
        filepaths_set.insert(std::get<0>(line));
    }

    // set address-to-line map
    this->_map_address_to_line = map_address_to_line;

    // set line-to-addresses map
    for (const auto &kv : map_address_to_line) {
        uint64_t addr = kv.first;
        const _file_line_t &line = kv.second;
        this->_map_line_to_addresses[line].push_back(addr);
    }
    for (auto &kv : this->_map_line_to_addresses) {
        auto &vec = kv.second;
        std::sort(vec.begin(), vec.end());
        vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
    }

    // parse line metadata
    for (const auto &kv : this->_map_line_to_addresses) {
        const _file_line_t &fl = kv.first;
        const auto &addrs = kv.second;

        if(this->_map_line_metadata.count(fl) > 0){
            line_metadata = this->_map_line_metadata[fl];
        } else {
            line_metadata = new gw_dwarf_line_metadata_t();
            this->_map_line_metadata[fl] = line_metadata;
        }

        // set basic line metadata
        line_metadata->file = std::get<0>(fl);
        line_metadata->line = std::get<1>(fl);
        line_metadata->addresses = addrs;
        if(map_line_is_stmt.find(fl) != map_line_is_stmt.end()){
            line_metadata->is_stmt = map_line_is_stmt[fl];
        } else {
            line_metadata->is_stmt = false;
        }

        // set line address block
        if (!addrs.empty()) {
            uint64_t block_begin = addrs[0];
            uint64_t prev = addrs[0];
            for (size_t i = 1; i < addrs.size(); ++i) {
                uint64_t cur = addrs[i];
                if (__is_addr_contiguous(prev, cur)) {
                    // extend current block
                    prev = cur;
                } else {
                    // close previous block and start new
                    line_metadata->blocks.emplace_back(block_begin, prev);
                    block_begin = cur;
                    prev = cur;
                }
            }
            // push last block
            line_metadata->blocks.emplace_back(block_begin, prev);
        } else {
            GW_WARN_C(
                "kernel line has no binary addresses: kernel(%s), file(%s), line(%lu)",
                this->mangled_prototype.c_str(), std::get<0>(fl).c_str(), std::get<1>(fl)
            );
        }
    }

exit:
    return retval;
}


gw_retval_t GWKernelDef::serialize_debug_info(nlohmann::json& output_object) const {
    gw_retval_t retval = GW_SUCCESS;

    try {
        output_object["mangled_prototype"] = this->mangled_prototype;
        output_object["map_address_to_line"] = this->_map_address_to_line;
        output_object["map_line_to_addresses"] = this->_map_line_to_addresses;
        output_object["map_line_metadata"] = nlohmann::json::object();
        for (const auto &kv : this->_map_line_metadata) {
            const auto &fileline = kv.first; // tuple<string,line>
            const std::string &file = std::get<0>(fileline);
            uint64_t line = std::get<1>(fileline);
            const gw_dwarf_line_metadata_t* meta_ptr = kv.second;
            
            if (!meta_ptr) continue;
            output_object["map_line_metadata"][file][std::to_string(line)] = *meta_ptr;
        }
    } catch (const std::exception &e) {
        GW_WARN_C(
            "failed to serialize debug info, caught exception: kernel(%s), exception(%s)",
            this->mangled_prototype.c_str(),
            e.what()
        );
        retval = GW_FAILED;
    } catch (...) {
        GW_WARN_C(
            "failed to serialize debug info, unknown exception: kernel(%s)",
            this->mangled_prototype.c_str()
        );
        retval = GW_FAILED;
    }

 exit:
    return retval;
}
