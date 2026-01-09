#include <iostream>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/string.hpp"
#include "scheduler/scheduler.hpp"
#include "scheduler/serve/profiler_instance.hpp"
#include "scheduler/serve/profiler_message.hpp"


gw_retval_t GWScheduler::__cb_profiler_write_db(
    const std::string& uri, const nlohmann::json& new_val, const nlohmann::json& old_val, void* user_data
){
    gw_retval_t retval = GW_SUCCESS;
    GWScheduler *scheduler = nullptr;

    GW_CHECK_POINTER(scheduler = (GWScheduler*)(user_data));

exit:
    return retval;
}
