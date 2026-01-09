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



class GWInternalMessagePayload_Common_DB_KV_Write final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Common_DB_KV_Write() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Common_DB_KV_Write() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["uri"] = this->uri;
        object["write_payload"] = this->write_payload;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("uri"))
            this->uri = json["uri"];
        if(json.contains("write_payload"))
            this->write_payload = json["write_payload"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB;

    // uri of the resource
    std::string uri;

    // payload 
    nlohmann::json write_payload;
};


class GWInternalMessagePayload_Common_DB_KV_Read : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Common_DB_KV_Read() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Common_DB_KV_Read() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["uri"] = this->uri;
        object["read_payload"] = this->read_payload;
        object["success"] = this->success;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("uri"))
            this->uri = json["uri"];
        if(json.contains("read_payload"))
            this->read_payload = json["read_payload"];
        if(json.contains("success"))
            this->success = json["success"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_KV_READ_DB;

    // uri of the resource
    std::string uri;

    // payload 
    nlohmann::json read_payload;

    // whether the read is success
    bool success = false;
};


class GWInternalMessagePayload_Common_DB_KV_Stream final : public GWInternalMessagePayload_Common_DB_KV_Read {
 public:
    GWInternalMessagePayload_Common_DB_KV_Stream() : GWInternalMessagePayload_Common_DB_KV_Read() {}
    ~GWInternalMessagePayload_Common_DB_KV_Stream() = default;

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_KV_STREAM_DB;
};

class GWInternalMessagePayload_Common_DB_KV_Subscribe final : public __GWInternalMessagePayload_Common_SubscribeOrNotDB {
 public:
    GWInternalMessagePayload_Common_DB_KV_Subscribe() : __GWInternalMessagePayload_Common_SubscribeOrNotDB() {}
    ~GWInternalMessagePayload_Common_DB_KV_Subscribe() = default;       

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_KV_SUBSCRIBE_DB;
};


class GWInternalMessagePayload_Common_DB_KV_Unsubscribe final : public __GWInternalMessagePayload_Common_SubscribeOrNotDB {
 public:
    GWInternalMessagePayload_Common_DB_KV_Unsubscribe() : __GWInternalMessagePayload_Common_SubscribeOrNotDB() {}
    ~GWInternalMessagePayload_Common_DB_KV_Unsubscribe() = default;

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_KV_UNSUBSCRIBE_DB;
};
