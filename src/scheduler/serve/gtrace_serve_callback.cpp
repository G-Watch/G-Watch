#include <iostream>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/string.hpp"
#include "scheduler/scheduler.hpp"
#include "scheduler/serve/capsule_instance.hpp"
#include "scheduler/serve/gtrace_message.hpp"
#include "scheduler/agent/context.hpp"


gw_retval_t GWScheduler::__process_gtrace_req_PINGPONG(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_Common_PingPong *payload = nullptr;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(gtrace_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_PINGPONG);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_PingPong>(GW_MESSAGE_TYPEID_COMMON_PINGPONG);

    GW_IF_FAILED(
        gtrace_instance->send(message->serialize()),
        retval,
        {
            GW_WARN("failed to send pong response to gTrace");
        }
    );

    return retval;
}


gw_retval_t GWScheduler::__process_gtrace_req_READ_KV_DB(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS, dirty_retval = GW_SUCCESS;
    nlohmann::json resource;
    GWInternalMessagePayload_Common_DB_KV_Read *payload = nullptr;

    GW_CHECK_POINTER(gtrace_instance);
    GW_CHECK_POINTER(message);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_KV_READ_DB);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_DB_KV_Read>(GW_MESSAGE_TYPEID_COMMON_KV_READ_DB);

    GW_IF_FAILED(
        this->_db_kv.get_resource(payload->uri, resource),
        retval,
        {
            GW_WARN_C("failed to read from database: uri(%s)", payload->uri.c_str());
            dirty_retval = retval;
        }
    );
    payload->success = (retval == GW_SUCCESS);
    payload->read_payload = resource;

    GW_IF_FAILED(
        gtrace_instance->send(message->serialize()),
        retval,
        {
            GW_WARN("failed to send capsule topology to gTrace");
            dirty_retval = retval;
        }
    );

    delete message;
    return dirty_retval;
}


gw_retval_t GWScheduler::__process_gtrace_req_WRITE_KV_DB(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_Common_DB_KV_Write *payload = nullptr;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(gtrace_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_DB_KV_Write>(GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB);

    GW_IF_FAILED(
        this->_db_kv.set_resource(payload->uri, payload->write_payload),
        retval,
        GW_WARN_C("failed to write to database")
    );

    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_gtrace_req_SUBSCRIBE_TS_DB(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_Common_DB_TS_Subscribe *payload = nullptr;
    
    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(gtrace_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_TS_SUBSCRIBE_DB);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_DB_TS_Subscribe>(GW_MESSAGE_TYPEID_COMMON_TS_SUBSCRIBE_DB);
    GW_CHECK_POINTER(payload);

    // subscribe
    retval =  this->_db_ts.subscribe(
        /* instance */ (void*)(gtrace_instance),
        /* subscribe_id */ message->ref_id,
        /* uri */ payload->uri,
        /* callback */ GWScheduler::__cb_gtrace_subcribe_ts_db,
        /* init */ GWScheduler::__init_gtrace_subcribe_ts_db,
        /* user_data */ nullptr
    );
    if(unlikely(retval != GW_SUCCESS)){
        GW_WARN_C("failed to subscribe ts db: uri(%s)", payload->uri.c_str());
        payload->success = false;
        goto exit;
    }
    payload->success = true;

exit:
    // send subscribe reply
    GW_IF_FAILED(
        gtrace_instance->send(message->serialize()),
        retval,
        {
            GW_WARN("failed to send subscribe ts db response to gTrace");
            goto real_exit;
        }
    );

real_exit:
    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_gtrace_req_UNSUBSCRIBE_TS_DB(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_Common_DB_TS_Unsubscribe *payload = nullptr;
    
    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(gtrace_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_TS_UNSUBSCRIBE_DB);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_DB_TS_Unsubscribe>(GW_MESSAGE_TYPEID_COMMON_TS_UNSUBSCRIBE_DB);
    GW_CHECK_POINTER(payload);
    
    GW_IF_FAILED(
        this->_db_ts.unsubscribe(
            /* instance */ (void*)(gtrace_instance),
            /* subscribe_id */ message->ref_id,
            /* uri */ payload->uri,
            /* need_free_user_data */ false
        ),
        retval,
        GW_WARN_C("failed to unsubscribe ts db: uri(%s)", payload->uri.c_str());
    );

    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_gtrace_req_READ_SQL_DB(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS, dirty_retval = GW_SUCCESS;
    nlohmann::json resource;
    GWInternalMessagePayload_Common_DB_SQL_Read *payload = nullptr;

    GW_CHECK_POINTER(gtrace_instance);
    GW_CHECK_POINTER(message);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_SQL_READ_DB);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_DB_SQL_Read>(GW_MESSAGE_TYPEID_COMMON_SQL_READ_DB);
    GW_CHECK_POINTER(payload);

    GW_IF_FAILED(
        this->_db_sql.query(payload->query, payload->query_result),
        retval,
        GW_WARN_C("failed to query SQL db: query(%s)", payload->query.c_str());
    );
    payload->success = (retval == GW_SUCCESS);

    GW_IF_FAILED(
        gtrace_instance->send(message->serialize()),
        retval,
        {
            GW_WARN("failed to send SQL read result to gTrace");
            dirty_retval = retval;
        }
    );

    delete message;
    return dirty_retval;
}


gw_retval_t GWScheduler::__process_gtrace_req_SUBSCRIBE_SQL_DB(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_Common_DB_SQL_Subscribe *payload = nullptr;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(gtrace_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_SQL_SUBSCRIBE_DB);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_DB_SQL_Subscribe>(GW_MESSAGE_TYPEID_COMMON_SQL_SUBSCRIBE_DB);
    GW_CHECK_POINTER(payload);

    // subscribe
    retval =  this->_db_sql.subscribe(
        /* instance */ (void*)(gtrace_instance),
        /* subscribe_id */ message->ref_id,
        /* uri */ payload->uri,
        /* callback */ GWScheduler::__cb_gtrace_subcribe_sql_db,
        /* init */ GWScheduler::__init_gtrace_subcribe_sql_db,
        /* user_data */ nullptr
    );
    if(unlikely(retval != GW_SUCCESS)){
        GW_WARN_C("failed to subscribe sql db: uri(%s)", payload->uri.c_str());
        payload->success = false;
        goto exit;
    }
    payload->success = true;

exit:
    // send subscribe reply
    GW_IF_FAILED(
        gtrace_instance->send(message->serialize()),
        retval,
        GW_WARN("failed to send subscribe sql db response to gTrace");
    );

    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_gtrace_req_UNSUBSCRIBE_SQL_DB(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_Common_DB_SQL_Unsubscribe *payload = nullptr;
    
    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(gtrace_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_SQL_UNSUBSCRIBE_DB);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_DB_SQL_Unsubscribe>(GW_MESSAGE_TYPEID_COMMON_SQL_UNSUBSCRIBE_DB);
    GW_CHECK_POINTER(payload);

    GW_IF_FAILED(
        this->_db_sql.unsubscribe(
            /* instance */ (void*)(gtrace_instance),
            /* subscribe_id */ message->ref_id,
            /* uri */ payload->uri,
            /* need_free_user_data */ false
        ),
        retval,
        GW_WARN_C("failed to unsubscribe SQL db: table_name(%s)", payload->uri.c_str());
    );

    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_gtrace_req_AGENT_CREATE_CONTEXT(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWAgentContext *agent_context = nullptr;
    GWInternalMessagePayload_GTrace_AgentCreateContext *payload = nullptr;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(gtrace_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_CONTEXT);

    payload = message->get_payload_ptr<GWInternalMessagePayload_GTrace_AgentCreateContext>(GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_CONTEXT);
    GW_CHECK_POINTER(payload);

    GW_CHECK_POINTER(agent_context = new GWAgentContext(
        /* gtrace_instance */ gtrace_instance,
        /* user_config */ payload->user_config,
        /* map_db */ std::map<std::string, GWUtilDatabase*>({
            { "kv", &(this->_db_kv) },
            { "sql", &(this->_db_sql) },
            { "ts", &(this->_db_ts) },
        })
    ));
    agent_context->global_id = std::to_string((uint64_t)agent_context);
    this->_map_active_context[agent_context->global_id] = agent_context;
    this->_map_context[agent_context->global_id] = agent_context;
    GW_DEBUG_C("create agent context: id(%s)", agent_context->global_id.c_str());

    payload->context_global_id = agent_context->global_id;
    payload->is_success = true;

 exit:
    // send creation reply
    GW_IF_FAILED(
        gtrace_instance->send(message->serialize()),
        retval,
        GW_WARN("failed to send agent context creation response to gTrace");
    );

    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_gtrace_req_AGENT_DESTORY_CONTEXT(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_GTrace_AgentDestoryContext *payload = nullptr;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(gtrace_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_GTRACE_AGENT_DESTORY_CONTEXT);

    payload = message->get_payload_ptr<GWInternalMessagePayload_GTrace_AgentDestoryContext>(GW_MESSAGE_TYPEID_GTRACE_AGENT_DESTORY_CONTEXT);
    GW_CHECK_POINTER(payload);

    // check context
    if(this->_map_active_context.count(payload->context_global_id) == 0){
        GW_WARN_C(
            "failed to destory agent context, no active context found: id(%s)",
            payload->context_global_id.c_str()
        );
        retval = GW_FAILED_NOT_EXIST;
        payload->is_success = false;
        goto exit;
    }

    // remove from active context
    this->_map_active_context.erase(payload->context_global_id);
    payload->is_success = true;
    GW_DEBUG_C("destory agent context: id(%s)", payload->context_global_id.c_str());

 exit:
    // send destory reply
    GW_IF_FAILED(
        gtrace_instance->send(message->serialize()),
        retval,
        GW_WARN("failed to send agent context destory response to gTrace");
    );

    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_gtrace_req_AGENT_CREATE_TASK(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWAgentContext *agent_context = nullptr;
    GWAgentContextTask *agent_task = nullptr;

    GWInternalMessagePayload_GTrace_AgentCreateTask *payload = nullptr;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(gtrace_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_TASK);

    payload = message->get_payload_ptr<GWInternalMessagePayload_GTrace_AgentCreateTask>(GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_TASK);
    GW_CHECK_POINTER(payload);

    // obtain context
    if(this->_map_active_context.count(payload->context_global_id) == 0){
        GW_WARN_C(
            "failed to create task, no active context found: context(%s)",
            payload->context_global_id.c_str()
        );
        retval = GW_FAILED_NOT_EXIST;
        payload->is_success = false;
        goto exit;
    }
    GW_CHECK_POINTER(agent_context = this->_map_active_context[payload->context_global_id]);

    // create new prompt task
    GW_IF_FAILED(
        agent_context->create_task_from_parents(payload->list_parent_task_global_idx, agent_task),
        retval,
        {
            GW_WARN_C(
                "failed to add prompt task to agent context: context(%s)",
                agent_context->global_id.c_str()
            );
            payload->is_success = false;
            goto exit;
        }
    );

    payload->task_global_id = agent_task->global_id;
    payload->is_success = true;

 exit:
    // send prompt reply
    GW_IF_FAILED(
        gtrace_instance->send(message->serialize()),
        retval,
        GW_WARN_C("failed to send agent context destory response to gTrace");
    );

    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_gtrace_req_AGENT_DESTORY_TASK(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_GTrace_AgentDestoryTask *payload = nullptr;
    GWAgentContext *agent_context = nullptr;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(gtrace_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_GTRACE_AGENT_DESTORY_TASK);

    payload = message->get_payload_ptr<GWInternalMessagePayload_GTrace_AgentDestoryTask>(GW_MESSAGE_TYPEID_GTRACE_AGENT_DESTORY_TASK);
    GW_CHECK_POINTER(payload);

    // check context
    if(this->_map_active_context.count(payload->context_global_id) == 0){
        GW_WARN_C("failed to destory agent task, no active context found: context(%s)", payload->context_global_id.c_str());
        retval = GW_FAILED_NOT_EXIST;
        payload->is_success = false;
        goto exit;
    }
    GW_CHECK_POINTER(agent_context = this->_map_active_context[payload->context_global_id]);

    // detosry
    GW_IF_FAILED(
        agent_context->destory_task(payload->task_global_id),
        retval,
        {
            GW_WARN_C("failed to destory agent task: context(%s), task(%lu)", payload->context_global_id.c_str(), payload->task_global_id);
            payload->is_success = false;
            goto exit;
        }
    );

    payload->is_success = true;

exit:
    // send prompt reply
    GW_IF_FAILED(
        gtrace_instance->send(message->serialize()),
        retval,
        GW_WARN_C("failed to send agent task destory response to gTrace");
    );

    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_gtrace_req_AGENT_CREATE_CONTENT(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_GTrace_AgentCreateContent *payload = nullptr, *reply_payload = nullptr;
    GWAgentContext *agent_context = nullptr;
    GWAgentContextTask *agent_task = nullptr;
    GWAgentContent *agent_content = nullptr;
    GWInternalMessage_gTrace reply_message;
    nlohmann::json update_json_object;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(gtrace_instance);

    // parse incoming payload
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_CONTENT);
    payload = message->get_payload_ptr<GWInternalMessagePayload_GTrace_AgentCreateContent>(
        GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_CONTENT
    );
    GW_CHECK_POINTER(payload);

    // create reply message
    reply_message.type_id = GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_CONTENT;
    reply_message.ref_id = message->ref_id;
    reply_payload = reply_message.get_payload_ptr<GWInternalMessagePayload_GTrace_AgentCreateContent>(
        GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_CONTENT
    );
    GW_CHECK_POINTER(reply_payload);
    reply_payload->context_global_id = payload->context_global_id;
    reply_payload->task_global_id = payload->task_global_id;
    reply_payload->content_typeid = payload->content_typeid;

    // check context
    if(this->_map_active_context.count(payload->context_global_id) == 0){
        GW_WARN_C("failed to create prompt content, no active context found: context(%s)", payload->context_global_id.c_str());
        retval = GW_FAILED_NOT_EXIST;
        reply_payload->is_success = false;
        goto exit;
    }
    GW_CHECK_POINTER(agent_context = this->_map_active_context[payload->context_global_id]);

    // check task
    GW_IF_FAILED(
        agent_context->get_task_by_global_id(payload->task_global_id, agent_task),
        retval,
        {
            GW_WARN_C("failed to create prompt content, no active task found: context(%s), task(%lu)", payload->context_global_id.c_str(), payload->task_global_id);
            retval = GW_FAILED_NOT_EXIST;
            reply_payload->is_success = false;
            goto exit;
        }
    );

    // create content
    GW_IF_FAILED(
        agent_task->create_content(payload->content_typeid, agent_content),
        retval,
        {
            GW_WARN_C(
                "failed to create prompt content: context(%s), task(%lu), content(%lu), type(%d)", 
                payload->context_global_id.c_str(), payload->task_global_id, agent_content->id, payload->content_typeid
            );
            reply_payload->is_success = false;
            goto exit;
        }
    );
    reply_payload->is_success = true;
    reply_payload->content_global_id = agent_content->global_id;

    // process based on different content types
    if(payload->content_typeid == GW_AGENT_CONTENT_TYPEID_PROMPT) {
        // fill in prompt content
        update_json_object = nlohmann::json::object();
        update_json_object["body"] = payload->body;
        update_json_object["list_github_bodies"] = payload->list_github_bodies;
        GW_IF_FAILED(
            agent_content->set_metadata(update_json_object),
            retval,
            {
                GW_WARN("failed to set metadata of prompt content");
                reply_payload->is_success = false;
                goto exit;
            }
        );

        // execute the prompt
        if(payload->do_execute){
            GW_IF_FAILED(
                agent_task->execute_prompt_content_async(agent_content, message->ref_id),
                retval,
                {
                    GW_WARN_C(
                        "failed to execute prompt content: context(%s), task(%s), prompt_content(%s)", 
                        payload->context_global_id.c_str(),
                        payload->task_global_id.c_str(),
                        agent_content->global_id.c_str()
                    );
                    reply_payload->is_success = false;
                    goto exit;
                }
            );
        }
    }

 exit:
    // send prompt reply
    GW_IF_FAILED(
        gtrace_instance->send(reply_message.serialize()),
        retval,
        GW_WARN_C("failed to send agent task destory response to gTrace");
    );

    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_gtrace_req(
    GWgTraceInstance *gtrace_instance, gw_socket_addr_t addr, GWInternalMessage_gTrace *message
){
    gw_retval_t retval = GW_SUCCESS;
    std::thread *process_thread;
    GWInternalMessage_gTrace *message_copy;

    GW_CHECK_POINTER(gtrace_instance);
    GW_CHECK_POINTER(message);

    // we need to wait previous thread finish (if exist)
    if(this->_map_gtrace_processing_thread.count(gtrace_instance) > 0){
        GW_CHECK_POINTER(process_thread = this->_map_gtrace_processing_thread[gtrace_instance]);
        if(process_thread->joinable()){
            process_thread->join();
        }
        this->_map_gtrace_processing_thread.erase(gtrace_instance);
        delete process_thread;
    }

    // we need to make a copy of the message to avoid the message being freed
    // message should be freed within callback
    GW_CHECK_POINTER(message_copy = new GWInternalMessage_gTrace());
    message_copy->load_from_other(message);

    switch(message->type_id){
        case GW_MESSAGE_TYPEID_COMMON_HEARTBEAT:
            delete message_copy;
            break;

        case GW_MESSAGE_TYPEID_COMMON_PINGPONG:
            process_thread = new std::thread(
                &GWScheduler::__process_gtrace_req_PINGPONG, this, gtrace_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_gtrace_processing_thread[gtrace_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_COMMON_KV_READ_DB:
            process_thread = new std::thread(
                &GWScheduler::__process_gtrace_req_READ_KV_DB, this, gtrace_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_gtrace_processing_thread[gtrace_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB:
            process_thread = new std::thread(
                &GWScheduler::__process_gtrace_req_WRITE_KV_DB, this, gtrace_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_gtrace_processing_thread[gtrace_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_COMMON_SQL_READ_DB:
            process_thread = new std::thread(
                &GWScheduler::__process_gtrace_req_READ_SQL_DB, this, gtrace_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_gtrace_processing_thread[gtrace_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_COMMON_SQL_SUBSCRIBE_DB:
            process_thread = new std::thread(
                &GWScheduler::__process_gtrace_req_SUBSCRIBE_SQL_DB, this, gtrace_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_gtrace_processing_thread[gtrace_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_COMMON_SQL_UNSUBSCRIBE_DB:
            process_thread = new std::thread(
                &GWScheduler::__process_gtrace_req_SUBSCRIBE_SQL_DB, this, gtrace_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_gtrace_processing_thread[gtrace_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_COMMON_TS_SUBSCRIBE_DB:
            process_thread = new std::thread(
                &GWScheduler::__process_gtrace_req_SUBSCRIBE_TS_DB, this, gtrace_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_gtrace_processing_thread[gtrace_instance] = process_thread;
            break;
        

        case GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_CONTEXT:
            process_thread = new std::thread(
                &GWScheduler::__process_gtrace_req_AGENT_CREATE_CONTEXT, this, gtrace_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_gtrace_processing_thread[gtrace_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_GTRACE_AGENT_DESTORY_CONTEXT:
            process_thread = new std::thread(
                &GWScheduler::__process_gtrace_req_AGENT_DESTORY_CONTEXT, this, gtrace_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_gtrace_processing_thread[gtrace_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_TASK:
            process_thread = new std::thread(
                &GWScheduler::__process_gtrace_req_AGENT_CREATE_TASK, this, gtrace_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_gtrace_processing_thread[gtrace_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_GTRACE_AGENT_DESTORY_TASK:
            process_thread = new std::thread(
                &GWScheduler::__process_gtrace_req_AGENT_DESTORY_TASK, this, gtrace_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_gtrace_processing_thread[gtrace_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_GTRACE_AGENT_CREATE_CONTENT:
            process_thread = new std::thread(
                &GWScheduler::__process_gtrace_req_AGENT_CREATE_CONTENT, this, gtrace_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_gtrace_processing_thread[gtrace_instance] = process_thread;
            break;

        default:
            GW_WARN_C("not implement support for gtrace message type %d", message->type_id);
            delete message_copy;
            break;
    }

    return retval;
}


gw_retval_t GWScheduler::__cb_gtrace_subcribe_ts_db(
    gw_db_subscribe_context_t* sub_cxt, const nlohmann::json& new_val, const nlohmann::json& old_val
){
    gw_retval_t retval = GW_SUCCESS;
    GWgTraceInstance *gtrace_instance = nullptr;
    GWUtilTimeSeriesSample sample;
    GWInternalMessage_gTrace gtrace_message;
    GWInternalMessagePayload_Common_DB_TS_Stream *payload;

    GW_CHECK_POINTER(sub_cxt);
    GW_CHECK_POINTER(gtrace_instance = reinterpret_cast<GWgTraceInstance*>(sub_cxt->instance));

    payload = gtrace_message.get_payload_ptr<GWInternalMessagePayload_Common_DB_TS_Stream>(GW_MESSAGE_TYPEID_COMMON_TS_STREAM_DB);
    GW_CHECK_POINTER(payload);

    // deserialize sample
    GW_IF_FAILED(
        sample.deserialize(new_val),
        retval,
        {
            GW_WARN("failed to deserialize time series sample: error(%s)", gw_retval_str(retval));
            goto exit;
        }
    );

    // formup payload
    gtrace_message.type_id = GW_MESSAGE_TYPEID_COMMON_TS_STREAM_DB;
    gtrace_message.ref_id = sub_cxt->subscribe_id;
    payload->success = true;
    payload->uri = sub_cxt->uri;
    payload->index = sample.index;
    payload->timestamp = sample.timestamp;
    payload->read_payload = sample.payload;

    // send to gTrace
    GW_IF_FAILED(
        gtrace_instance->send(gtrace_message.serialize()),
        retval,
        {
            GW_WARN("failed to send to gTrace: %s", gw_retval_str(retval));
            goto exit;
        }
    );

exit:
    return retval;
}


gw_retval_t GWScheduler::__init_gtrace_subcribe_ts_db(GWUtilDatabase* db, gw_db_subscribe_context_t* sub_cxt){
    gw_retval_t retval = GW_SUCCESS, tmp_retval = GW_SUCCESS;
    GWgTraceInstance *gtrace_instance = nullptr;
    GWUtilTimeSeriesDatabase* ts_db = nullptr;
    std::vector<GWUtilTimeSeriesSample> response;
    GWInternalMessage_gTrace gtrace_stream_message;
    GWInternalMessagePayload_Common_DB_TS_Stream *stream_payload = nullptr;

    GW_CHECK_POINTER(sub_cxt);
    GW_CHECK_POINTER(ts_db = reinterpret_cast<GWUtilTimeSeriesDatabase*>(db));
    GW_CHECK_POINTER(gtrace_instance = reinterpret_cast<GWgTraceInstance*>(sub_cxt->instance));

    tmp_retval = ts_db->query_lockless(sub_cxt->uri, response);
    if(tmp_retval != GW_SUCCESS and tmp_retval != GW_FAILED_NOT_EXIST){
        GW_WARN("failed to initialize subscribtion, failed to query time series database: %s", gw_retval_str(tmp_retval));
        retval = tmp_retval;
        goto exit;
    }

    if(response.size() > 0){
        gtrace_stream_message.type_id = GW_MESSAGE_TYPEID_COMMON_TS_STREAM_DB;
        gtrace_stream_message.ref_id = sub_cxt->subscribe_id;
        stream_payload = gtrace_stream_message.get_payload_ptr<GWInternalMessagePayload_Common_DB_TS_Stream>(GW_MESSAGE_TYPEID_COMMON_TS_STREAM_DB);
        GW_CHECK_POINTER(stream_payload);
        stream_payload->uri = sub_cxt->uri;
        stream_payload->success = true;

        for(auto& sample : response){
            stream_payload->index = sample.index;
            stream_payload->timestamp = sample.timestamp;
            stream_payload->read_payload = sample.payload;
            GW_IF_FAILED(
                gtrace_instance->send(gtrace_stream_message.serialize()),
                retval,
                {
                    GW_WARN("failed to send existing samples to gTrace: %s", gw_retval_str(retval));
                    goto exit;
                }
            );
        }
    }

exit:
    return retval;
}


gw_retval_t GWScheduler::__cb_gtrace_subcribe_sql_db(
    gw_db_subscribe_context_t* sub_cxt, const nlohmann::json& new_val, const nlohmann::json& old_val
){
    gw_retval_t retval = GW_SUCCESS;
    GWgTraceInstance *gtrace_instance = nullptr;
    GWUtilTimeSeriesSample sample;
    GWInternalMessage_gTrace gtrace_message;
    GWInternalMessagePayload_Common_DB_SQL_Stream *payload;

    // rename
    const nlohmann::json query_result_json = new_val;
    std::string operation = old_val;

    GW_CHECK_POINTER(sub_cxt);
    GW_CHECK_POINTER(gtrace_instance = (GWgTraceInstance*)(sub_cxt->instance));

    payload = gtrace_message.get_payload_ptr<GWInternalMessagePayload_Common_DB_SQL_Stream>(GW_MESSAGE_TYPEID_COMMON_SQL_STREAM_DB);
    GW_CHECK_POINTER(payload);

    gtrace_message.type_id = GW_MESSAGE_TYPEID_COMMON_SQL_STREAM_DB;
    gtrace_message.ref_id = sub_cxt->subscribe_id;
    payload->success = true;
    payload->query = sub_cxt->uri;
    payload->query_result.deserialize(query_result_json);

    // send to gTrace
    GW_IF_FAILED(
        gtrace_instance->send(gtrace_message.serialize()),
        retval,
        {
            GW_WARN("failed to send to gTrace: %s", gw_retval_str(retval));
            goto exit;
        }
    );

exit:
    return retval;
}


gw_retval_t GWScheduler::__init_gtrace_subcribe_sql_db(GWUtilDatabase* db, gw_db_subscribe_context_t* sub_cxt){
    gw_retval_t retval = GW_SUCCESS, tmp_retval = GW_SUCCESS;
    GWgTraceInstance *gtrace_instance = nullptr;
    GWUtilSQLDatabase* sql_db = nullptr;
    GWInternalMessage_gTrace gtrace_stream_message;
    GWInternalMessagePayload_Common_DB_SQL_Stream *stream_payload = nullptr;
    GWUtilSqlQueryResult result;
    std::string sql;

    GW_CHECK_POINTER(sub_cxt);
    GW_CHECK_POINTER(sql_db = reinterpret_cast<GWUtilSQLDatabase*>(db));
    GW_CHECK_POINTER(gtrace_instance = reinterpret_cast<GWgTraceInstance*>(sub_cxt->instance));

    sql = "SELECT * FROM " + sub_cxt->uri + ";";
    result.clear();
    tmp_retval = sql_db->query_lockless(sql, result);
    if(tmp_retval != GW_SUCCESS and tmp_retval != GW_FAILED_NOT_EXIST){
        GW_WARN(
            "failed to init subscribtion of SQL database, failed to query existing result: "
            "error(%s), uri(%s)",
            gw_retval_str(tmp_retval),
            sub_cxt->uri
        );
        retval = tmp_retval;
        goto exit;
    }

    if(result.rows.size() > 0){
        gtrace_stream_message.type_id = GW_MESSAGE_TYPEID_COMMON_SQL_STREAM_DB;
        gtrace_stream_message.ref_id = sub_cxt->subscribe_id;

        stream_payload = gtrace_stream_message.get_payload_ptr<GWInternalMessagePayload_Common_DB_SQL_Stream>(GW_MESSAGE_TYPEID_COMMON_SQL_STREAM_DB);
        GW_CHECK_POINTER(stream_payload);
        stream_payload->success = true;
        stream_payload->query = sub_cxt->uri;
        stream_payload->query_result = result;
        GW_IF_FAILED(
            gtrace_instance->send(gtrace_stream_message.serialize()),
            retval,
            {
                GW_WARN(
                    "failed to init subscribtion of SQL database, failed to send existing samples to gTrace: "
                    "error(%s), uri(%s)",
                    gw_retval_str(retval),
                    sub_cxt->uri
                );
                goto exit;
            }
        );
    }

exit:
    return retval;
}
