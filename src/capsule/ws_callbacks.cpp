#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <string>
#include <cstring>
#include <filesystem>
#include <queue>
#include <atomic>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <numa.h>
#include <libwebsockets.h>


#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/system.hpp"
#include "common/utils/string.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/socket.hpp"
#include "common/utils/queue.hpp"
#include "capsule/event.hpp"
#include "capsule/capsule.hpp"
#include "capsule/metric.hpp"
#include "scheduler/serve/capsule_message.hpp"


void GWCapsule::__process_capsule_resp_REGISTER(GWInternalMessage_Capsule *message){
    gw_retval_t tmp_retval;
    GWInternalMessagePayload_Capsule_Register *payload;

    payload = message->get_payload_ptr<GWInternalMessagePayload_Capsule_Register>(GW_MESSAGE_TYPEID_CAPSULE_REGISTER);
    GW_CHECK_POINTER(payload);

    // detect capsule info
    GW_IF_FAILED(
        this->__detect_capsule(payload->capsule_info),
        tmp_retval,
        GW_WARN_C("failed to detect capsule info");
    );

    // detect cpu topology
    GW_IF_FAILED(
        this->__detect_cpu_topology(payload->cpu_info),
        tmp_retval,
        GW_WARN_C("failed to detect cpu topology");
    );

    // detect gpu tolopogy
    GW_IF_FAILED(
        this->__detect_gpu_topology(payload->list_gpu_info),
        tmp_retval,
        GW_WARN_C("failed to detect gpu topology");
    );

    if(likely(tmp_retval == GW_SUCCESS)){
        payload->success = true;
    }

    // send back registration response
    GW_IF_FAILED(
        this->send_to_scheduler(message),
        tmp_retval,
        {
            GW_WARN_C("failed to send registration response");
            goto exit;
        }
    );

exit:
    ;
}
