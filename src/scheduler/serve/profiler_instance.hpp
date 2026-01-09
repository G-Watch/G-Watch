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
 *  \brief  instance of a profiler process
 */
class GWProfilerInstance final : public GWUtilWebSocketInstance {
 public:
    GWProfilerInstance(struct lws *wsi, volatile bool *do_exit);
    ~GWProfilerInstance();
};
