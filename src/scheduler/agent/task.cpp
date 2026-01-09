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
#include "scheduler/agent/context.hpp"
#include "scheduler/agent/system_prompt.hpp"


GWAgentContextTask::GWAgentContextTask(
    GWAgentContext* context, uint64_t id_, std::map<std::string, GWUtilDatabase*> map_db
)
    : _context(context), id(id_), _map_db(map_db)
{   
    // set global id of the task
    this->global_id = this->_context->global_id + "_" + std::to_string(this->id);

    // check database
    GW_ASSERT(map_db.count("kv") > 0);
    GW_ASSERT(map_db.count("sql") > 0);
    GW_ASSERT(map_db.count("ts") > 0);
    this->_db_kv = reinterpret_cast<GWUtilKVDatabase*>(map_db["kv"]);
    this->_db_sql = reinterpret_cast<GWUtilSQLDatabase*>(map_db["sql"]);
    this->_db_timeseries = reinterpret_cast<GWUtilTimeSeriesDatabase*>(map_db["ts"]);

    // insert task to db
    this->_db_sql->insert_row(
        "mgnt_agent_task",
        {
            { "global_id", this->global_id },
            { "context_global_id", context->global_id },
            { "status", "active" }
        }
    );
}


GWAgentContextTask::~GWAgentContextTask() {
    // delete task from db
    this->_db_sql->delete_row(
        "mgnt_agent_task",
        std::format("global_id = {}", this->global_id)
    );
}


gw_retval_t GWAgentContextTask::add_parent_task(GWAgentContextTask* task){
    GW_CHECK_POINTER(task);
    this->set_parent_tasks.insert(task);

    // insert topology to db
    this->_db_sql->insert_row(
        "mgnt_agent_task_topo",
        {
            { "parent_global_id", task->global_id },
            { "child_global_id", this->global_id }
        }
    );

    return GW_SUCCESS;
}


gw_retval_t GWAgentContextTask::add_parent_task(std::string global_id){
    gw_retval_t retval = GW_SUCCESS;
    GWAgentContextTask *task = nullptr;

    GW_IF_FAILED(
        this->_context->get_task_by_global_id(global_id, task),
        retval,
        goto exit;
    );
    GW_CHECK_POINTER(task);

    retval = this->add_parent_task(task);

exit:
    return retval;
}


gw_retval_t GWAgentContextTask::add_child_task(GWAgentContextTask* task){
    GW_CHECK_POINTER(task);
    this->set_children_tasks.insert(task);

    // insert topology to db
    this->_db_sql->insert_row(
        "mgnt_agent_task_topo",
        {
            { "parent_global_id", this->global_id },
            { "child_global_id", task->global_id }
        }
    );

    return GW_SUCCESS;
}


gw_retval_t GWAgentContextTask::add_child_task(std::string global_id){
    gw_retval_t retval = GW_SUCCESS;
    GWAgentContextTask *task = nullptr;

    GW_IF_FAILED(
        this->_context->get_task_by_global_id(global_id, task),
        retval,
        goto exit;
    );
    GW_CHECK_POINTER(task);
    
    retval = this->add_child_task(task);

exit:
    return retval;
}


gw_retval_t GWAgentContextTask::remove_parent_task(GWAgentContextTask* task){
    this->set_parent_tasks.erase(task);

    // remove topology to db
    this->_db_sql->delete_row(
        "mgnt_agent_task_topo",
        std::format(
            "parent_global_id = {} AND child_global_id = {}", 
            task->global_id, this->global_id
        )
    );

    return GW_SUCCESS;
}


gw_retval_t GWAgentContextTask::remove_parent_task(std::string global_id){
    gw_retval_t retval = GW_SUCCESS;
    GWAgentContextTask *task = nullptr;

    GW_IF_FAILED(
        this->_context->get_task_by_global_id(global_id, task),
        retval,
        goto exit;
    );
    GW_CHECK_POINTER(task);
    
    retval = this->remove_parent_task(task);

exit:
    return retval;
}


gw_retval_t GWAgentContextTask::remove_child_task(GWAgentContextTask* task){
    this->set_children_tasks.erase(task);

    // remove topology to db
    this->_db_sql->delete_row(
        "mgnt_agent_task_topo",
        std::format(
            "parent_global_id = {} AND child_global_id = {}", 
            this->global_id, task->global_id
        )
    );

    return GW_SUCCESS;
}


gw_retval_t GWAgentContextTask::remove_child_task(std::string global_id){
    gw_retval_t retval = GW_SUCCESS;
    GWAgentContextTask *task = nullptr;

    GW_IF_FAILED(
        this->_context->get_task_by_global_id(global_id, task),
        retval,
        goto exit;
    );
    GW_CHECK_POINTER(task);

    retval = this->remove_child_task(task);

exit:
    return retval;
}


std::set<GWAgentContextTask*> GWAgentContextTask::get_parent_tasks() const{
    return this->set_parent_tasks;
}


std::set<GWAgentContextTask*> GWAgentContextTask::get_children_tasks() const {
    return this->set_children_tasks;
}


std::vector<std::vector<GWAgentContextTask*>> GWAgentContextTask::__get_parent_tasks() const {
    std::vector<std::vector<GWAgentContextTask*>> result;
    std::vector<GWAgentContextTask*> path;
    std::unordered_set<const GWAgentContextTask*> visiting;

    std::function<void(const GWAgentContextTask*)> dfs = [&](const GWAgentContextTask* task) {
        if (!task) { return; }
        if (visiting.find(task) != visiting.end()) { return; }

        visiting.insert(task);
        path.push_back(const_cast<GWAgentContextTask*>(task));

        if (task->set_parent_tasks.empty()) {
            result.push_back(path);
        } else {
            for (GWAgentContextTask* p : task->set_parent_tasks) {
                dfs(p);
            }
        }
        path.pop_back();
        visiting.erase(task);
    };

    for (GWAgentContextTask* p : this->set_parent_tasks) { dfs(p); }
    return result;
}


gw_retval_t GWAgentContextTask::create_content(gw_agent_content_typeid_t type, GWAgentContent*& content){
    gw_retval_t retval = GW_SUCCESS;
    std::lock_guard<std::mutex> lk(this->_mutex_list_contents);

    content = new GWAgentContent(type, this, this->_list_contents.size(), this->_map_db);

    content->id = this->_list_contents.size();
    this->_list_contents.push_back(content);

exit:
    return retval;
}


gw_retval_t GWAgentContextTask::execute_prompt_content_async(GWAgentContent* agent_content_prompt, std::string stream_ref_id){
    gw_retval_t retval = GW_SUCCESS;
    uint64_t i = 0;
    std::string final_prompt = "";
    GWAgentContent *agent_content_response = nullptr;
    std::vector<std::vector<GWAgentContextTask*>> list_parent_tasks = {};

   // create response content
    GW_IF_FAILED(
        this->create_content(GW_AGENT_CONTENT_TYPEID_RESPONSE, agent_content_response),
        retval,
        {
            GW_WARN_C(
                "failed to create response content: context(%s), task(%s), prompt_content(%s)", 
                this->get_context()->global_id.c_str(),
                this->global_id,
                agent_content_prompt->global_id.c_str()
            );
            goto exit;
        }
    );

    // formup the prompt from previous parent tasks
    final_prompt = GWAgentSystemPrompt::get_intro_prompt();
    list_parent_tasks = this->__get_parent_tasks();
    for(auto& parent_tasks : list_parent_tasks){
        for(auto& parent_task : parent_tasks){
            final_prompt += "\n<task id=" + std::to_string(parent_task->id) + ">\n";
            final_prompt += parent_task->promptize();
            final_prompt += "\n</task>n\n";
        }
    }

    // formup the prompt based on previous contents and current content
    final_prompt += "\n<task id=" + std::to_string(this->id) + ">\n";
    {
        std::lock_guard<std::mutex> lk(this->_mutex_list_contents);
        for(i=0; i<=agent_content_prompt->id; i++){
            final_prompt += this->_list_contents[i]->promptize();
        }
    }
    final_prompt += "\n</task>n\n";

    // execute the content asynchronously
    GW_IF_FAILED(
        this->__execute_content_async(
            &GWAgentContextTask::__util_request_api_service,
            /* prompt */ final_prompt,
            /* agent_content_response */ agent_content_response,
            /* do_streaming */ true,
            /* streaming_ref_id */ stream_ref_id
        ),
        retval,
        {
            GW_WARN_C(
                "failed to execute prompt content: context(%s), task(%s), prompt_content(%s), response_content(%s)", 
                this->get_context()->global_id.c_str(),
                this->global_id,
                agent_content_prompt->global_id.c_str(),
                agent_content_response->global_id.c_str()
            );
            goto exit;
        }
    );

exit:
    return retval;
}


std::string GWAgentContextTask::promptize() const {
    std::string retval = "";
    uint64_t i = 0;

    for(i=0; i<this->_list_contents.size(); i++){
        retval += this->_list_contents[i]->promptize();
    }

    return retval;
}


gw_retval_t GWAgentContextTask::__util_request_api_service(
    std::string prompt, GWAgentContent* agent_content_response, bool do_streaming, std::string streaming_ref_id
){
    gw_retval_t retval = GW_SUCCESS;
    uint64_t i = 0;
    int sdk_retval = 0;
    std::vector<std::vector<GWAgentContextTask*>> list_parent_tasks = {};
    CURL *curl = nullptr;
    struct curl_slist *headers = nullptr;
    std::string curl_payload = "";
    nlohmann::json curl_payload_json;
    CURLcode res = CURLE_OK;
    __cb_api_service_param_t cb_param = {};

    // init CURL
    // todo(zhuobin): should we move to a unified place?
    curl = curl_easy_init();
    if(unlikely(curl == nullptr)){
        GW_WARN_C("failed to execute agent context task, due to failed init CURL");
        retval = GW_FAILED_SDK;
        goto exit;
    }

    // TODO(zhuobin): we static assign for debug here
    this->_context->user_config.api_service_url = "https://api.siliconflow.cn/v1/chat/completions";
    this->_context->user_config.api_service_model_name = "Qwen/Qwen3-8B";
    this->_context->user_config.api_service_token = "sk-ereidzvzkpmptcvtbnhqglgworkoamxmioatrfxxnbqfqkou";

    curl_payload_json["model"] = this->_context->user_config.api_service_model_name;
    curl_payload_json["messages"] = nlohmann::json::array();
    curl_payload_json["messages"].push_back({{"role", "user"}, {"content", prompt}});
    curl_payload_json["stream"] = true;
    curl_payload = curl_payload_json.dump();

    // formup CURL request
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(
        headers,
        std::format("Authorization: Bearer {}", this->_context->user_config.api_service_token).c_str()
    );
    curl_easy_setopt(curl, CURLOPT_URL, this->_context->user_config.api_service_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, curl_payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)curl_payload.size());

    // register callback
    cb_param.gtrace_instance = this->_context->_gtrace_instance;
    cb_param.agent_content_response = agent_content_response;
    cb_param.streaming_ref_id = streaming_ref_id;
    if(do_streaming){
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, GWAgentContextTask::__cb_api_service_sse);
    } else {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, GWAgentContextTask::__cb_api_service_non_sse);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cb_param);

    // allow redirection
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // for debug
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    // execute request
    GW_IF_CURL_FAILED(
        curl_easy_perform(curl),
        sdk_retval,
        {
            GW_WARN_C(
                "failed to execute agent context task, due to failed perform CURL: error(%s)",
                curl_easy_strerror((CURLcode)sdk_retval)
            );
            retval = GW_FAILED_SDK;
            goto exit;
        }
    );

exit:
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return retval;
}


size_t GWAgentContextTask::__cb_api_service_sse(char *ptr, size_t size, size_t nmemb, void *userdata){
    size_t total = size * nmemb;
    gw_retval_t tmp_retval = GW_SUCCESS;
    int idx = 0;
    std::string event_block = "", payload = "", piece = "";
    std::vector<std::string> lines = {};
    __cb_api_service_param_t *cb = nullptr;
    size_t sep_pos = std::string::npos;
    uint64_t start = 0, nl = 0, bi = 0, sep_len = 0;
    nlohmann::json response_json = "";
    GWgTraceInstance *gtrace_instance = nullptr;
    GWAgentContent* agent_content_response = nullptr;
    GWInternalMessage_gTrace stream_message;
    GWInternalMessagePayload_GTrace_AgentStream *stream_payload = nullptr;
    nlohmann::json update_json_object;
    
    auto __trim_str = [](const std::string &s) -> std::string {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    };

    // helper: find end index of the first JSON object in s (handles strings and escapes)
    auto find_json_end = [](const std::string &s) -> size_t {
        if (s.empty()) return std::string::npos;
        size_t n = s.size();
        // we expect a JSON object starting with '{'
        size_t i = 0;
        // skip leading whitespace
        while (i < n && isspace((unsigned char)s[i])) ++i;
        if (i >= n) return std::string::npos;
        if (s[i] != '{' && s[i] != '[') {
            return std::string::npos;
        }
        char open_char = s[i];
        char close_char = (open_char == '{') ? '}' : ']';
        int depth = 0;
        bool in_string = false;
        bool escape = false;
        for (; i < n; ++i) {
            char c = s[i];
            if (in_string) {
                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (c == '"') {
                    in_string = false;
                }
                continue;
            } else {
                if (c == '"') {
                    in_string = true;
                    continue;
                }
                if (c == open_char) {
                    ++depth;
                    continue;
                }
                if (c == close_char) {
                    --depth;
                    if (depth == 0) {
                        return i; // end index of the JSON object/array
                    }
                    continue;
                }
            }
        }
        return std::string::npos;
    };

    if (total == 0) return 0;
    if (ptr == nullptr) return 0;

    try {
        cb = static_cast<__cb_api_service_param_t*>(userdata);
        if (!cb){
            GW_WARN("failed to process sse event, due to null callback userdata");
            return total;
        }

        // formup the streaming message
        stream_message.type_id = GW_MESSAGE_TYPEID_GTRACE_AGENT_STREAM;
        stream_message.ref_id = cb->streaming_ref_id;
        stream_payload = stream_message.get_payload_ptr<GWInternalMessagePayload_GTrace_AgentStream>(
            GW_MESSAGE_TYPEID_GTRACE_AGENT_STREAM
        );
        GW_CHECK_POINTER(stream_payload);
        GW_CHECK_POINTER(gtrace_instance = cb->gtrace_instance)

        GW_CHECK_POINTER(agent_content_response = cb->agent_content_response);

        // append incoming bytes to buffer
        cb->sse_buffer.append(ptr, total);

        // process while we have at least one full SSE event (sep: \r\n\r\n or \n\n)
        while (true) {
            // prefer CRLF CRLF first
            sep_pos = cb->sse_buffer.find("\r\n\r\n");
            if (sep_pos != std::string::npos) {
                sep_len = 4;
            } else {
                sep_pos = cb->sse_buffer.find("\n\n");
                if (sep_pos != std::string::npos){
                    sep_len = 2;
                }
            }
            if (sep_pos == std::string::npos){
                break;
            }

            // extract one event block
            event_block = cb->sse_buffer.substr(0, sep_pos);
            // remove processed part including separator
            cb->sse_buffer.erase(0, sep_pos + sep_len);

            // --- reset per-event containers ---
            lines.clear();
            payload.clear();

            // split event_block into lines
            start = 0;
            while (start < event_block.size()) {
                nl = event_block.find_first_of("\r\n", start);
                if (nl == std::string::npos) {
                    lines.push_back(event_block.substr(start));
                    break;
                }
                // handle \r\n pair
                if (event_block[nl] == '\r' && nl + 1 < event_block.size() && event_block[nl+1] == '\n') {
                    lines.push_back(event_block.substr(start, nl - start));
                    start = nl + 2;
                } else {
                    lines.push_back(event_block.substr(start, nl - start));
                    // skip one newline char
                    start = nl + 1;
                }
            }

            // collect data lines (SSE spec: multiple data: lines should be joined with '\n')
            for (auto &ln : lines) {
                if (ln.rfind("data:", 0) == 0) {
                    std::string d = ln.substr(5); // remove 'data:'
                    payload += d;
                    payload.push_back('\n');
                }
            }
            payload = __trim_str(payload);

            if (payload.empty()) {
                GW_WARN("failed to process sse event, due to empty payload: nb_lines(%lu)", lines.size());
                continue;
            }

            // process possibly multiple JSON objects and trailing [DONE] packed into payload
            std::string remaining = payload;
            while (!remaining.empty()) {
                remaining = __trim_str(remaining);
                if (remaining.empty()) break;

                // handle explicit [DONE] (with or without quotes)
                if (remaining == "[DONE]" || remaining == "\"[DONE]\"" ) {
                    stream_payload->is_end = true;
                    remaining.clear();
                    break;
                }
                // if remaining begins with [DONE] but has trailing whitespace or chars, handle that:
                if (remaining.rfind("[DONE]", 0) == 0) {
                    stream_payload->is_end = true;
                    remaining.erase(0, 6);
                    continue;
                }
                if (remaining.rfind("\"[DONE]\"", 0) == 0) {
                    stream_payload->is_end = true;
                    remaining.erase(0, 8);
                    continue;
                }

                // if starts with JSON object or array
                size_t json_end = find_json_end(remaining);
                if (json_end != std::string::npos) {
                    // extract the first complete JSON substring
                    std::string jstr = remaining.substr(0, json_end + 1);
                    // remove it from remaining (and trim)
                    remaining.erase(0, json_end + 1);

                    // trim any separators between JSONs, like whitespace or commas/newlines
                    remaining = __trim_str(remaining);

                    try {
                        response_json = nlohmann::json::parse(jstr);

                        if (response_json.contains("choices") && response_json["choices"].is_array()) {
                            for (bi = 0; bi < response_json["choices"].size(); ++bi) {
                                auto &ch = response_json["choices"][bi];
                                if (ch.contains("delta")) {
                                    // index may be absent in some schemas; guard it
                                    if (ch.contains("index") && ch["index"].is_number_integer()) {
                                        idx = ch["index"];
                                    } else {
                                        idx = 0;
                                    }

                                    // reasoning tokens
                                    if(ch["delta"].contains("reasoning_content") && ch["delta"]["reasoning_content"].is_string()){
                                        piece = ch["delta"]["reasoning_content"].get<std::string>();
                                        cb->map_reasoning_output[idx] += piece;
                                        stream_payload->map_reasoning_output[idx] += piece;
                                        GW_DEBUG("streaming reasoning data: %s", piece.c_str());
                                    }

                                    // non-reasoning tokens
                                    if(ch["delta"].contains("content") && ch["delta"]["content"].is_string()){
                                        piece = ch["delta"]["content"].get<std::string>();
                                        cb->map_non_reasoning_output[idx] += piece;
                                        stream_payload->map_non_reasoning_output[idx] += piece;
                                        GW_DEBUG("streaming data: %s", piece.c_str());
                                    }
                                }
                            }
                        } else {
                            GW_WARN("callback: unrecognized response JSON chunk (not containing choices)");
                            stream_payload->is_success = false;
                            goto send_stream_payload;
                        }
                    } catch (const std::exception &e) {
                        GW_WARN("failed to parse LLM response to json chunk: error(%s), chunk(%s)", e.what(), jstr.c_str());
                        stream_payload->is_success = false;
                        goto send_stream_payload;
                    }

                    // loop to handle next JSON or [DONE] in remaining
                    continue;
                }

                // if we get here, remaining data is not a JSON object nor a recognized [DONE]; try a best-effort parse
                try {
                    response_json = nlohmann::json::parse(remaining);
                    // same processing as above
                    if (response_json.contains("choices") && response_json["choices"].is_array()) {
                        for (bi = 0; bi < response_json["choices"].size(); ++bi) {
                            auto &ch = response_json["choices"][bi];
                            if (ch.contains("delta")) {
                                if (ch.contains("index") && ch["index"].is_number_integer()) {
                                    idx = ch["index"];
                                } else {
                                    idx = 0;
                                }
                                if(ch["delta"].contains("reasoning_content") && ch["delta"]["reasoning_content"].is_string()){
                                    piece = ch["delta"]["reasoning_content"].get<std::string>();
                                    cb->map_reasoning_output[idx] += piece;
                                    stream_payload->map_reasoning_output[idx] += piece;
                                    GW_DEBUG("streaming reasoning data: %s", piece.c_str());
                                }
                                if(ch["delta"].contains("content") && ch["delta"]["content"].is_string()){
                                    piece = ch["delta"]["content"].get<std::string>();
                                    cb->map_non_reasoning_output[idx] += piece;
                                    stream_payload->map_non_reasoning_output[idx] += piece;
                                    GW_DEBUG("streaming data: %s", piece.c_str());
                                }
                            }
                        }
                    } else {
                        GW_WARN("callback: unrecognized response JSON (best-effort parse).");
                    }
                    // consumed all
                    remaining.clear();
                } catch (const std::exception &e) {
                    // give up for this event block if we can't parse what's left
                    GW_WARN("failed to parse remaining payload: error(%s), remaining(%s)", e.what(), remaining.c_str());
                    stream_payload->is_success = false;
                    goto send_stream_payload;
                }
            } // end while processing multiple JSONs in payload
        } // end while processing events

        stream_payload->is_success = true;
        stream_payload->content_global_id = agent_content_response->global_id;

        // record the final output  
        if(unlikely(stream_payload->is_end)){
            update_json_object = nlohmann::json::object();
            update_json_object["map_reasoning_output"] = cb->map_reasoning_output;
            update_json_object["map_non_reasoning_output"] = cb->map_non_reasoning_output;
            GW_IF_FAILED(
                agent_content_response->set_metadata(update_json_object),
                tmp_retval,
                {
                    GW_WARN("failed to set metadata after streaming finished");
                    stream_payload->is_success = false;
                    goto send_stream_payload;
                }
            );
        }

    send_stream_payload:
        GW_IF_FAILED(
            gtrace_instance->send(stream_message.serialize()),
            tmp_retval,
            {
                GW_WARN("failed to send stream message");
            }
        );
    } catch (...) {
        GW_WARN("callback_response caught unknown exception");
        return total;
    }

    return total;
}


size_t GWAgentContextTask::__cb_api_service_non_sse(char *ptr, size_t size, size_t nmemb, void *userdata){
    size_t totalSize = size * nmemb;
    std::string response = "";
    response.append(ptr, totalSize);
    GW_DEBUG("response: %s", response.c_str());
    return totalSize;
}
