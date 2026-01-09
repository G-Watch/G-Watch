#pragma once

#include <iostream>
#include <vector>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/instrument.hpp"
#include "capsule/capsule.hpp"
#include "capsule/cuda_impl/trace.hpp"


// forward declaration
class GWTraceTask_CUDA_Kernel_BlockSchedule;


// register trace task creator
namespace {
    const bool __register_trace_task_creator = []() {
        GWTraceTaskFactory::instance().register_type<GWTraceTask_CUDA_Kernel_BlockSchedule>("kernel:block_schedule");
        GW_DEBUG("registered trace task creator: kernel:block_schedule");        
        return true;
    }();
}


class GWTraceTask_CUDA_Kernel_BlockSchedule final : public GWTraceTask_CUDA {
 public:
    /*!
     *  \brief  constructor
     */
    GWTraceTask_CUDA_Kernel_BlockSchedule();


    /*!
     *  \brief  destructor
     */
    ~GWTraceTask_CUDA_Kernel_BlockSchedule();


    /*!
     *  \brief  execute the trace application
     *  \param  capsule                         capsule to execute the trace
     *  \param  global_id                       global_id of the trace task instance
     *  \param  kernel                          kernel to be executed
     *  \param  map_existing_instrument_ctx     map of existing instrument context
     *  \param  map_current_instrument_ctx      map of new instrument context
     *  \return GW_SUCCESS for successful execution
     */
    gw_retval_t execute(
        GWCapsule* capsule, std::string global_id, GWKernel *kernel,
        std::map<std::string, GWInstrumentCxt*>& map_existing_instrument_ctx,
        std::map<std::string, GWInstrumentCxt*>& map_current_instrument_ctx
    ) const override;


    /*!
     *  \brief  serialize the event
     *  \return serialized string
     */
    nlohmann::json serialize(
        std::string global_id,
        std::map<std::string, GWInstrumentCxt*>& map_instrument_ctx
    ) const override;


 private:
};


GWTraceTask_CUDA_Kernel_BlockSchedule::GWTraceTask_CUDA_Kernel_BlockSchedule()
    : GWTraceTask_CUDA()
{
    this->_type = "kernel:block_schedule";
}


GWTraceTask_CUDA_Kernel_BlockSchedule::~GWTraceTask_CUDA_Kernel_BlockSchedule()
{}


gw_retval_t GWTraceTask_CUDA_Kernel_BlockSchedule::execute(
    GWCapsule* capsule, std::string global_id, GWKernel *kernel,
    std::map<std::string, GWInstrumentCxt*>& map_existing_instrument_ctx,
    std::map<std::string, GWInstrumentCxt*>& map_current_instrument_ctx
) const {
    gw_retval_t retval = GW_SUCCESS;
    std::vector<std::string> list_instrument_ctx_name = { 
        "sass::count_control_flow"
    };
    return this->__execute(
        capsule, kernel, global_id, list_instrument_ctx_name, map_existing_instrument_ctx, map_current_instrument_ctx
    );
}


nlohmann::json GWTraceTask_CUDA_Kernel_BlockSchedule::serialize(
    std::string global_id,
    std::map<std::string, GWInstrumentCxt*>& map_instrument_ctx
) const {
    nlohmann::json object;

    object["metadata"] = this->_map_metadata;

    // TODO: generate trace result

    return object;
}
