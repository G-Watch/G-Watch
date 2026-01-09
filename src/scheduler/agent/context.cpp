#include <iostream>
#include <string>
#include <format>
#include <mutex>
#include <functional>
#include <unordered_set>

#include <curl/curl.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/database.hpp"
#include "common/utils/database_kv.hpp"
#include "common/utils/database_sql.hpp"
#include "common/utils/database_timeseries.hpp"
#include "scheduler/scheduler.hpp"
#include "scheduler/agent/system_prompt.hpp"
#include "scheduler/agent/context.hpp"


GWAgentContext::GWAgentContext(
    GWgTraceInstance* gtrace_instance,
    gw_agent_context_userconfig_t user_config_,
    std::map<std::string, GWUtilDatabase*> map_db
)
    : _gtrace_instance(gtrace_instance), user_config(user_config_), _map_db(map_db)
{
    // check database
    GW_ASSERT(this->_map_db.count("kv") > 0);
    GW_ASSERT(this->_map_db.count("sql") > 0);
    GW_ASSERT(this->_map_db.count("ts") > 0);
    this->_db_kv = reinterpret_cast<GWUtilKVDatabase*>(this->_map_db["kv"]);
    this->_db_sql = reinterpret_cast<GWUtilSQLDatabase*>(this->_map_db["sql"]);
    this->_db_timeseries = reinterpret_cast<GWUtilTimeSeriesDatabase*>(this->_map_db["ts"]);

    // insert context to db
    this->_db_sql->insert_row(
        "mgnt_agent_context",
        { 
            { "global_id", std::to_string((uint64_t)this) } 
        }
    );
}


GWAgentContext::~GWAgentContext()
{
    // delete context from db
    this->_db_sql->delete_row(
        "mgnt_agent_context",
        std::format("global_id = {}", std::to_string((uint64_t)this))
    );
}


gw_retval_t GWAgentContext::create_task(GWAgentContextTask*& task){
    gw_retval_t retval = GW_SUCCESS;
    std::string task_type = "";
    std::lock_guard<std::mutex> lock(this->_mutex_tasks);

    GW_CHECK_POINTER(task = new GWAgentContextTask(this, this->__get_nb_tasks_lockless(), this->_map_db));
    this->_map_tasks.insert({ task->global_id, task });

    return retval;
}


gw_retval_t GWAgentContext::destory_task(std::string task_global_id){
    gw_retval_t retval = GW_SUCCESS;
    GWAgentContextTask *task = nullptr;

    // check task in map
    if(unlikely(this->_map_tasks.count(task_global_id) == 0)){
        GW_WARN_C("failed to destory task, due to task not in map: context(%s), task_global_id(%lu)", this->global_id.c_str(), task_global_id);
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }
    GW_CHECK_POINTER(task = this->_map_tasks[task_global_id]);

    // update the row
    this->_db_sql->update_row(
        "mgnt_agent_task", {{ "status", "inactive" }}, std::format("task_global_id = {}", task_global_id)
    );

exit:
    return retval;
}


gw_retval_t GWAgentContext::create_task_from_parents(std::vector<GWAgentContextTask*> list_parent_tasks, GWAgentContextTask*& task){
    gw_retval_t retval = GW_SUCCESS;
    
    retval = this->create_task(task);
    if(unlikely(retval != GW_SUCCESS)){ goto exit; }

    GW_CHECK_POINTER(task);

    for(auto& parent_task : list_parent_tasks){
        GW_IF_FAILED(
            task->add_parent_task(parent_task),
            retval,
            {
                GW_WARN_C("failed to add parent task to child task: context(%s)", this->global_id.c_str());
                goto exit;
            }
        );
        GW_IF_FAILED(
            parent_task->add_child_task(task),
            retval,
            {
                GW_WARN_C("failed to add child task to parent task: context(%s)", this->global_id.c_str());
                goto exit;
            }
        );
    }

exit:
    return retval;
}


gw_retval_t GWAgentContext::create_task_from_parents(
    std::vector<std::string> list_parent_task_global_ids, GWAgentContextTask*& task
){
    gw_retval_t retval = GW_SUCCESS;
    std::vector<GWAgentContextTask*> list_parent_tasks = {};
    for(auto& parent_task_global_idx : list_parent_task_global_ids){
        if(likely(this->_map_tasks.count(parent_task_global_idx) > 0))
            list_parent_tasks.push_back(this->_map_tasks[parent_task_global_idx]);
    }
    retval = this->create_task_from_parents(list_parent_tasks, task);
    return retval;
}
