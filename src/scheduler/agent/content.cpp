#include <iostream>
#include <string>
#include <format>
#include <mutex>
#include <functional>
#include <unordered_set>


#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/database.hpp"
#include "common/utils/database_kv.hpp"
#include "common/utils/database_sql.hpp"
#include "common/utils/database_timeseries.hpp"
#include "scheduler/scheduler.hpp"
#include "scheduler/agent/context.hpp"



GWAgentContent::GWAgentContent(
    gw_agent_content_typeid_t type_,
    GWAgentContextTask* task,
    uint64_t id_,
    std::map<std::string, GWUtilDatabase*> map_db
)
    : _task(task), id(id_), _map_db(map_db)
{
    this->type = type_;
    this->global_id = task->global_id + "_" + std::to_string(this->id);

    // check database
    GW_ASSERT(map_db.count("kv") > 0);
    GW_ASSERT(map_db.count("sql") > 0);
    GW_ASSERT(map_db.count("ts") > 0);
    this->_db_kv = reinterpret_cast<GWUtilKVDatabase*>(map_db["kv"]);
    this->_db_sql = reinterpret_cast<GWUtilSQLDatabase*>(map_db["sql"]);
    this->_db_timeseries = reinterpret_cast<GWUtilTimeSeriesDatabase*>(map_db["ts"]);

    // insert task to db
    this->_db_sql->insert_row(
        "mgnt_agent_content",
        {
            { "global_id", this->global_id },
            { "context_global_id", task->get_context()->global_id },
            { "task_global_id", task->global_id },
            { "type", std::to_string(this->type) },
            { "metadata", "" }
        }
    );
}


GWAgentContent::~GWAgentContent(){
    // delete task from db
    this->_db_sql->delete_row(
        "mgnt_agent_content",
        std::format("global_id = {}", this->global_id)
    );
}


gw_retval_t GWAgentContent::set_metadata(const std::string key, const nlohmann::json value) {
    nlohmann::json updates = nlohmann::json::object();
    updates[key] = value;
    return this->set_metadata(updates);
}


gw_retval_t GWAgentContent::set_metadata(const nlohmann::json &updates) {
    gw_retval_t retval = GW_SUCCESS;
    nlohmann::json json_object;
    std::string where_clause = "";

    try {
        if (!updates.is_object()) {
            GW_WARN_C("set_metadata expects a JSON object (key -> value).");
            retval = GW_FAILED_INVALID_INPUT;
            goto exit;
        }

        // update local map (optionally guarded by mutex)
        {
            std::lock_guard<std::mutex> guard(this->_metadata_mutex);
            for (auto it = updates.begin(); it != updates.end(); ++it) {
                const std::string k = it.key();
                const nlohmann::json &v = it.value();

                if (v.is_null()) {
                    // treat null as delete
                    this->_map_metadata.erase(k);
                } else {
                    this->_map_metadata[k] = v;
                }
            }
        }

        // build the whole metadata JSON object from _map_metadata
        json_object = nlohmann::json::object();
        {
            std::lock_guard<std::mutex> guard(this->_metadata_mutex);
            for (const auto &p : this->_map_metadata) {
                json_object[p.first] = p.second;
            }
        }

        // update metadata in db
        where_clause = std::format("global_id = '{}'", this->global_id);
        GW_IF_FAILED(
            this->_db_sql->update_row(
                /* table_name */ "mgnt_agent_content",
                /* data */ { { "metadata", json_object } },
                /* where_clause */ where_clause
            ),
            retval,
            {
                GW_WARN_C(
                    "failed to set metadata of content in SQL database: "
                    "context(%s), task(%s), content(%s), json_object(%s)",
                    this->_task->get_context()->global_id.c_str(),
                    this->_task->global_id.c_str(),
                    this->global_id.c_str(),
                    json_object.dump().c_str()
                );
                goto exit;
            }
        );
    } catch (const std::exception &e) {
        GW_ERROR_C_DETAIL("exception while setting metadata: %s", e.what());
    }

exit:
    return retval;
}


nlohmann::json GWAgentContent::get_metadata(std::string key) const {
    if(this->_map_metadata.count(key) == 0){
        return nlohmann::json();
    } else {
        return this->_map_metadata.at(key);
    }
}


std::string GWAgentContent::promptize() const {
    std::string retval = "";

    if(this->type == GW_AGENT_CONTENT_TYPEID_PROMPT){
        retval += "<user_prompt>\n";

        // main body
        retval += "<body>\n" + this->get_metadata("body").dump() + "\n</body>\n";

        // github component
        if(this->get_metadata("list_github_bodies").is_array()){
            for(auto& component : this->get_metadata("list_github_bodies").get<std::vector<std::string>>()) {
                retval += "<github>\n" + component + "\n</github>\n";
            }
        }

        retval += "</user_prompt>\n";
    } else if (this->type == GW_AGENT_CONTENT_TYPEID_RESPONSE){
        retval += "<assistant_response>\n";
        retval += "<body>\n" + this->get_metadata("body").dump() + "\n</body>\n";
        retval += "</assistant_response>\n";
    }
    

    return retval;
}
