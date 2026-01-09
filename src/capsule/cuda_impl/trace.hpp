#pragma once

#include <iostream>
#include <vector>

#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/instrument.hpp"
#include "capsule/capsule.hpp"
#include "capsule/trace.hpp"


class GWTraceTask_CUDA : public GWTraceTask {
 public:
    /*!
     *  \brief  constructor
     */
    GWTraceTask_CUDA();


    /*!
     *  \brief  destructor
     */
    ~GWTraceTask_CUDA();


 protected:
    /*!
     *  \brief  execute one instrument context
     *  \param  capsule             capsule to execute the trace
     *  \param  instrument_cxt      instrument context to be executed
     *  \return GW_SUCCESS for      successful execution
     */
    gw_retval_t __execute_instrument_cxt(GWCapsule* capsule, GWInstrumentCxt* instrument_cxt) const override;
};
