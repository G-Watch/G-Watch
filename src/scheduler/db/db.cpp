#include <iostream>
#include <string>

#include "common/common.hpp"
#include "common/log.hpp"
#include "scheduler/scheduler.hpp"


gw_retval_t GWScheduler::__init_db(){
    gw_retval_t retval = GW_SUCCESS;

    // create all necessary tables in SQL
    GW_IF_FAILED(
        this->__init_sql(),
        retval,
        {
            GW_WARN_C(
                "failed to initialize SQL database: error(%s)", gw_retval_str(retval)
            )
            goto exit;
        }
    );

    // start doing initial 
    GW_IF_FAILED(
        this->__init_vector(),
        retval,
        {
            GW_WARN_C(
                "failed to initialize vector database: error(%s)", gw_retval_str(retval)
            )
            goto exit;
        }
    );

exit:
    return retval;
}
