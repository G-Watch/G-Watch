#pragma once

#include <iostream>
#include <map>
#include <any>
#include <string>

#include <stdint.h>
#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/timer.hpp"
#include "common/message.hpp"


class GWInternalMessagePayload_Common_PingPong final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Common_PingPong() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Common_PingPong() = default;
    
    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        return object;
    }

    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {}

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_PINGPONG;
};


class GWInternalMessagePayload_Common_Heartbeat final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Common_Heartbeat() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Common_Heartbeat() = default;

    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        return object;
    }

    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {}

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_HEARTBEAT;
};
