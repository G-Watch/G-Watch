#pragma once
#include <iostream>
#include <map>
#include <any>
#include <string>

#include <stdint.h>
#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/message.hpp"
#include "scheduler/agent/context.hpp"


/*!
 *  \brief  message type for communication between scheduler and gtrace
 */
enum :gw_message_typeid_t {
    // agent admin
    GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_CONTEXT = 100,
    GW_MESSAGE_TYPEID_GTRACE_AGENT_DESTORY_CONTEXT = 101,
    // agent user msg
    GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_TASK = 110,
    GW_MESSAGE_TYPEID_GTRACE_AGENT_DESTORY_TASK,
    // agent msg
    GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_CONTENT = 120,
    GW_MESSAGE_TYPEID_GTRACE_AGENT_STREAM,
};


class GWInternalMessagePayload_GTrace_AgentCreateContext final : public GWInternalMessagePayload {
 public:    
    GWInternalMessagePayload_GTrace_AgentCreateContext() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_GTrace_AgentCreateContext() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["context_global_id"] = this->context_global_id;
        object["user_config"] = this->user_config;
        object["is_success"] = this->is_success;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("context_global_id"))
            this->context_global_id = json["context_global_id"];

        if(json.contains("user_config"))
            this->user_config = json["user_config"];

        if(json.contains("is_success"))
            this->is_success = json["is_success"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_CONTEXT;

    // id of the context
    std::string context_global_id = "";

    // config of the context
    gw_agent_context_userconfig_t user_config;

    // whether the create success
    bool is_success = false;
};


class GWInternalMessagePayload_GTrace_AgentDestoryContext final : public GWInternalMessagePayload {
 public:    
    GWInternalMessagePayload_GTrace_AgentDestoryContext() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_GTrace_AgentDestoryContext() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["context_global_id"] = this->context_global_id;
        object["is_success"] = this->is_success;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("context_global_id"))
            this->context_global_id = json["context_global_id"];

        if(json.contains("is_success"))
            this->is_success = json["is_success"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_GTRACE_AGENT_DESTORY_CONTEXT;

    // id of the context
    std::string context_global_id = "";

    // whether the destroy success
    bool is_success = false;
};


class GWInternalMessagePayload_GTrace_AgentCreateTask final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_GTrace_AgentCreateTask() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_GTrace_AgentCreateTask() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["context_global_id"] = this->context_global_id;
        object["list_parent_task_global_idx"] = this->list_parent_task_global_idx;
        object["task_global_id"] = this->task_global_id;
        object["is_success"] = this->is_success;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("context_global_id"))
            this->context_global_id = json["context_global_id"];

        if(json.contains("list_parent_task_global_idx"))
            this->list_parent_task_global_idx = json["list_parent_task_global_idx"].get<std::vector<std::string>>();

        if(json.contains("task_global_id"))
            this->task_global_id = json["task_global_id"];

        if(json.contains("is_success"))
            this->is_success = json["is_success"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_TASK;

    // id of the context
    std::string context_global_id = "";

    // list of parent nodes
    std::vector<std::string> list_parent_task_global_idx = {};

    // created task id
    std::string task_global_id = "";

    // whether the prompt success
    bool is_success = false;
};


class GWInternalMessagePayload_GTrace_AgentDestoryTask final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_GTrace_AgentDestoryTask() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_GTrace_AgentDestoryTask() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["context_global_id"] = this->context_global_id;
        object["task_global_id"] = this->task_global_id;
        object["is_success"] = this->is_success;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("context_global_id"))
            this->context_global_id = json["context_global_id"];

        if(json.contains("task_global_id"))
            this->task_global_id = json["task_global_id"];

        if(json.contains("is_success"))
            this->is_success = json["is_success"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_GTRACE_AGENT_DESTORY_TASK;

    // id of the context
    std::string context_global_id = "";

    // id of the task to be destoried
    std::string task_global_id = "";

    // whether the destory success
    bool is_success = false;
};


class GWInternalMessagePayload_GTrace_AgentCreateContent final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_GTrace_AgentCreateContent() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_GTrace_AgentCreateContent() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["context_global_id"] = this->context_global_id;
        object["task_global_id"] = this->task_global_id;
        object["content_global_id"] = this->content_global_id;
        object["content_typeid"] = this->content_typeid;
        object["do_execute"] = this->do_execute;
        object["is_success"] = this->is_success;

        /* ====================== user prompt ====================== */
        object["body"] = this->body;
        object["list_github_bodies"] = this->list_github_bodies;
        /* ====================== user prompt ====================== */
    
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("context_global_id"))
            this->context_global_id = json["context_global_id"];

        if(json.contains("task_global_id"))
            this->task_global_id = json["task_global_id"];

        if(json.contains("content_global_id"))
            this->content_global_id = json["content_global_id"];

        if(json.contains("content_typeid"))
            this->content_typeid = json["content_typeid"];

        if(json.contains("do_execute"))
            this->do_execute = json["do_execute"];

        if(json.contains("is_success"))
            this->is_success = json["is_success"];

        /* ====================== user prompt ====================== */
        if(json.contains("body"))
            this->body = json["body"];
    
        if(json.contains("list_github_bodies"))
            this->list_github_bodies = json["list_github_bodies"];
        /* ====================== user prompt ====================== */
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_CONTENT;

    // id of the context
    std::string context_global_id = "";

    // id of the task
    std::string task_global_id = "";

    // type of the created content
    gw_agent_content_typeid_t content_typeid = GW_AGENT_CONTENT_TYPEID_UNKNOWN;

    // id of the created content
    std::string content_global_id = "";

    // whether to do execution after creation
    bool do_execute = false;

    // whether the create success
    bool is_success = false;

    /* ====================== user prompt ====================== */
    // prompt body
    std::string body = "";

    // list of github bodies
    std::vector<std::string> list_github_bodies = {};
    /* ====================== user prompt ====================== */
};


class GWInternalMessagePayload_GTrace_AgentStream final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_GTrace_AgentStream() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_GTrace_AgentStream() = default;


    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["map_reasoning_output"] = this->map_reasoning_output;
        object["map_non_reasoning_output"] = this->map_non_reasoning_output;
        object["content_global_id"] = this->content_global_id;
        object["is_success"] = this->is_success;
        object["is_end"] = this->is_end;
        return object;
    }


    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("map_reasoning_output"))
            this->map_reasoning_output = json["map_reasoning_output"];

        if(json.contains("map_non_reasoning_output"))
            this->map_non_reasoning_output = json["map_non_reasoning_output"];

        if(json.contains("content_global_id"))
            this->content_global_id = json["content_global_id"];

        if(json.contains("is_success"))
            this->is_success = json["is_success"];

        if(json.contains("is_end"))
            this->is_end = json["is_end"];
    }


    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_GTRACE_AGENT_STREAM;

    // global id of the content
    std::string content_global_id = "";

    // output of the streaming
    std::map<int, std::string> map_reasoning_output;
    std::map<int, std::string> map_non_reasoning_output;

    // whether the streaming success
    bool is_success = false;

    // whether the streaming is end
    bool is_end = false;
};


class GWInternalMessage_gTrace final : public GWInternalMessage<
    GWInternalMessagePayload_Common_PingPong,
    GWInternalMessagePayload_Common_DB_KV_Write,
    GWInternalMessagePayload_Common_DB_KV_Read,
    GWInternalMessagePayload_Common_DB_TS_Stream,
    GWInternalMessagePayload_Common_DB_TS_Subscribe,
    GWInternalMessagePayload_Common_DB_TS_Unsubscribe,
    GWInternalMessagePayload_Common_DB_SQL_Read,
    GWInternalMessagePayload_Common_DB_SQL_Stream,
    GWInternalMessagePayload_Common_DB_SQL_Subscribe,
    GWInternalMessagePayload_Common_DB_SQL_Unsubscribe,
    GWInternalMessagePayload_Common_Heartbeat,
    GWInternalMessagePayload_GTrace_AgentCreateContext,
    GWInternalMessagePayload_GTrace_AgentDestoryContext,
    GWInternalMessagePayload_GTrace_AgentCreateTask,
    GWInternalMessagePayload_GTrace_AgentDestoryTask,
    GWInternalMessagePayload_GTrace_AgentCreateContent,
    GWInternalMessagePayload_GTrace_AgentStream
>{
 public:
    GWInternalMessage_gTrace() : GWInternalMessage() {}
    ~GWInternalMessage_gTrace() = default;
};
