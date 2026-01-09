#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <mutex>

#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/socket.hpp"


/*
 *  \brief  instance of a gTrace terminal
 */
class GWgTraceInstance final : public GWUtilWebSocketInstance {
 public:
    GWgTraceInstance(struct lws *wsi, volatile bool *do_exit);
    ~GWgTraceInstance();
};
