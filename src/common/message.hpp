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


using gw_message_typeid_t = uint32_t;


enum :gw_message_typeid_t {
    GW_MESSAGE_TYPEID_UNKNOWN = 0,

    // utilities
    GW_MESSAGE_TYPEID_COMMON_PINGPONG = 10,
    GW_MESSAGE_TYPEID_COMMON_HEARTBEAT,

    // key-value databse
    GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB = 20,
    GW_MESSAGE_TYPEID_COMMON_KV_READ_DB,
    GW_MESSAGE_TYPEID_COMMON_KV_STREAM_DB,
    GW_MESSAGE_TYPEID_COMMON_KV_SUBSCRIBE_DB,
    GW_MESSAGE_TYPEID_COMMON_KV_UNSUBSCRIBE_DB,

    // time-series database
    GW_MESSAGE_TYPEID_COMMON_TS_WRITE_DB = 30,
    GW_MESSAGE_TYPEID_COMMON_TS_READ_DB,
    GW_MESSAGE_TYPEID_COMMON_TS_STREAM_DB,
    GW_MESSAGE_TYPEID_COMMON_TS_SUBSCRIBE_DB,
    GW_MESSAGE_TYPEID_COMMON_TS_UNSUBSCRIBE_DB,

    // sql database
    GW_MESSAGE_TYPEID_COMMON_SQL_WRITE_DB = 40,
    GW_MESSAGE_TYPEID_COMMON_SQL_READ_DB,
    GW_MESSAGE_TYPEID_COMMON_SQL_STREAM_DB,
    GW_MESSAGE_TYPEID_COMMON_SQL_CREATE_TABLE,
    GW_MESSAGE_TYPEID_COMMON_SQL_DROP_TABLE,
    GW_MESSAGE_TYPEID_COMMON_SQL_SUBSCRIBE_DB,
    GW_MESSAGE_TYPEID_COMMON_SQL_UNSUBSCRIBE_DB,
};


/*!
 *  \brief  internal message payload
 */
class GWInternalMessagePayload {
 public:
    GWInternalMessagePayload(){}
    ~GWInternalMessagePayload(){}


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    virtual nlohmann::json serialize(){
        GW_ERROR_C_DETAIL("not implemented");
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    virtual void deserialize(const nlohmann::json& json){
        GW_ERROR_C_DETAIL("not implemented");
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_UNKNOWN; 
};


/*!
 *  \brief  internal message using across scheduler and profiler/capsule/gtrace
 */
template<typename... PayloadTypes>
class GWInternalMessage {
 public:
    /*!
     *  \brief  constructor
     */
    GWInternalMessage() : type_id(GW_MESSAGE_TYPEID_UNKNOWN), ref_id("") {
        (this->_map_payload.emplace(PayloadTypes::msg_typeid, new PayloadTypes()), ...);
    }


    /*!
     *  \brief  destructor
     */
    ~GWInternalMessage() {
        auto delete_handler = [this]<typename T>() {
            const uint32_t type_id = T::msg_typeid;
            auto it = _map_payload.find(type_id);
            if (it != _map_payload.end()) {
                try {
                    delete std::any_cast<T*>(it->second);
                } catch (std::exception& e) {
                    GW_ERROR_DETAIL("failed to any_cast: %s", e.what());
                }
            }
        };
        (delete_handler.template operator()<PayloadTypes>(), ...);
    }


    /*!
     *  \brief  serialize the message to json string
     *  \return json string
     */
    virtual std::string serialize() {
        nlohmann::json object;
        object["type_id"] = this->type_id;
        object["ref_id"] = this->ref_id;

        if (unlikely(_map_payload.find(type_id) == _map_payload.end())) {
            GW_ERROR_C_DETAIL("unknown message type: type_id(%d)", type_id);
            return "";
        }

        nlohmann::json payload_json;
        auto try_cast = [&]<typename T>() -> bool {
            if (T::msg_typeid == type_id) {
                try {
                    T* ptr = std::any_cast<T*>(_map_payload[type_id]);
                    payload_json = ptr->serialize();
                } catch (std::exception& e) {
                    GW_ERROR_DETAIL("failed to any_cast: %s", e.what());
                }
                return true;
            }
            return false;
        };

        bool matched = (try_cast.template operator()<PayloadTypes>() || ...);
        if (unlikely(!matched)) {
            GW_ERROR_C_DETAIL("type_id(%d) has no registered payload type", type_id);
        }

        object["payload"] = payload_json;
        return object.dump();
    }


    /*!
     *  \brief  deserialize the message from json string
     *  \param  json string
     *  \return GW_SUCCESS for successfully deserialized
     */
    virtual gw_retval_t deserialize(const std::string& json) {
        nlohmann::json object;
        gw_retval_t retval = GW_SUCCESS;

        try {
            object = nlohmann::json::parse(json);
        } catch (std::exception& e) {
            GW_WARN_C("failed to parse json string: %s", e.what());
            return GW_FAILED_INVALID_INPUT;
        }

        this->type_id = object["type_id"];
        this->ref_id = object["ref_id"];
        if (unlikely(_map_payload.find(type_id) == _map_payload.end())) {
            GW_ERROR_C_DETAIL("unknown message type: type_id(%d)", type_id);
            return GW_FAILED_INVALID_INPUT;
        }

        bool deserialized = false;
        auto try_deserialize = [&]<typename T>() -> bool {
            if (T::msg_typeid == type_id) {
                try {
                    T* ptr = std::any_cast<T*>(_map_payload[type_id]);
                    ptr->deserialize(object["payload"]);
                    deserialized = true;
                } catch (std::exception& e) {
                    GW_ERROR_DETAIL("failed to any_cast: %s", e.what());
                }
                return true;
            }
            return false;
        };

        (try_deserialize.template operator()<PayloadTypes>() || ...);
        if (unlikely(!deserialized)) {
            GW_ERROR_C_DETAIL("type_id(%d) has no registered payload type", type_id);
        }

        return retval;
    }


    /*!
     *  \brief  load message from another message
     *  \param  other   other message
     */
    inline void load_from_other(GWInternalMessage<PayloadTypes...>* other) {
        this->type_id = other->type_id;
        this->ref_id = other->ref_id;

        auto copy_payload = [&](gw_message_typeid_t type_id) {
            auto src_it = other->_map_payload.find(type_id);
            if (src_it == other->_map_payload.end()) return;

            auto dst_it = this->_map_payload.find(type_id);
            if (dst_it == this->_map_payload.end()) {
                GW_ERROR_C("type_id(%u) not found in destination message", type_id);
            }

            auto try_copy = [&]<typename T>() -> bool {
                if (T::msg_typeid == type_id) {
                    try {
                        T* src_ptr = std::any_cast<T*>(src_it->second);
                        T* dst_ptr = std::any_cast<T*>(dst_it->second);
                        dst_ptr->deserialize(src_ptr->serialize());
                    } catch (std::exception& e) {
                        GW_ERROR_DETAIL("failed to any_cast: %s", e.what());
                    }
                    return true;
                }
                return false;
            };

            bool matched = (try_copy.template operator()<PayloadTypes>() || ...);
            if (unlikely(!matched)) {
                // GW_ERROR_C("type_id(%u) has no registered payload type", type_id);
            }
        };

        for (auto& payload_pair : other->_map_payload) {
            copy_payload(payload_pair.first);
        }
    }


    /*!
     *  \brief  get the payload of the message
     *  \tparam T       type of the payload
     *  \param  type_id type id of the payload
     *  \return payload of the message
     */
    template<typename T>
    T* get_payload_ptr(gw_message_typeid_t type_id) {
        T* retval = nullptr;

        if(unlikely(this->_map_payload.find(type_id) == this->_map_payload.end())){
            GW_ERROR_C_DETAIL("unknown message type: type_id(%d)", type_id);
        }
        
        try {
            retval = std::any_cast<T*>(this->_map_payload[type_id]);
        } catch (std::exception& e) {
            GW_ERROR_DETAIL("failed to any_cast: %s", e.what());
        }

        return retval;
    }


    // type id of the message
    gw_message_typeid_t type_id;

    // ref id of the message
    std::string ref_id = "";

 protected:
    // map of payloads
    std::map<gw_message_typeid_t, std::any> _map_payload;
};


class __GWInternalMessagePayload_Common_SubscribeOrNotDB : public GWInternalMessagePayload {
 public:
    __GWInternalMessagePayload_Common_SubscribeOrNotDB() : GWInternalMessagePayload() {}
    ~__GWInternalMessagePayload_Common_SubscribeOrNotDB() = default;

    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["uri"] = this->uri;
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
        if(json.contains("success"))
            this->success = json["success"];
    }

    // subscribe URI
    std::string uri = "";
    
    // mark whether the subcribe is success
    bool success = false;
};


#include "scheduler/serve/database_kv_message.hpp"
#include "scheduler/serve/database_ts_message.hpp"
#include "scheduler/serve/database_sql_message.hpp"
#include "scheduler/serve/util_message.hpp"
