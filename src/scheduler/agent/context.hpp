#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <thread>
#include <mutex>
#include <queue>

#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/database.hpp"
#include "common/utils/database_kv.hpp"
#include "common/utils/database_sql.hpp"
#include "common/utils/database_timeseries.hpp"


// forward declaration
class GWScheduler;
class GWgTraceInstance;
class GWAgentContext;
class GWAgentContextTask;
class GWAgentContent;


enum gw_agent_content_typeid_t : uint8_t {
    GW_AGENT_CONTENT_TYPEID_UNKNOWN = 0,
    GW_AGENT_CONTENT_TYPEID_PROMPT,
    GW_AGENT_CONTENT_TYPEID_RESPONSE,
};


class GWAgentContent {
    /* ==================== Common ==================== */
 public:
    GWAgentContent(
        gw_agent_content_typeid_t type_,
        GWAgentContextTask* task,
        uint64_t id_,
        std::map<std::string, GWUtilDatabase*> map_db
    );
    ~GWAgentContent();


    /*!
     *  \brief  promptize the content
     *  \return the promptized string
     */
    std::string promptize() const;


    // whether the response is end
    bool is_end = false;

    // index of the content in the task
    uint64_t id = 0;

    // global id of the content
    std::string global_id = "";

    // type of the action
    gw_agent_content_typeid_t type = GW_AGENT_CONTENT_TYPEID_UNKNOWN;


    /*!
     *  \brief  set the metadata of the content
     *  \param  key   key of the metadata
     *  \param  value value of the metadata
     *  \return GW_OK if success
     */
    gw_retval_t set_metadata(const std::string key, const nlohmann::json value);
    gw_retval_t set_metadata(const nlohmann::json &updates);


    /*!
     *  \brief  get the metadata of the content
     *  \param  key key of the metadata
     *  \return value of the metadata
     */
    nlohmann::json get_metadata(std::string key) const;


 protected:
    GWAgentContextTask* _task = nullptr;

    // metadata of the content
    std::mutex _metadata_mutex;
    std::map<std::string, nlohmann::json> _map_metadata = {};

    // database
    std::map<std::string, GWUtilDatabase*> _map_db;
    GWUtilKVDatabase* _db_kv = nullptr;
    GWUtilSQLDatabase* _db_sql = nullptr;
    GWUtilTimeSeriesDatabase* _db_timeseries = nullptr;
    /* ==================== Common ==================== */
};


class GWAgentContextTask {
    /* ==================== Common ==================== */
 public:
    GWAgentContextTask(
        GWAgentContext* context, uint64_t id_, std::map<std::string, GWUtilDatabase*> map_db
    );
    ~GWAgentContextTask();


    /*!
     *  \brief  get the context of the task
     *  \return the context of the task
     */
    GWAgentContext* get_context() const { return this->_context; }
    

    /*!
     *  \brief  serialize the task to string
     *  \return the serialized string
     */
    std::string promptize() const;


    // id of the task (within the context)
    uint64_t id = 0;

    // global id of the task
    std::string global_id = "";

 protected:
    // context of the task
    GWAgentContext *_context = nullptr;

    // database
    std::map<std::string, GWUtilDatabase*> _map_db;
    GWUtilKVDatabase* _db_kv = nullptr;
    GWUtilSQLDatabase* _db_sql = nullptr;
    GWUtilTimeSeriesDatabase* _db_timeseries = nullptr;
    /* ==================== Common ==================== */


    /* ============== Content Management ============== */
 public:
    /*!
     *  \brief  add a content to the task
     *  \param  type the type of the content
     *  \param  content the content to be added
     *  \return GW_SUCCESS for successfully added
     */
    gw_retval_t create_content(gw_agent_content_typeid_t type, GWAgentContent*& content);


    /*!
     *  \brief  execute the prompt content asynchronously
     *  \param  agent_content_prompt    the prompt content to be executed
     *  \param  stream_ref_id           the reference id of the stream
     *  \return GW_SUCCESS for successfully execution
     */
    gw_retval_t execute_prompt_content_async(GWAgentContent* agent_content_prompt, std::string stream_ref_id);


 protected:
    /*!
     *  \brief  execute the content asynchronously
     *  \param  memfn the member function pointer to be executed
     *  \param  params the parameters to be passed to the member function
     *  \return GW_SUCCESS for successfully executed
     */
    template<typename R, typename C, typename... MFParams, typename... CallParams>
    gw_retval_t __execute_content_async(R (C::*memfn)(MFParams...), CallParams&&... params) {
        static_assert((std::is_convertible_v<std::decay_t<CallParams>, MFParams> && ...),
                    "call argument types must be convertible to member function parameter types");
        return this->__execute_content_async_impl(memfn, std::forward<CallParams>(params)...);
    }

    template<typename R, typename C, typename... MFParams, typename... CallParams>
    gw_retval_t __execute_content_async(R (C::*memfn)(MFParams...) const, CallParams&&... params) {
        static_assert((std::is_convertible_v<std::decay_t<CallParams>, MFParams> && ...),
                    "call argument types must be convertible to member function parameter types");
        return this->__execute_content_async_impl(memfn, std::forward<CallParams>(params)...);
    }

    template<typename MemFn, typename... Params>
    gw_retval_t __execute_content_async_impl(MemFn mem_fn, Params&&... params) {
        using ArgsTuple = std::tuple<std::decay_t<Params>...>;
        ArgsTuple args(std::forward<Params>(params)...);
        std::thread *thread_ptr = nullptr;

        auto wrapper = [this, mem_fn, args = std::move(args)]() mutable {
            // --- Start of PRE logic ---
            {
                // wait until previous content finish their job
                std::lock_guard<std::mutex> lk(this->_mutex_content_execution_thread);
                for(auto& it : this->_map_content_execution_thread){
                    if(it.second->joinable() && it.first != std::this_thread::get_id()){ 
                        it.second->join();
                    }
                }
            }
            // --- End of PRE logic ---

            try {
                std::apply(
                    [&](auto&&... unpacked) {std::invoke(mem_fn, this, std::forward<decltype(unpacked)>(unpacked)...);}, args
                );
            } catch (const std::exception& e) {
                GW_WARN_C("exception occurs in thread: %s", e.what());
            } catch (...) {
                GW_WARN_C("unknown exception occurs in thread");
            }

            // --- Start of POST logic ---
            {
                // remove current threads from the map
                std::lock_guard<std::mutex> lk(this->_mutex_content_execution_thread);
                auto id = std::this_thread::get_id();
                auto it = _map_content_execution_thread.find(id);
                if (it != _map_content_execution_thread.end()) {
                    std::thread* ptr = it->second;
                    _map_content_execution_thread.erase(it);
                }
            }
            // --- End of POST logic ---
        };

        GW_CHECK_POINTER(thread_ptr = new std::thread(std::move(wrapper)));
        {
            std::lock_guard<std::mutex> lk(this->_mutex_content_execution_thread);
            auto id = thread_ptr->get_id();
            _map_content_execution_thread.emplace(id, thread_ptr);
        }

        return GW_SUCCESS;
    }

    // map of execution threads
    std::mutex _mutex_content_execution_thread;
    std::map<std::thread::id, std::thread*> _map_content_execution_thread = {};

    // list of contents within the task
    std::mutex _mutex_list_contents;
    std::vector<GWAgentContent*> _list_contents = {};
    /* ============== Content Management ============== */


    /* ================ Task Management =============== */
 public:
    /*!
     *  \brief  topology manager
     */
    gw_retval_t add_parent_task(GWAgentContextTask* task);
    gw_retval_t add_parent_task(std::string global_id);
    gw_retval_t add_child_task(GWAgentContextTask* task);
    gw_retval_t add_child_task(std::string global_id);
    gw_retval_t remove_parent_task(GWAgentContextTask* task);
    gw_retval_t remove_parent_task(std::string global_id);
    gw_retval_t remove_child_task(GWAgentContextTask* task);
    gw_retval_t remove_child_task(std::string global_id);
    std::set<GWAgentContextTask*> get_parent_tasks() const;
    std::set<GWAgentContextTask*> get_children_tasks() const;

 protected:
    /*!
     *  \brief  get the parent tasks of the task
     *  \return the parent tasks of the task
     */
    std::vector<std::vector<GWAgentContextTask*>> __get_parent_tasks() const;
 
    // topology
    std::set<GWAgentContextTask*> set_parent_tasks = {};
    std::set<GWAgentContextTask*> set_children_tasks = {};
    /* ================ Task Management =============== */


    /* ==================== Utils ===================== */
 protected:
    /*!
     *  \brief  execute request api service
     *  \param  agent_content   the content used to be store the output of request
     *  \param  prompt          the prompt to be executed
     *  \param  do_streaming    whether to do streaming
     *  \return GW_SUCCESS for successfully executed
     */
    gw_retval_t __util_request_api_service(
        std::string prompt, GWAgentContent* agent_content_response, bool do_streaming, std::string streaming_ref_id
    );

 private:
    /*!
     *  \brief  callback function to do api request
     */
    typedef struct {
        GWgTraceInstance* gtrace_instance = nullptr;
        GWAgentContent* agent_content_response = nullptr;
        std::string sse_buffer = "";
        std::string streaming_ref_id = "";
        std::map<int, std::string> map_non_reasoning_output = {};
        std::map<int, std::string> map_reasoning_output = {};
    } __cb_api_service_param_t;
    static size_t __cb_api_service_sse(char *ptr, size_t size, size_t nmemb, void *userdata);
    static size_t __cb_api_service_non_sse(char *ptr, size_t size, size_t nmemb, void *userdata);
    /* ==================== Utils ===================== */
};


typedef struct gw_agent_context_userconfig {
    // api service
    std::string api_service_url = "";
    std::string api_service_model_name = "";
    std::string api_service_token = "";
} gw_agent_context_userconfig_t;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    gw_agent_context_userconfig_t,
    api_service_url, api_service_model_name, api_service_token
);


class GWAgentContext {
    /* ==================== Common ==================== */
 public:
    GWAgentContext(
        GWgTraceInstance* gtrace_instance,
        gw_agent_context_userconfig_t user_config_,
        std::map<std::string, GWUtilDatabase*> map_db
    );
    ~GWAgentContext();


    // id of the context
    std::string global_id = "";

    // config of the context
    gw_agent_context_userconfig_t user_config;

 protected:
    friend class GWAgentContextTask;
    friend class GWAgentContent;

    // gTrace instance this context beblongs to
    GWgTraceInstance* _gtrace_instance = nullptr;

    // databases
    std::map<std::string, GWUtilDatabase*> _map_db;
    GWUtilKVDatabase* _db_kv = nullptr;
    GWUtilSQLDatabase* _db_sql = nullptr;
    GWUtilTimeSeriesDatabase* _db_timeseries = nullptr;
    /* ==================== Common ==================== */


    /* ================ Task Management =============== */
 public:
    /*!
     *  \brief  get the number of tasks in the context
     *  \return the number of tasks in the context
     */
    inline uint64_t get_nb_tasks() {
        std::lock_guard<std::mutex> lock(this->_mutex_tasks);
        return this->_map_tasks.size();
    }


    /*!
     *  \brief  get the task by global id
     *  \param  global_id the global id of the task
     *  \param  task the task to be returned
     *  \return GW_SUCCESS for successfully got the task
     */
    inline gw_retval_t get_task_by_global_id(std::string global_id, GWAgentContextTask*& task) {
        gw_retval_t retval = GW_SUCCESS;
        std::lock_guard<std::mutex> lock(this->_mutex_tasks);

        if(this->_map_tasks.count(global_id) == 0){
            retval = GW_FAILED_NOT_EXIST;
            goto exit;
        }
        task = this->_map_tasks[global_id];

    exit:
        return retval;
    }


    /*!
     *  \brief  add a task to the context
     *  \param  type the type of the task
     *  \param  task the task to be added
     *  \return GW_SUCCESS for successfully added
     */
    gw_retval_t create_task(GWAgentContextTask*& task);


    /*!
     *  \brief  destory the task
     *  \param  task_global_id the global id of the task to be destoryed
     *  \return GW_SUCCESS for successfully destoryed
     */
    gw_retval_t destory_task(std::string task_global_id);


    /*!
     *  \brief  add a task to the context
     *  \param  type the type of the task
     *  \param  list_parent_tasks the parent tasks of the task
     *  \param  task the task to be added
     *  \return GW_SUCCESS for successfully added
     */
    gw_retval_t create_task_from_parents(std::vector<GWAgentContextTask*> list_parent_tasks, GWAgentContextTask*& task);
    gw_retval_t create_task_from_parents(std::vector<std::string> list_parent_task_global_idx, GWAgentContextTask*& task);


 protected:
    /*!
     *  \brief  get the number of tasks in the context
     *  \return the number of tasks in the context
     */
    inline uint64_t __get_nb_tasks_lockless() {
        return this->_map_tasks.size();
    }

    // map of tasks in the context
    std::mutex _mutex_tasks;
    std::map<std::string, GWAgentContextTask*> _map_tasks = {};
    /* ================ Task Management =============== */
};
