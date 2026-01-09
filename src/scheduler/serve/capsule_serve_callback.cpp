#include <iostream>
#include <cstring>

#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/system.hpp"
#include "common/utils/string.hpp"
#include "common/utils/exception.hpp"
#include "scheduler/scheduler.hpp"
#include "scheduler/serve/capsule_instance.hpp"
#include "scheduler/serve/gtrace_message.hpp"


gw_retval_t GWScheduler::__process_capsule_resp_PINGPONG(
    GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_Common_PingPong *payload;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(capsule_instance);
    GW_ASSERT(capsule_instance->state == GWCapsuleInstance::state_t::STATE_IDLE);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_PINGPONG);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_PingPong>(GW_MESSAGE_TYPEID_COMMON_PINGPONG);

    GW_IF_FAILED(
        capsule_instance->send(message->serialize()),
        retval,
        {
            GW_WARN("failed to send pong response to capsule");
        }
    );

    return retval;
}


gw_retval_t GWScheduler::__process_capsule_resp_REGISTER(
    GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
){
    gw_retval_t retval = GW_SUCCESS;
    std::string gpu_global_id;
    GWInternalMessagePayload_Capsule_Register *payload;
    GWUtilSqlQueryResult result;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(capsule_instance);
    GW_ASSERT(capsule_instance->state == GWCapsuleInstance::state_t::STATE_IDLE);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_CAPSULE_REGISTER);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Capsule_Register>(GW_MESSAGE_TYPEID_CAPSULE_REGISTER);
    GW_CHECK_POINTER(payload);

    if(unlikely(payload->success == false)){
        GW_WARN_C("failed to register capsule, capsule-side error");
        goto exit;
    }

    // record capsule instance with its global id
    capsule_instance->global_id = payload->capsule_info.global_id;
    this->_map_global_id_to_capsule.insert({ capsule_instance->global_id, capsule_instance });

    // record capsule infomation to database
    result.clear();
    GW_IF_FAILED(
        this->_db_sql.query("SELECT global_id FROM mgnt_capsule WHERE global_id = \"" + payload->capsule_info.global_id + "\";", result),
        retval,
        goto exit;
    )
    if(result.rows.size() == 0){
        GW_IF_FAILED(
            this->_db_sql.insert_row(
                /* table_name */ "mgnt_capsule",
                /* data */      {
                    { "global_id", payload->capsule_info.global_id },
                    { "cpu_global_id", payload->capsule_info.cpu_global_id },
                    { "start_time", payload->capsule_info.start_time },
                    { "start_tsc", std::to_string(payload->capsule_info.start_tsc) },
                    { "end_time", payload->capsule_info.end_time },
                    { "state", payload->capsule_info.state },
                    { "kernel_version", payload->capsule_info.kernel_version },
                    { "os_distribution", payload->capsule_info.os_distribution },
                    { "pid", std::to_string(payload->capsule_info.pid) },
                    { "ip_addr", payload->capsule_info.ip_addr },
                    #ifdef GW_BACKEND_CUDA
                        { "cuda_rt_version", std::to_string(payload->capsule_info.cuda_rt_version) },
                        { "cuda_dv_version", std::to_string(payload->capsule_info.cuda_dv_version) },
                    #endif
                }
            ),
            retval,
            goto exit;
        )
        GW_DEBUG_C(
            "record new capsule to database: global_id(%s), cpu_global_id(%s), start_time(%s), start_tsc(%lu), end_time(%s), state(%s), kernel_version(%s), os_distribution(%s), pid(%u), ip_addr(%s)",
            payload->capsule_info.global_id.c_str(),
            payload->capsule_info.cpu_global_id.c_str(),
            payload->capsule_info.start_time.c_str(),
            payload->capsule_info.start_tsc,
            payload->capsule_info.end_time.c_str(),
            payload->capsule_info.state.c_str(),
            payload->capsule_info.kernel_version.c_str(),
            payload->capsule_info.os_distribution.c_str(),
            payload->capsule_info.pid,
            payload->capsule_info.ip_addr.c_str()
        );
    } else {
        GW_WARN_C("capsule with global id %s has already been registered, is this a bug?", payload->capsule_info.global_id.c_str());
    }

    // record cpu topology to database
    result.clear();
    GW_IF_FAILED(
        this->_db_sql.query("SELECT global_id FROM mgnt_cpu WHERE global_id = \"" + payload->capsule_info.cpu_global_id + "\";", result),
        retval,
        goto exit;
    )
    if(result.rows.size() == 0){
        GW_IF_FAILED(
            this->_db_sql.insert_row(
                /* table_name */ "mgnt_cpu",
                /* data */      {
                    { "global_id", payload->capsule_info.cpu_global_id },
                    { "cpu_name", payload->cpu_info.cpu_name },
                    { "ip_addr", payload->cpu_info.ip_addr },
                    { "tsc_freq", std::to_string(payload->cpu_info.tsc_freq) },
                    { "num_cpu_cores", std::to_string(payload->cpu_info.num_cpu_cores) },
                    { "num_numa_nodes", std::to_string(payload->cpu_info.num_numa_nodes) },
                    { "dram_size", std::to_string(payload->cpu_info.dram_size) },
                }
            ),
            retval,
            goto exit;
        )
        GW_DEBUG_C(
            "record new CPU to database: global_id(%s), cpu_name(%s), ip_addr(%s), tsc_freq(%lf), num_cpu_cores(%d), num_numa_nodes(%d), dram_size(%lu)", 
            payload->cpu_info.global_id.c_str(),
            payload->cpu_info.cpu_name.c_str(),
            payload->cpu_info.ip_addr.c_str(),
            payload->cpu_info.tsc_freq,
            payload->cpu_info.num_cpu_cores,
            payload->cpu_info.num_numa_nodes,
            payload->cpu_info.dram_size
        );
    }

    for(gw_capsule_gpu_info_t &gpu_info : payload->list_gpu_info){
        // record gpu topology to database
        result.clear();
        GW_IF_FAILED(
            this->_db_sql.query("SELECT global_id FROM mgnt_gpu WHERE global_id = \"" + gpu_info.global_id + "\";", result),
            retval,
            goto exit;
        )
        if(result.rows.size() == 0){
            GW_IF_FAILED(
                this->_db_sql.insert_row(
                    /* table_name */ "mgnt_gpu",
                    /* data */      {
                        { "global_id", gpu_info.global_id },
                        { "cpu_global_id", payload->capsule_info.cpu_global_id },
                        { "pcie_bus_id", gpu_info.pcie_bus_id },
                        { "chip_name", gpu_info.chip_name },
                        { "macro_arch", std::to_string(gpu_info.macro_arch) },
                        { "micro_arch", std::to_string(gpu_info.micro_arch) },
                        { "num_SMs", std::to_string(gpu_info.num_SMs) },
                        { "hbm_size", std::to_string(gpu_info.hbm_size) },
                    }
                ),
                retval,
                goto exit;
            )
            GW_DEBUG_C(
                "record new GPU to database: global_id(%s), cpu_global_id(%s), pcie_bus_id(%s), chip_name(%s), macro_arch(%d), micro_arch(%d), num_SMs(%d), hbm_size(%lu)", 
                gpu_info.global_id.c_str(),
                payload->capsule_info.cpu_global_id.c_str(),
                gpu_info.pcie_bus_id.c_str(),
                gpu_info.chip_name.c_str(),
                gpu_info.macro_arch,
                gpu_info.micro_arch,
                gpu_info.num_SMs,
                gpu_info.hbm_size
            );
        }

        // record mappings of capsule-gpu
        GW_IF_FAILED(
            this->_db_sql.insert_row(
                /* table_name */ "mgnt_capsule_gpu",
                /* data */      {
                    { "capsule_global_id", payload->capsule_info.global_id },
                    { "gpu_global_id", gpu_info.global_id },
                    { "gpu_local_id", std::to_string(gpu_info.local_id) },
                }
            ),
            retval,
            goto exit;
        )
        GW_DEBUG_C(
            "record mapping of capsule-gpu: capsule_global_id(%s), gpu_global_id(%s), gpu_local_id(%s)",
            payload->capsule_info.global_id.c_str(),
            gpu_info.global_id.c_str(),
            std::to_string(gpu_info.local_id).c_str()
        );

        // record mappings of cpu-gpu
        GW_IF_FAILED(
            this->_db_sql.insert_row(
                /* table_name */ "mgnt_cpu_gpu",
                /* data */      {
                    { "cpu_global_id", payload->capsule_info.cpu_global_id },
                    { "gpu_global_id", gpu_info.global_id },
                }
            ),
            retval,
            goto exit;
        );
        GW_DEBUG_C(
            "record mapping of cpu-gpu: cpu_global_id(%s), gpu_global_id(%s)",
            payload->capsule_info.cpu_global_id.c_str(),
            gpu_info.global_id.c_str()
        );
    }

exit:
    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_capsule_resp_DB_KV_WRITE(
    GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_Common_DB_KV_Write *payload;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(capsule_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_DB_KV_Write>(GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB);

    GW_IF_FAILED(
        this->_db_kv.set_resource(payload->uri, payload->write_payload),
        retval,
        GW_WARN_C("failed to write to key-value database")
    );

    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_capsule_resp_DB_TS_WRITE(
    GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_Common_DB_TS_Write *payload;
    GWUtilTimeSeriesSample sample;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(capsule_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_DB_TS_Write>(GW_MESSAGE_TYPEID_COMMON_TS_WRITE_DB);
    GW_CHECK_POINTER(payload);

    GW_IF_FAILED(
        sample.deserialize(payload->serialize()),
        retval,
        {
            GW_WARN_C("failed to write to timeseries database");
            goto exit;
        }
    );
    
    GW_IF_FAILED(
        this->_db_ts.append(payload->uri, sample),
        retval,
        {
            GW_WARN_C("failed to write to timeseries database");
            goto exit;
        }
    );
    GW_DEBUG_C("capsule write to timeseries database: uri(%s), index(%lu), timestamp(%lu)", payload->uri.c_str(), sample.index, sample.timestamp);

exit:
    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_capsule_resp_DB_SQL_CREATE_TABLE(
    GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_Common_DB_SQL_CreateTable *payload;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(capsule_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_SQL_CREATE_TABLE);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_DB_SQL_CreateTable>(GW_MESSAGE_TYPEID_COMMON_SQL_CREATE_TABLE);
    GW_CHECK_POINTER(payload);

    GW_IF_FAILED(
        this->_db_sql.create_table(payload->table_name, payload->schema),
        retval,
        {
            GW_WARN_C("failed to create table in SQL database: table_name(%s)", payload->table_name.c_str());
            goto exit;
        }
    );

exit:
    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_capsule_resp_DB_SQL_WRITE(
    GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
){
    gw_retval_t retval = GW_SUCCESS;
    GWInternalMessagePayload_Common_DB_SQL_Write *payload;

    GW_CHECK_POINTER(message);
    GW_CHECK_POINTER(capsule_instance);
    GW_ASSERT(message->type_id == GW_MESSAGE_TYPEID_COMMON_SQL_WRITE_DB);

    payload = message->get_payload_ptr<GWInternalMessagePayload_Common_DB_SQL_Write>(GW_MESSAGE_TYPEID_COMMON_SQL_WRITE_DB);
    GW_CHECK_POINTER(payload);
    
     GW_IF_FAILED(
        this->_db_sql.insert_row(payload->table_name, payload->insert_data),
        retval,
        {
            GW_WARN_C("failed to write to SQL database: table_name(%s)", payload->table_name.c_str());
            goto exit;
        }
    );

exit:
    delete message;
    return retval;
}


gw_retval_t GWScheduler::__process_capsule_resp(
    GWCapsuleInstance *capsule_instance, gw_socket_addr_t addr, GWInternalMessage_Capsule *message
){
    gw_retval_t retval = GW_SUCCESS;
    std::thread *process_thread;
    GWInternalMessage_Capsule *message_copy;

    GW_CHECK_POINTER(capsule_instance);
    GW_CHECK_POINTER(message);

    // we need to wait previous thread finish (if exist)
    if(this->_map_capsule_processing_thread.count(capsule_instance) > 0){
        GW_CHECK_POINTER(process_thread = this->_map_capsule_processing_thread[capsule_instance]);
        if(process_thread->joinable()){
            process_thread->join();
        }
        this->_map_capsule_processing_thread.erase(capsule_instance);
        delete process_thread;
    }

    // we need to make a copy of the message to avoid the message being freed
    // message should be freed within callback
    GW_CHECK_POINTER(message_copy = new GWInternalMessage_Capsule());
    message_copy->load_from_other(message);

    switch(message->type_id){
        case GW_MESSAGE_TYPEID_COMMON_PINGPONG:
            process_thread = new std::thread(
                &GWScheduler::__process_capsule_resp_PINGPONG, this, capsule_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_capsule_processing_thread[capsule_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_COMMON_KV_WRITE_DB:
            process_thread = new std::thread(
                &GWScheduler::__process_capsule_resp_DB_KV_WRITE, this, capsule_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_capsule_processing_thread[capsule_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_COMMON_TS_WRITE_DB:
            process_thread = new std::thread(
                &GWScheduler::__process_capsule_resp_DB_TS_WRITE, this, capsule_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_capsule_processing_thread[capsule_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_COMMON_SQL_WRITE_DB:
            process_thread = new std::thread(
                &GWScheduler::__process_capsule_resp_DB_SQL_WRITE, this, capsule_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_capsule_processing_thread[capsule_instance] = process_thread;
            break;

        case GW_MESSAGE_TYPEID_CAPSULE_HEARTBEAT:
            delete message_copy;
            break;

        case GW_MESSAGE_TYPEID_CAPSULE_REGISTER:
            process_thread = new std::thread(
                &GWScheduler::__process_capsule_resp_REGISTER, this, capsule_instance, addr, message_copy
            );
            GW_CHECK_POINTER(process_thread);
            this->_map_capsule_processing_thread[capsule_instance] = process_thread;
            break;

        default:
            GW_ERROR_C("not implement support for capsule message type %d", message->type_id);
    }

    return retval;
}
