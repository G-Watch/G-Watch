#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>

#include "nlohmann/json.hpp"

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/instrument.hpp"
#include "common/utils/exception.hpp"


class GWBasicBlock {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     */
    GWBasicBlock(uint64_t instruction_size);


    /*!
     *  \brief  destructor
     */
    virtual ~GWBasicBlock();


    /*!
     *  \brief  serialize the basic block
     *  \return serialized json object
     */
    nlohmann::json serialize();

    // index of the basic block in CFG
    uint64_t id = 0;

    // base pc of this basic block
    uint64_t base_pc = 0x0;
    uint64_t end_pc = 0x0;

    // list of instructions in this basic block
    std::vector<GWInstruction*> list_instructions = {};

    // map of input and output basic blocks of this basic block
    std::map<GWBasicBlock*, std::pair<uint64_t,uint64_t>> map_in_bb = {};   // <bb, <from_pc, to_pc>>
    std::map<GWBasicBlock*, std::pair<uint64_t,uint64_t>> map_out_bb = {};  // <bb, <from_pc, to_pc>>

 protected:
    // size of the instruction
    uint64_t _instruction_size = 0;
    /* ==================== Common ==================== */


    /* ============== Register Liveness =============== */
 public:
    /*!
     *  \brief  obtain register define/use set
     *  \param  reg_type    register type
     *  \param  base_pc     base pc to start parsing
     *  \param  set_reg_idx output register define/use set
     *  \return GW_SUCCESS if success, otherwise GW_FAILED
     */
    gw_retval_t get_registers_define_set(std::string reg_type, uint64_t base_pc, std::set<uint64_t>& set_reg_idx);
    gw_retval_t get_registers_use_set(std::string reg_type, uint64_t base_pc, std::set<uint64_t>& set_reg_idx);


    //  register IN/OUT set in this basic block: <reg_type, <reg_id>>
    std::map<std::string, std::set<uint64_t>> map_registers_in = {};
    std::map<std::string, std::set<uint64_t>> map_registers_out = {};

 protected:
    /*!
     *  \brief  parse register DEFINE and USE set
     *  \param  base_pc     base pc to start parsing
     *  \return GW_SUCCESS if success, otherwise GW_FAILED
     */
    gw_retval_t __parse_register_use_define_set(std::string reg_type, uint64_t base_pc);

    // register define/use record
    typedef struct {
        uint64_t base_pc = 0;
        std::set<uint64_t> set_define_reg_idx = {};
        std::set<uint64_t> set_use_reg_idx = {};
    } reg_define_use_record_t;

    // set of register used/defined in this basic block: <reg_type, [reg_define_use_set_t]>
    std::map<std::string, std::vector<reg_define_use_record_t>> _map_registers_define_use_record = {};
    /* ============== Register Liveness =============== */
};


/*!
 *  \brief  represent a GPU kernel
 */
class GWKernelDef {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     */
    GWKernelDef();


    /*!
     *  \brief  destructor
     */
    virtual ~GWKernelDef();


    // getters
    inline uint64_t get_nb_instructions() const { return this->list_instructions.size(); }


    // name of the kernel
    std::string mangled_prototype = "";

    // parameter metatdatas of this kernel
    std::vector<uint64_t> list_param_sizes = {};
    std::vector<uint64_t> list_param_sizes_reversed = {};
    std::vector<uint64_t> list_param_offsets_reversed = {};

    // list of instructions in this kernel
    std::vector<GWInstruction*> list_instructions;
    std::map<uint64_t, GWInstruction*> map_pc_to_instruction = {};

    // list of basic block of this kernel
    std::vector<GWBasicBlock*> list_basic_blocks;

    // raw bytes of the kernel definition
    std::vector<uint8_t> raw_bytes;
    /* ==================== Common ==================== */


    /* ==================== Parser ==================== */
    /*!
     *  \brief  add list of instructions to the kernel
     *  \param  bytes   byte sequence of instructions
     *  \param  size    size of byte sequence
     *  \return GW_SUCCESS if success, otherwise GW_FAILURE
     */
    virtual gw_retval_t parse_instructions(const uint8_t *bytes, uint64_t size){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  parse the control flow graph of the kernel
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t parse_cfg(){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  parse register liveness in this kernel
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t parse_register_liveness(){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  identify whether all instructions have been parsed
     *  \return whether all instructions have been parsed
     */
    inline bool is_instructions_parsed() const {
        return this->list_instructions.size() > 0;
    }


    /*!
     *  \brief  identify whether CFG have been parsed
     *  \return whether CFG have been parsed
     */
    inline bool is_cfg_parsed() const {
        return this->list_basic_blocks.size() > 0;
    }


    /*!
     *  \brief  identify whether register liveness have been parsed
     *  \return whether register liveness have been parsed
     */
    inline bool is_register_liveness_parsed() const {
        return this->_is_register_liveness_parsed;
    }


    /*!
     *  \brief  obtain all basic blocks of this kernel
     *  \param  basic_blocks    output basic blocks
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t get_all_basic_blocks(std::vector<GWBasicBlock*>& basic_blocks) const {
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  obtain the basic block by pc
     *  \param  pc              pc of the basic block
     *  \param  basic_block     basic block that be found
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t get_basic_block_by_pc(uint64_t pc, GWBasicBlock*& basic_block) const {
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  export list of instructions inside this kernel
     *  \param  output_object    json object to export the instructions
     *  \return GW_SUCCESS for successfully dump
     */
    virtual gw_retval_t serialize_instrutions(nlohmann::json& output_object) const {
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  export CFG of this kernel
     *  \param  output_object    json object to export the CFG
     *  \return GW_SUCCESS for successfully dump
     */
    virtual gw_retval_t serialize_cfg(nlohmann::json& output_object) const {
        return GW_FAILED_NOT_IMPLEMENTAED;
    }

 protected:
    // whether register liveness have been parsed
    bool _is_register_liveness_parsed = false;
    /* ==================== Parser ==================== */


    /* ==================== Debug ==================== */
 public:
    typedef struct gw_dwarf_line_metadata {
        std::string file;
        uint64_t line;
        bool is_stmt;
        std::vector<uint64_t> addresses;
        std::vector<std::pair<uint64_t,uint64_t>> blocks; // inclusive [begin, end]
    } gw_dwarf_line_metadata_t;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(gw_dwarf_line_metadata_t, line, is_stmt, addresses, blocks);


    /*!
     *  \brief  append debug info of the kernel
     *  \param  map_address_to_line map from address to line number
     *  \param  map_line_is_stmt    map from line number to whether it is a statement
     *  \return GW_SUCCESS if success
     */
    gw_retval_t set_debug_info(
        std::map<uint64_t, std::tuple<std::string, uint64_t>> map_address_to_line,
        std::map<std::tuple<std::string, uint64_t>, bool> map_line_is_stmt
    );


    /*!
     *  \brief  export debug info of the kernel
     *  \param  output_object    json object to export the debug info
     *  \return GW_SUCCESS if success
     */
    gw_retval_t serialize_debug_info(nlohmann::json& output_object) const;

 protected:
    std::set<std::string> _set_filepaths = {};
    std::map<uint64_t, std::tuple<std::string, uint64_t>> _map_address_to_line = {};
    std::map<std::tuple<std::string, uint64_t>, std::vector<uint64_t>> _map_line_to_addresses = {};
    std::map<std::tuple<std::string, uint64_t>, gw_dwarf_line_metadata_t*> _map_line_metadata = {};
    /* ==================== Debug ==================== */


    /* ==================== Register Management ==================== */
 public:
    /*!
     *  \brief  export register trace
     *  \param  reg_type        type of the register (general, predicate, uniform, uniform_predicate)
     *  \param  output_objec    json object to export the register trace
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t serialize_register_op_trace(std::string reg_type, nlohmann::json& output_object) const {
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  obtain register trace
     *  \param  reg_type    type of the register to be exported
     *  \param  reg_trace   output trace  
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t get_register_op_trace(
        std::string reg_type, std::map<uint64_t, std::vector<std::pair<uint64_t, std::string>>>& reg_trace
    ) const {
        return GW_FAILED_NOT_IMPLEMENTAED;
    }
    /* ==================== Register Management ==================== */
};
