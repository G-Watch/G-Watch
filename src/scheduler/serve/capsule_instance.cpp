#include <iostream>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <libwebsockets.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/system.hpp"
#include "common/utils/string.hpp"
#include "common/utils/exception.hpp"
#include "scheduler/scheduler.hpp"
#include "scheduler/serve/capsule_instance.hpp"


GWCapsuleInstance::GWCapsuleInstance(struct lws *wsi, volatile bool *do_exit)
    :GWUtilWebSocketInstance(wsi, do_exit) {}


GWCapsuleInstance::~GWCapsuleInstance()
{}
