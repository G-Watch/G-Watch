#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/binary.hpp"
#include "common/instrument.hpp"
#include "common/assemble/kernel.hpp"
#include "common/utils/exception.hpp"


class GWCapsule;


/*!
 *  \brief  descriptor of a trace task instance
 */
class GWTraceTask {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     */
    GWTraceTask();


    /*!
     *  \brief  destructor
     */
    ~GWTraceTask();


    /*!
     *  \brief  get type string of the trace task (dynamic)
     *  \return type string of the trace task
     */
    virtual std::string get_type() { return this->_type; }


    /*!
     *  \brief  identify whether specific input need current trace task
     *  \param  query  query
     *  \return whether specific input need current trace task
     */
    virtual bool do_need_trace(nlohmann::json query) const;


    /*!
     *  \brief  serialize the event
     *  \return serialized string
     */
    virtual nlohmann::json serialize (
        std::string global_id,
        std::map<std::string, GWInstrumentCxt*>& map_instrument_ctx
    ) const {
        nlohmann::json object;
        GW_ERROR_C("not implemented");
        return object;
    }


    /*!
     *  \brief  execute the trace application
     *  \param  capsule                         capsule to execute the trace
     *  \param  global_id                       global_id of the trace task instance
     *  \param  kernel                          kernel to be executed
     *  \param  map_existing_instrument_ctx     map of existing instrument context
     *  \param  map_current_instrument_ctx      map of new instrument context
     *  \return GW_SUCCESS for successful execution
     */
    virtual gw_retval_t execute(
        GWCapsule* capsule, std::string global_id, GWKernel *kernel,
        std::map<std::string, GWInstrumentCxt*>& map_existing_instrument_ctx,
        std::map<std::string, GWInstrumentCxt*>& map_current_instrument_ctx
    ) const {
        GW_ERROR_C_DETAIL("not implemented yet");
        return GW_FAILED;
    }


    /*!
     *  \brief  get metadata of the trace definition
     *  \param  key     key of the metadata
     *  \param  value   value of the metadata
     *  \return metadata of the trace definition
     */
    inline gw_retval_t get_metadata(std::string key, nlohmann::json& value) const {
        gw_retval_t retval = GW_SUCCESS;

        if(unlikely(this->_map_metadata.find(key) == this->_map_metadata.end())){
            retval = GW_FAILED_NOT_EXIST;
            GW_WARN_C("failed to find metadata: key(%s)", key.c_str());
            goto exit;
        }

        value = this->_map_metadata.at(key);

    exit:
        return retval;
    }


    /*!
     *  \brief  check whether the trace definition has specific metadata
     *  \param  key   key of the metadata
     *  \return whether the trace definition has specific metadata
     */
    inline bool has_metadata(std::string key) const {
        return this->_map_metadata.find(key) != this->_map_metadata.end();
    }


    /*!
     *  \brief  set metadata of the trace definition
     *  \param  key   key of the metadata
     *  \param  value value of the metadata
     */
    inline void set_metadata(std::string key, nlohmann::json value){
        this->_map_metadata[key] = value;
    }


 protected:
    /*!
     *  \brief  execute the trace application (internally called)
     *  \param  capsule                         capsule to execute the trace
     *  \param  kernel                          kernel to be executed
     *  \param  global_id                       global_id of the trace task instance
     *  \param  map_instrument_ctx_name         map of names of instrument context to be executed
     *  \param  map_existing_instrument_ctx     map of existing instrument context
     *  \param  map_current_instrument_ctx      map of new instrument context
     *  \return GW_SUCCESS for successful execution
     */
    gw_retval_t __execute(
        GWCapsule* capsule, GWKernel *kernel, std::string global_id,
        std::vector<std::string> map_instrument_ctx_name,
        std::map<std::string, GWInstrumentCxt*>& map_existing_instrument_ctx,
        std::map<std::string, GWInstrumentCxt*>& map_current_instrument_ctx
    ) const;


    /*!
     *  \brief  execute one instrument context
     *  \param  capsule             capsule to execute the trace
     *  \param  instrument_cxt      instrument context to be executed
     *  \return GW_SUCCESS for successful execution
     */
    virtual gw_retval_t __execute_instrument_cxt(GWCapsule* capsule, GWInstrumentCxt* instrument_cxt) const {
        GW_ERROR_C_DETAIL("not implemented yet");
        return GW_FAILED;
    }


    // type name of the trace task
    std::string _type = "unknown";

    // metadata of the trace definition
    std::map<std::string, nlohmann::json> _map_metadata;
    /* ==================== Common ==================== */
};


class GWTraceTaskFactory {
 public:
    /*!
     *  \brief  creator of instrument context
     *  \param  trace_task  trace task of the instrument context
     *  \param  kernel      kernel of the instrument context
     *  \return the created instrument context
     */
    using GWTraceTaskCreator = std::function<GWTraceTask*()>;


    /*!
     *  \brief  get the instance of the factory
     *  \return the instance of the factory
     */
    static GWTraceTaskFactory& instance() {
        static GWTraceTaskFactory f_;
        return f_;
    }


    /*!
     *  \brief  create an typed trace task
     *  \param  type        type name of the instrument context
     *  \param  trace_task  trace task of the instrument context
     *  \param  kernel      kernel of the instrument context
     *  \return the created instrument context
     */
    GWTraceTask* create(const std::string& type) const {
        auto it = this->_map_creators.find(type);
        if (it == this->_map_creators.end()){
            GW_ERROR_C("failed to found creator of class %s", type.c_str());
        }
        return it->second();
    }


    /*!
     *  \brief  register a type of instrument context
     *  \param  type  type name of the instrument context
     *  \return GW_SUCCESS if success
     */
    template<typename T>
    void register_type(const std::string& type) {
        this->_map_creators[type] = []() -> GWTraceTask* {
            return new T();
        };
    }


 protected:
    // map of instrument context creators
    std::unordered_map<std::string, GWTraceTaskCreator> _map_creators;
};
