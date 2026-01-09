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
class GWOperand;
class GWInstructionDef;


/*!
 *  \brief  represent an instruction instance
 */
class GWInstruction {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     */
    GWInstruction(GWInstructionDef* def);


    /*!
     *  \brief  destructor
     */
    virtual ~GWInstruction();


    /*!
     *  \brief  equator
     */
    GWInstruction& operator=(const GWInstruction& other);


    /*!
     *  \brief  obtain the readable string of the instruction
     *  \param  flatten whether to flatten the output
     *  \param  simply  simply the output format
     *  \return readable string of the instruction
     */
    virtual std::string str(bool flatten=false, bool simply=false){
        throw GWException("unimplemented method");
        return "";
    }


    /*!
     *  \brief  serialize the instruction
     *  \return serialized result
     */
    virtual nlohmann::json serialize(){
        nlohmann::json object;
        GW_ERROR_C("not implemented");
        return object;
    }


    /*!
     *  \brief  encode instruction instance to byte sequence
     *  \return GW_SUCCESS if success, otherwise GW_FAILURE
     */
    virtual gw_retval_t encode(std::vector<uint8_t> &bytes){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  get definition of the current instruction instance
     *  \return definition of the current instruction instance
     */
    inline GWInstructionDef* get_def(){
        return this->_def;
    }


    // map of operand instances
    std::map<std::string, GWOperand*> map_operands;

    // map of modifier instances
    std::map<std::string, GWOperand*> map_modifier;

    // byte sequence of the instruction
    std::vector<uint8_t> bytes;


    /*!
     *  \brief set the operand / suboperand of the instruction
     *  \param opname           name of the operand
     *  \param suboperand_type  type of the suboperand
     *  \param suboperand_name  name of the suboperand
     *  \param value            value of the operand
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t set_operand_unsigned(
        std::string opname, std::string suboperand_type="", std::string suboperand_name="", uint64_t value = 0
    ){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }
    virtual gw_retval_t set_operand_signed(
        std::string opname, std::string suboperand_type="", std::string suboperand_name="", int64_t value = 0
    ){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }
    

    /*!
     *  \brief set the constraint of the instruction
     *  \param opname           name of the operand
     *  \param suboperand_type  type of the suboperand
     *  \param suboperand_name  name of the suboperand
     *  \param constrain_name   name of the constraint
     *  \param value            value of the constraint
     */
    virtual gw_retval_t set_constrain_unsigned(
        std::string opname, std::string suboperand_type="", std::string suboperand_name="", std::string constrain_name="", uint64_t value = 0
    ){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }
    virtual gw_retval_t set_constrain_signed(
        std::string opname, std::string suboperand_type="", std::string suboperand_name="", std::string constrain_name="", int64_t value = 0
    ){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief set the modifier of the instruction
     *  \param mfname   name of the modifier
     *  \param value    value of the modifier
     */
    virtual gw_retval_t set_modifier(std::string mfname, uint64_t value){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    // map of used register operands in this instruction: <reg_type, <register_operand>>
    std::map<std::string, std::set<GWOperand*>> map_register_operands = {};

    // map of IN/OUT register set: <reg_type, <reg_idx>>
    std::map<std::string, std::set<uint64_t>> map_register_set_IN = {};
    std::map<std::string, std::set<uint64_t>> map_register_set_OUT = {};

 protected:
    // definition of the current instruction instance
    GWInstructionDef* _def = nullptr;


    /*!
     *  \brief  clone operand from a old one
     *  \param  operand     operand to be cloned
     *  \return cloned operand
     */
    virtual GWOperand* __clone_operand(const GWOperand* operand) const;
    /* ==================== Common ==================== */
};
