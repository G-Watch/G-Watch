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


/*!
 *  \brief  represent an operand definition
 */
class GWOperandDef {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     */
    GWOperandDef();


    /*!
     *  \brief  destructor
     */
    virtual ~GWOperandDef();


    /*!
     *  \brief  load operand definition
     *  \param  proto   proto object to be deserialize
     *  \return GW_SUCCESS if success, otherwise GW_FAILURE
     */
    virtual gw_retval_t from_proto(std::istream& proto){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    // metadatas
    std::string type = "";
    std::string name = "";
    std::string optype = ""; // 'r', 'w', 'rw', ''
    bool is_imm = false;
    bool is_imm_signed = false;
    uint32_t imm_bitlen = 0;
    /* ==================== Common ==================== */
};
