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
#include "common/utils/database_sql.hpp"
#include "common/message.hpp"


class GWInternalMessagePayload_Common_DB_SQL_Write final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Common_DB_SQL_Write() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Common_DB_SQL_Write() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["table_name"] = this->table_name;
        object["insert_data"] = this->insert_data;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("table_name"))
            this->table_name = json["table_name"];
        if(json.contains("insert_data"))
            this->insert_data = json["insert_data"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_SQL_WRITE_DB;

    // uri of the resource
    std::string table_name;

    std::map<std::string, nlohmann::json> insert_data;
};


class GWInternalMessagePayload_Common_DB_SQL_Read : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Common_DB_SQL_Read() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Common_DB_SQL_Read() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["query"] = this->query;
        object["query_result"] = this->query_result.serialize();
        object["success"] = this->success;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("query"))
            this->query = json["query"];
        if(json.contains("query_result"))
            this->query_result.deserialize(json["query_result"]);
        if(json.contains("success"))
            this->success = json["success"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_SQL_READ_DB;

    // table name
    std::string query = "";

    // query result
    GWUtilSqlQueryResult query_result;

    // whether the read is success
    bool success = false;
};


class GWInternalMessagePayload_Common_DB_SQL_Stream final : public GWInternalMessagePayload_Common_DB_SQL_Read {
 public:
    GWInternalMessagePayload_Common_DB_SQL_Stream() : GWInternalMessagePayload_Common_DB_SQL_Read() {}
    ~GWInternalMessagePayload_Common_DB_SQL_Stream() = default;

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_SQL_STREAM_DB;
};


class GWInternalMessagePayload_Common_DB_SQL_CreateTable : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Common_DB_SQL_CreateTable() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Common_DB_SQL_CreateTable() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["table_name"] = this->table_name;
        object["schema"] = this->schema;
        object["success"] = this->success;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("table_name"))
            this->table_name = json["table_name"];
        if(json.contains("schema"))
            this->schema = json["schema"];
        if(json.contains("success"))
            this->success = json["success"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_SQL_CREATE_TABLE;

    // table name
    std::string table_name = "";

    // schema to create table
    std::string schema = "";

    // whether the read is success
    bool success = false;
};


class GWInternalMessagePayload_Common_DB_SQL_DropTable : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Common_DB_SQL_DropTable() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Common_DB_SQL_DropTable() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["table_name"] = this->table_name;
        object["success"] = this->success;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("table_name"))
            this->table_name = json["table_name"];
        if(json.contains("success"))
            this->success = json["success"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_SQL_DROP_TABLE;

    // table name
    std::string table_name = "";

    // whether the read is success
    bool success = false;
};


class GWInternalMessagePayload_Common_DB_SQL_Subscribe final : public __GWInternalMessagePayload_Common_SubscribeOrNotDB {
 public:
    GWInternalMessagePayload_Common_DB_SQL_Subscribe() : __GWInternalMessagePayload_Common_SubscribeOrNotDB() {}
    ~GWInternalMessagePayload_Common_DB_SQL_Subscribe() = default;       

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_SQL_SUBSCRIBE_DB;
};


class GWInternalMessagePayload_Common_DB_SQL_Unsubscribe final : public __GWInternalMessagePayload_Common_SubscribeOrNotDB {
 public:
    GWInternalMessagePayload_Common_DB_SQL_Unsubscribe() : __GWInternalMessagePayload_Common_SubscribeOrNotDB() {}
    ~GWInternalMessagePayload_Common_DB_SQL_Unsubscribe() = default;

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_COMMON_SQL_UNSUBSCRIBE_DB;
};
