#include <iostream>
#include <vector>
#include <regex>
#include <format>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/instrument.hpp"
#include "common/assemble/kernel_def.hpp"
#include "capsule/trace.hpp"


GWTraceTask::GWTraceTask()
{}


GWTraceTask::~GWTraceTask()
{}


bool GWTraceTask::do_need_trace(nlohmann::json query) const {
    bool retval = false;
    std::smatch m;
    std::regex pattern;
    std::string kernel_name = "";
    std::string pattern_str = "";

    // check whether filtering rule exists
    if(unlikely(this->_map_metadata.find("match_pattern") == this->_map_metadata.end())){
        goto exit;
    }

    pattern_str = this->_map_metadata.at("match_pattern");
    pattern = std::regex(pattern_str);

    kernel_name = query.get<std::string>();
    if (std::regex_search(kernel_name, m, pattern)){
        retval = true;
    } else {
        retval = false;
    }

exit:
    return retval;
}


gw_retval_t GWTraceTask::__execute(
    GWCapsule* capsule,
    GWKernel *kernel,
    std::string global_id,
    std::vector<std::string> map_instrument_ctx_name,
    std::map<std::string, GWInstrumentCxt*>& map_existing_instrument_ctx,
    std::map<std::string, GWInstrumentCxt*>& map_current_instrument_ctx
) const {
    gw_retval_t retval = GW_SUCCESS;
    uint64_t i = 0, subtrace_id = 0;
    std::string instrument_ctx_name = "";
    GWInstrumentCxt *instrument_cxt = nullptr;

    GW_CHECK_POINTER(capsule);
    GW_CHECK_POINTER(kernel);

    for(i=0; i<map_instrument_ctx_name.size(); i++){
        instrument_ctx_name = map_instrument_ctx_name[i];
        if(map_existing_instrument_ctx.find(instrument_ctx_name) == map_existing_instrument_ctx.end()){
            // create the instrument context
            instrument_cxt = GWInstrumentCxtFactory::instance().create(instrument_ctx_name, this, kernel);
            GW_CHECK_POINTER(instrument_cxt);
            instrument_cxt->global_id = std::format("{}-childtrace[{}]", global_id, subtrace_id);

            // execute the instrument context
            GW_IF_FAILED(
                this->__execute_instrument_cxt(capsule, instrument_cxt),
                retval,
                {
                    GW_WARN_C(
                        "failed to execute instrumentation context: instruementation(%s), kernel(%s), error(%s)",
                        instrument_ctx_name.c_str(),
                        kernel->get_def()->mangled_prototype.c_str(),
                        gw_retval_str(retval)
                    );
                    map_current_instrument_ctx.insert({ instrument_ctx_name, instrument_cxt });
                    goto exit;
                }
            )
        }
        subtrace_id += 1;
        map_current_instrument_ctx.insert({ instrument_ctx_name, instrument_cxt });
    }

exit:
    return retval;
}
