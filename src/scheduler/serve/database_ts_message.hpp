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


class GWInternalMessagePayload_Common_DB_TS_Write final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Common_DB_TS_Write() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Common_DB_TS_Write() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["uri"] = this->uri;
        object["index"] = this->index;
        object["timestamp"] = this->timestamp;
        object["end_timestamp"] = this->end_timestamp;
        object["payload"] = this->payload;
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

        if(json.contains("index"))
            this->index = json["index"];

        if(json.contains("timestamp"))
            this->timestamp = json["timestamp"];

        if(json.contains("end_timestamp"))
            this->end_timestamp = json["end_timestamp"];

        if(json.contains("payload"))
            this->payload = json["payload"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_TS_WRITE_DB;

    // uri of the resource
    std::string uri = "";

    // index of the time series sample
    uint64_t index = 0;
    
    // timestamp of the time series sample
    uint64_t timestamp = 0;

    // end timestamp of the time series sample
    uint64_t end_timestamp = 0;

    // payload 
    nlohmann::json payload = "";
};


class GWInternalMessagePayload_Common_DB_TS_Read : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Common_DB_TS_Read() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Common_DB_TS_Read() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["uri"] = this->uri;
        object["index"] = this->index;
        object["timestamp"] = this->timestamp;
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
        if(json.contains("index"))
            this->index = json["index"];
        if(json.contains("timestamp"))
            this->timestamp = json["timestamp"];
        if(json.contains("read_payload"))
            this->read_payload = json["read_payload"];
        if(json.contains("success"))
            this->success = json["success"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_TS_READ_DB;

    // uri of the resource
    std::string uri;
    
    // index of the record
    uint64_t index = 0;
    
    // timestamp of the record
    uint64_t timestamp = 0;
    
    // payload 
    nlohmann::json read_payload;

    // whether the read is success
    bool success = false;
};


class GWInternalMessagePayload_Common_DB_TS_Stream final : public GWInternalMessagePayload_Common_DB_TS_Read {
 public:
    GWInternalMessagePayload_Common_DB_TS_Stream() : GWInternalMessagePayload_Common_DB_TS_Read() {}
    ~GWInternalMessagePayload_Common_DB_TS_Stream() = default;

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_TS_STREAM_DB;
};


class GWInternalMessagePayload_Common_DB_TS_Subscribe final : public __GWInternalMessagePayload_Common_SubscribeOrNotDB {
 public:
    GWInternalMessagePayload_Common_DB_TS_Subscribe() : __GWInternalMessagePayload_Common_SubscribeOrNotDB() {}
    ~GWInternalMessagePayload_Common_DB_TS_Subscribe() = default;

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_TS_SUBSCRIBE_DB;
};


class GWInternalMessagePayload_Common_DB_TS_Unsubscribe final : public __GWInternalMessagePayload_Common_SubscribeOrNotDB {
 public:
    GWInternalMessagePayload_Common_DB_TS_Unsubscribe() : __GWInternalMessagePayload_Common_SubscribeOrNotDB() {}
    ~GWInternalMessagePayload_Common_DB_TS_Unsubscribe() = default;

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_TS_UNSUBSCRIBE_DB;
};
