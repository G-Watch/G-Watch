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
#include "common/assemble/instruction_def.hpp"


// forward declaration
class GWInstructionDef;


/*!
 *  \brief  represent a set of instructions of specific architecture
 */
class GWInstructionSet {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     */
    GWInstructionSet();


    /*!
     *  \brief  destructor
     */
    ~GWInstructionSet();


    /*!
     *  \brief  load instruction details from directory
     *  \param  dir_metadatas   directory that contains all instruction details
     *  \return GW_SUCCESS if success, otherwise GW_FAILURE
     */
    virtual gw_retval_t load_metadata_from_dir(std::string dir_metadatas){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  create an empty instruction instance
     *  \param  instr_instance      output instruction instance
     *  \return GW_SUCCESS if success, otherwise GW_FAILURE
     */
    gw_retval_t create_instruction_shell_by_instruction_name(std::string instr_name, GWInstruction*& instr_instance);


    /*!
     *  \brief  create an instruction instance with specified values
     *  \param  instr_name              name of the instruction
     *  \param  map_unsigned_operand    map of unsigned operand values      <<operand_name, suboperand_type, suboperand_name>, value>
     *  \param  map_signed_operand      map of signed operand values        <<operand_name, suboperand_type, suboperand_name>, value>
     *  \param  map_unsigned_constrain  map of unsigned constrain values    <<operand_name, suboperand_type, suboperand_name, constrain_name>, value>
     *  \param  map_signed_constrain    map of signed constrain values      <<operand_name, suboperand_type, suboperand_name, constrain_name>, value>
     *  \param  map_modifier            map of modifier values              <modifier_name, value>
     *  \param  instr_instance          output instruction instance
     *  \return GW_SUCCESS if success, otherwise GW_FAILURE
     */
    gw_retval_t create_instruction(
        std::string instr_name,
        std::map<std::tuple<std::string, std::string, std::string>, uint64_t> map_unsigned_operand,
        std::map<std::tuple<std::string, std::string, std::string>, int64_t> map_signed_operand,
        std::map<std::tuple<std::string, std::string, std::string, std::string>, uint64_t> map_unsigned_constrain,
        std::map<std::tuple<std::string, std::string, std::string, std::string>, int64_t> map_signed_constrain,
        std::map<std::string, uint64_t> map_modifier,
        GWInstruction*& instr_instance
    );


    /*!
     *  \brief  parse instruction from byte sequence
     *  \param  bytes           byte sequence of instructions
     *  \param  instruction     instruction that be parsed out
     *  \return GW_SUCCESS if success, otherwise GW_FAILURE
     */
    virtual gw_retval_t disassemble(const uint8_t* bytes, GWInstruction*& instruction){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


 protected:
    // instruction map
    std::vector<GWInstructionDef*> _list_instructions;
    std::map<std::string, GWInstructionDef*> _map_name_to_instruction_def;
    std::multimap<gw_instruction_opcode_t, GWInstructionDef*> _map_opcode_to_instructions;

    // directory that contains all instruction details
    std::string _dir_metadatas = "";
    /* ==================== Common ==================== */    
};
