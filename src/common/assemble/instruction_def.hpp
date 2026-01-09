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


// forward declaration
class GWOperandDef;
class GWInstruction;


using gw_instruction_opcode_t = uint16_t;


/*!
 *  \brief  represent a field attribute
 */
class GWInstructionFieldAttr {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     */
    GWInstructionFieldAttr();


    /*!
     *  \brief  destructor
     */
    virtual ~GWInstructionFieldAttr();


    /*!
     *  \brief  get length of the field
     *  \return length of the field
     */
    inline uint64_t get_field_length(){
        uint64_t length = 0;
        for(auto pair : this->bitmap){
            length += pair.second - pair.first + 1;
        }
        return length;
    }

    // bit position of the field
    std::vector<std::pair<uint64_t, uint64_t>> bitmap;

    // string of the value (e.g., register name, imm name)
    std::string value_str = "";
    /* ==================== Common ==================== */
};


/*!
 *  \brief  represent an instruction definition
 */
class GWInstructionDef {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     */
    GWInstructionDef(uint32_t instruction_size_);


    /*!
     *  \brief  destructor
     */
    virtual ~GWInstructionDef();


    /*!
     *  \brief  load instruction details
     *  \param  proto   proto object to be deserialize
     *  \return GW_SUCCESS if success, otherwise GW_FAILURE
     */
    virtual gw_retval_t from_proto(std::istream& proto){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  disassemble instruction instance from byte sequence
     *  \param  bytes               byte sequence
     *  \param  instr_instance      output instruction instance
     *  \return GW_SUCCESS if success, otherwise GW_FAILURE
     */
    virtual gw_retval_t disassemble(const uint8_t* bytes, GWInstruction*& instr_instance){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  create an empty instruction instance
     *  \param  instr_instance      output instruction instance
     *  \return GW_SUCCESS if success, otherwise GW_FAILURE
     */
    virtual gw_retval_t create_instruction_shell(GWInstruction*& instr_instance){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }
    /* ==================== Common ==================== */


    /* ==================== Basic ==================== */
 public:
    // name of the instruction
    std::string name = "unknown";
    std::string readable_name = "unknown";

    // byte size of the instruction
    uint32_t instruction_size = 16;
    /* ==================== Basic ==================== */


    /* ==================== OpCode ==================== */
 public:
    // operation code of the instruction
    gw_instruction_opcode_t opcode = 0;
    std::map<std::string, gw_instruction_opcode_t> map_pipe_opcode;

    // bytes of the operation code
    std::vector<uint8_t> opcode_bytes;
    /* ==================== OpCode ==================== */


    /* ==================== Field Map ==================== */
 public:
    /*!
     *  \brief  get field attribute by string of the value (e.g., register name)
     *  \param  value_str   string of the value
     *  \param  field_attr  output field attribute
     *  \return GW_SUCCESS if success, otherwise GW_FAILURE
     */
    gw_retval_t get_field_attr_by_value_str(std::string value_str, GWInstructionFieldAttr*& field_attr) const;


    std::map<std::string, GWInstructionFieldAttr*> map_value_name_to_field_attrs;
    /* ==================== Field Map ==================== */


    /* ==================== Operands ==================== */
 public:
    std::map<std::string, GWOperandDef*> map_operand_defs;
    std::map<std::string, GWOperandDef*> map_modifier_defs;
    /* ==================== Operands ==================== */
};
