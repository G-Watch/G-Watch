#pragma once

#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>
#include <libwebsockets.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/socket.hpp"


/*!
 *  \brief  instance of a running capsule 
 */
class GWCapsuleInstance : public GWUtilWebSocketInstance {
 public:
    GWCapsuleInstance(struct lws *wsi, volatile bool *do_exit);
    ~GWCapsuleInstance();

    // global id of the capsule instance [gw_capsule_info_t::global_id]
    std::string global_id = "";
};
