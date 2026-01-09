#include <iostream>

#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/instrument.hpp"
#include "common/utils/string.hpp"
#include "common/assemble/instruction.hpp"
#include "common/assemble/kernel_def.hpp"
#include "common/assemble/kernel.hpp"


GWInstrumentRegAllocCxt::GWInstrumentRegAllocCxt(GWKernel* kernel)
    : _kernel(kernel)
{
    GW_CHECK_POINTER(this->_kernel_def = _kernel->get_def());
}


GWInstrumentRegAllocCxt::~GWInstrumentRegAllocCxt()
{}


gw_retval_t GWInstrumentRegAllocCxt::alloc_extra(std::string type, uint64_t& reg_id){
    gw_retval_t retval = GW_SUCCESS;
    std::vector<uint64_t> list_reg_idx;

    retval = this->alloc_extra(type, 1, list_reg_idx);
    if(retval == GW_SUCCESS){
        GW_ASSERT(list_reg_idx.size() == 1);
        reg_id = list_reg_idx[0];
    }

exit:
    return retval;
}


uint64_t GWInstrumentRegAllocCxt::get_nb_used_reg(std::string type){
    gw_retval_t retval = GW_SUCCESS;
    uint64_t nb_used_reg = 0;
    
    if(unlikely(this->_map_reg_op.find(type) == this->_map_reg_op.end())){
        nb_used_reg = 0;
    } else {
        nb_used_reg = this->_map_reg_op[type].size();
    }
    
    return nb_used_reg;
}


GWInstrumentCxt::GWInstrumentCxt(const GWTraceTask* trace_task, GWKernel* kernel)
    : _trace_task(trace_task), _kernel(kernel)
{}


GWInstrumentCxt::~GWInstrumentCxt()
{
    if(this->_reg_alloc_cxt != nullptr){
        delete this->_reg_alloc_cxt;
    }
}
