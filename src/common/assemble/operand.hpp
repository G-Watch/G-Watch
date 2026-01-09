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


class GWOperand {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     */
    GWOperand(const GWOperandDef* def);


    /*!
     *  \brief  constructor
     */
    GWOperand(const GWOperand& other) 
        : value_str(other.value_str),
          is_signed(other.is_signed),
          is_valid(other.is_valid),
          _def(other._def)
    {
        this->value.u64 = other.value.u64;
    }


    /*!
     *  \brief  equator
     */
    GWOperand& operator=(const GWOperand& other) {
        if (this != &other) {
            this->value_str = other.value_str;
            this->is_signed = other.is_signed;
            this->is_valid = other.is_valid;
            this->value.u64 = other.value.u64;
            this->_def = other._def;
        }
        return *this;
    }


    /*!
     *  \brief  destructor
     */
    virtual ~GWOperand();


    /*!
     *  \brief  obtain the readable string of the operand
     *  \param  simply  simply the output format
     *  \return readable string of the operand
     */
    virtual std::string str(bool simply=false){
        throw GWException("unimplemented method");
        return "";
    }


    /*!
     *  \brief  get definition of the current operand instance
     *  \return definition of the current operand instance
     */
    inline const GWOperandDef* get_def(){
        return const_cast<GWOperandDef*>(this->_def);
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


    // value string of the operand
    std::string value_str = "";

    // value of the operand
    bool is_signed = false;
    bool is_valid = false;
    union {
        uint64_t u64;
        int64_t s64;
    } value;

 protected:
    const GWOperandDef* _def;
    /* ==================== Common ==================== */
};
