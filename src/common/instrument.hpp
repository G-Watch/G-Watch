#pragma once

#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <any>

#include <nlohmann/json.hpp>

#include "common/common.hpp"
#include "common/log.hpp"


class GWInstruction;
class GWInstructionSet;
class GWKernelDef;
class GWKernel;
class GWTraceTask;


class GWInstrumentRegAllocCxt {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     */
    GWInstrumentRegAllocCxt(GWKernel* kernel);


    /*!
     *  \brief  destructor
     */
    ~GWInstrumentRegAllocCxt();


    /*!
     *  \brief  allocate extra register
     *  \param  type    type of register
     *  \param  reg_id  id of the extra register
     *  \return GW_SUCCESS if success
     */
    gw_retval_t alloc_extra(std::string type, uint64_t& reg_id);


    /*!
     *  \brief  get max register id
     *  \param  type            type of register
     *  \param  max_reg_id      max register id
     *  \param  omit_largest    whether omit the largest register id
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t get_max_reg_id(std::string type, uint64_t& max_reg_id, bool omit_largest = true){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  get number of used register
     *  \param  type            type of register
     *  \return number of used register
     */
    uint64_t get_nb_used_reg(std::string type);


    /*!
     *  \brief  allocate extra register
     *  \param  type            type of register
     *  \param  nb_continuous   number of continous register to be allocated
     *  \param  list_reg_idx    list of idx of register
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t alloc_extra(
        std::string type, uint32_t nb_continuous, std::vector<uint64_t>& list_reg_idx
    ){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  set register operation at certain pc
     *  \param  type    type of register
     *  \param  reg_id  id of register
     *  \param  pc      pc of instruction
     *  \param  op      operation of register
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t record_operation(
        std::string type, uint64_t reg_id, uint64_t pc, std::string op
    ){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  allocate existing register
     *  \param  start_pc  start pc
     *  \param  end_pc    end pc
     *  \param  type      type of register
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t alloc_reused(uint64_t start_pc, uint64_t end_pc, std::string type){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  get number of extra allocated register
     *  \param  type  type of register
     *  \return number of extra register
     */
    virtual uint64_t get_nb_extra_reg(std::string type) const {
        return 0;
    }


 protected:
    // kernel instance to conduct register allocation
    GWKernel *_kernel = nullptr;
    const GWKernelDef *_kernel_def = nullptr;

    // map of indices of extra allocated register: <type,[reg_id]>
    std::map<std::string, std::set<uint64_t>> _map_extra_allocated_reg;

    // map of register operation map: <type: <reg_id, [<pc, operation>]>>
    // NOTE(zhuobin): this map could modified as the register allocate/deallocate
    std::map<std::string, std::map<uint64_t, std::vector<std::pair<uint64_t, std::string>>>> _map_reg_op;
    /* ==================== Common ==================== */
};


class GWInstrumentCxt {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     *  \param  type        type of instrument
     *  \param  trace_task  trace task to be instrumented
     *  \param  kernel      kernel to be instrumented
     */
    GWInstrumentCxt(const GWTraceTask* trace_task, GWKernel* kernel);


    /*!
     *  \brief  destructor
     */
    ~GWInstrumentCxt();


    /*!
     *  \brief  instrument kernel
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t adopt(){ return GW_FAILED_NOT_IMPLEMENTAED; }


    /*!
     *  \brief  collect result after executing the instrumented kernel
     *  \return GW_SUCCESS if success
     */
    virtual gw_retval_t collect(){ return GW_FAILED_NOT_IMPLEMENTAED; }


    /*!
     *  \brief  serialize the event
     *  \return serialized string
     */
    virtual nlohmann::json serialize() {
        nlohmann::json object;
        GW_ERROR_C("not implemented");
        return object;
    }


    /*!
     *  \brief  obtain the trace type (dynamic)
     *  \return the trace type
     */
    virtual std::string get_type() { return this->_type; }


    /*!
     *  \brief  get profile result
     *  \return profile result
     */
    inline std::map<std::string, std::map<std::string, nlohmann::json>> get_trace_result() const {
        return this->_map_trace_result;
    }


    /*!
     *  \brief  set profile result
     *  \param  key     type of the profiler
     *  \param  value   value of the profile result
     */
    inline void set_trace_result(std::string panel_name, std::string key, nlohmann::json value) {
        this->_map_trace_result[panel_name][key] = value; 
    }


    /*!
     *  \brief  check if has certain profile result
     *  \return true if has profile result, false otherwise
     */
    inline bool has_trace_result(std::string panel_name, std::string key) const {
        return this->_map_trace_result.count(panel_name) > 0 
                and this->_map_trace_result.at(panel_name).count(key) > 0;
    }


    /*!
     *  \brief  obtain kernel processed by this instrumentation context
     *  \return kernel processed by this instrumentation context
     */
    GWKernel* get_kernel() const { return this->_kernel; }


    // index of the instrument context
    std::string global_id = "";

    // number of added register
    uint32_t nb_added_general_register = 0;
    uint64_t nb_updated_general_register = 0;

    // size of extra device memory
    void* extra_dmem = nullptr;
    uint64_t size_extra_dmem = 0;

    // size of extra shared memory usage
    uint64_t added_shared_memory_size = 0;

    // list of added parameters after instrumentation
    std::vector<uint32_t> list_added_parameter_size = {};
    std::vector<void*> list_added_parameters = {};

    // list of instructions after instrumentation
    std::vector<GWInstruction*> list_out_instructions = {};

    // number of origin instructions that have been inserted
    uint64_t nb_inserted_origin_instructions = 0; 

    // list of instrumentation positions
    std::vector<uint64_t> list_instrument_pc = {};

    // raw bytes of the instrumented binary (e.g., cubin for CUDA)
    std::vector<uint8_t> instrumented_binary_bytes = {};

 protected:
    friend class GWTraceTask;

    // type of the instrumentation application
    std::string _type = "";

    // trace task which this instrument context belongs to
    const GWTraceTask *_trace_task = nullptr;

    // kernel to be instrumented
    GWKernel* _kernel = nullptr;

    // trace result: <panel_name, <result_name, result_value>>
    std::map<std::string, std::map<std::string, nlohmann::json>> _map_trace_result;

    // register allocation context
    GWInstrumentRegAllocCxt *_reg_alloc_cxt = nullptr;
    /* ==================== Common ==================== */


    /* ==================== Dependencies ==================== */
 public:
    /*!
     *  \brief  add dependent instrument context
     *  \param  name    the name of the dependent instrument context
     *  \param  cxt     the dependent instrument context
     *  \return GW_SUCCESS if success
     */
    inline gw_retval_t add_dependent_cxt(std::string name, GWInstrumentCxt* cxt){
        gw_retval_t retval = GW_SUCCESS;

        GW_CHECK_POINTER(cxt);
        if(this->_map_dependent_cxt.count(name) > 0){
            GW_WARN_C("dependent instrument context %s already exists, overwritten", name.c_str());
        }
        this->_map_dependent_cxt[name] = cxt;

    exit:
        return retval;
    }


    /*!
     *  \brief  get dependent instrument context
     *  \param  name    the name of the dependent instrument context
     *  \return the dependent instrument context
     */
    inline gw_retval_t get_dependent_cxt(std::string name, GWInstrumentCxt*& cxt){
        gw_retval_t retval = GW_SUCCESS;

        if(this->_map_dependent_cxt.count(name) > 0){
            cxt = this->_map_dependent_cxt[name];
        } else {
            GW_WARN_C("dependent instrument context %s not found", name.c_str());
            cxt = nullptr;
            retval = GW_FAILED_NOT_EXIST;
        }

    exit:
        return retval;
    }

 protected:
    std::map<std::string, GWInstrumentCxt*> _map_dependent_cxt;
    /* ==================== Dependencies ==================== */
};


class GWInstrumentCxtFactory {
 public:
    /*!
     *  \brief  creator of instrument context
     *  \param  trace_task  trace task of the instrument context
     *  \param  kernel      kernel of the instrument context
     *  \return the created instrument context
     */
    using GWInstrumentCxtCreator = std::function<GWInstrumentCxt*(const GWTraceTask*, GWKernel*)>;


    /*!
     *  \brief  get the instance of the factory
     *  \return the instance of the factory
     */
    static GWInstrumentCxtFactory& instance() {
        static GWInstrumentCxtFactory f_;
        return f_;
    }


    /*!
     *  \brief  create an typed instrument context
     *  \param  type        type name of the instrument context
     *  \param  trace_task  trace task of the instrument context
     *  \param  kernel      kernel of the instrument context
     *  \return the created instrument context
     */
    GWInstrumentCxt* create(const std::string& type, const GWTraceTask* trace_task, GWKernel* kernel) const {
        auto it = this->_map_creators.find(type);
        if (it == this->_map_creators.end()){
            GW_ERROR_C("failed to found creator of class %s", type.c_str());
        }
        return it->second(trace_task, kernel);
    }


    /*!
     *  \brief  register a type of instrument context
     *  \param  type  type name of the instrument context
     *  \return GW_SUCCESS if success
     */
    template<typename T>
    void register_type(const std::string& type) {
        this->_map_creators[type] = [](const GWTraceTask* trace_task, GWKernel* kernel) -> GWInstrumentCxt* {
            return new T(trace_task, kernel);
        };
    }


 protected:
    // map of instrument context creators
    std::unordered_map<std::string, GWInstrumentCxtCreator> _map_creators;
};
