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
#include "scheduler/serve/capsule_instance.hpp"

#ifdef GW_BACKEND_CUDA
    #include "common/cuda_impl/function_attribute.hpp"
#endif


/*!
 *  \brief  message type for communication between scheduler and capsule
 */
enum :gw_message_typeid_t {
    // message for profiler to report heartbeat to scheduler
    GW_MESSAGE_TYPEID_CAPSULE_HEARTBEAT = 100,

    // message for capsule to report topology to scheduler
    GW_MESSAGE_TYPEID_CAPSULE_REGISTER,
};


/*!
 *  \brief  information of the capsule
 */
typedef struct gw_capsule_info {
    // global id of the capsule
    std::string global_id = "";

    // cpu global id of the capsule
    std::string cpu_global_id = "";

    // start time of the capsule
    std::string start_time = "";

    // start TSC tick of the capsule
    uint64_t start_tsc = 0;

    // end time of the capsule
    std::string end_time = "";

    // state of the capsule
    std::string state = "";

    // kernel version of the host
    std::string kernel_version = "";

    // os distribution
    std::string os_distribution = "";

    // pid of the process
    pid_t pid = 0;

    // ip address
    std::string ip_addr = "";

    // CUDA runtime / driver version
    #ifdef GW_BACKEND_CUDA
        int cuda_rt_version = 0;
        int cuda_dv_version = 0;
    #endif

    gw_capsule_info& operator=(const gw_capsule_info& other) {
        if (this != &other) {
            this->global_id = other.global_id;
            this->cpu_global_id = other.cpu_global_id;
            this->start_time = other.start_time;
            this->start_tsc = other.start_tsc;
            this->end_time = other.end_time;
            this->state = other.state;
            this->kernel_version = other.kernel_version;
            this->os_distribution = other.os_distribution;
            this->pid = other.pid;
            this->ip_addr = other.ip_addr;
            #ifdef GW_BACKEND_CUDA
                this->cuda_rt_version = other.cuda_rt_version;
                this->cuda_dv_version = other.cuda_dv_version;
            #endif
        }
        return *this;
    }

    gw_capsule_info(){}
    ~gw_capsule_info(){}
} gw_capsule_info_t;

#ifdef GW_BACKEND_CUDA
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
        gw_capsule_info_t,
        global_id,
        cpu_global_id,
        start_time,
        start_tsc,
        end_time,
        state,
        kernel_version,
        os_distribution,
        pid,
        ip_addr,
        cuda_rt_version,
        cuda_dv_version
    );
#else
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
        gw_capsule_info_t,
        global_id,
        cpu_global_id,
        start_time,
        start_tsc,
        end_time,
        state,
        kernel_version,
        os_distribution,
        pid,
        ip_addr
    );
#endif


/*!
 *  \brief  information of the host on which the capsule is running
 */
typedef struct gw_capsule_cpu_info {
    // global id of the CPU
    std::string global_id = "";

    // IP address
    std::string ip_addr = "";

    // TSC frequency
    double tsc_freq = 0;

    // CPU name
    std::string cpu_name = "";

    // number of CPU cores
    uint64_t num_cpu_cores = 0;

    // number of NUMA nodes
    uint8_t num_numa_nodes = 0;

    // DRAM size
    uint64_t dram_size = 0;

    gw_capsule_cpu_info& operator=(const gw_capsule_cpu_info& other) {
        if (this != &other) {
            this->global_id = other.global_id;
            this->ip_addr = other.ip_addr;
            this->tsc_freq = other.tsc_freq;
            this->cpu_name = other.cpu_name;
            this->num_cpu_cores = other.num_cpu_cores;
            this->num_numa_nodes = other.num_numa_nodes;
            this->dram_size = other.dram_size;
        }
        return *this;
    }

    static constexpr uint64_t cpu_name_length = 256;

    gw_capsule_cpu_info(){}
    ~gw_capsule_cpu_info(){}
} gw_capsule_cpu_info_t;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    gw_capsule_cpu_info_t,
    global_id,
    cpu_name,
    ip_addr,
    tsc_freq,
    num_cpu_cores,
    num_numa_nodes,
    dram_size
);


/*!
 *  \brief  information of the GPU on which the capsule is running
 */
typedef struct gw_capsule_gpu_info {
    // global id of the GPU
    std::string global_id = "";

    // cpu global id of the GPU
    std::string cpu_global_id = "";

    // local id of the GPU viewed by the capsule
    int local_id = 0;

    // PCIe bus id
    std::string pcie_bus_id = "";

    // chip name of the GPU
    std::string chip_name = "";

    // major architecture of the GPU
    uint16_t macro_arch = 0;
    
    // micro architecture of the GPU
    uint16_t micro_arch = 0;

    // number of SMs
    uint64_t num_SMs = 0;

    // HBM size
    uint64_t hbm_size = 0;

    gw_capsule_gpu_info& operator=(const gw_capsule_gpu_info& other) {
        if (this != &other) {
            this->global_id = other.global_id;
            this->cpu_global_id = other.cpu_global_id;
            this->local_id = other.local_id;
            this->pcie_bus_id = other.pcie_bus_id;
            this->chip_name = other.chip_name;
            this->macro_arch = other.macro_arch;
            this->micro_arch = other.micro_arch;
            this->num_SMs = other.num_SMs;
            this->hbm_size = other.hbm_size;
        }
        return *this;
    }

    gw_capsule_gpu_info(){}
    ~gw_capsule_gpu_info(){}
} gw_capsule_gpu_info_t;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    gw_capsule_gpu_info_t,
    global_id,
    cpu_global_id,
    local_id,
    chip_name,
    pcie_bus_id,
    macro_arch,
    micro_arch,
    num_SMs,
    hbm_size
);


class GWInternalMessagePayload_Capsule_Heartbeat final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Capsule_Heartbeat() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Capsule_Heartbeat() = default;

    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        return object;
    }

    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {}

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_CAPSULE_HEARTBEAT;
};


class GWInternalMessagePayload_Capsule_Register final : public GWInternalMessagePayload {
 public:
    GWInternalMessagePayload_Capsule_Register() : GWInternalMessagePayload() {}
    ~GWInternalMessagePayload_Capsule_Register() = default;

    /*!
     *  \brief  serialize the payload to json object
     *  \return json object
     */
    nlohmann::json serialize() override {
        nlohmann::json object;
        object["success"] = this->success;
        object["capsule_info"] = this->capsule_info;
        object["cpu_info"] = this->cpu_info;
        object["list_gpu_info"] = this->list_gpu_info;
        return object;
    }

    /*!
     *  \brief  deserialize the payload from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) override {
        if(json.contains("success"))
            this->success = json["success"];
        if(json.contains("capsule_info"))
            this->capsule_info = json["capsule_info"];
        if(json.contains("cpu_info"))
            this->cpu_info = json["cpu_info"];
        if(json.contains("list_gpu_info"))
            this->list_gpu_info = json["list_gpu_info"].get<std::vector<gw_capsule_gpu_info_t>>();
    }

    // index of the message type
    static constexpr gw_message_typeid_t msg_typeid = GW_MESSAGE_TYPEID_CAPSULE_REGISTER;

    // whether the register is success
    bool success = false;

    // capsule information
    gw_capsule_info_t capsule_info;

    // cpu topology
    gw_capsule_cpu_info_t cpu_info;

    // gpu topology
    std::vector<gw_capsule_gpu_info_t> list_gpu_info;
};


class GWInternalMessage_Capsule final : public GWInternalMessage<
    GWInternalMessagePayload_Common_PingPong,
    GWInternalMessagePayload_Common_DB_KV_Write,
    GWInternalMessagePayload_Common_DB_TS_Write,
    GWInternalMessagePayload_Common_DB_SQL_Write,
    GWInternalMessagePayload_Common_DB_SQL_CreateTable,
    GWInternalMessagePayload_Common_DB_SQL_DropTable,
    GWInternalMessagePayload_Capsule_Heartbeat,
    GWInternalMessagePayload_Capsule_Register
>{
 public:
    GWInternalMessage_Capsule() : GWInternalMessage() {}
    ~GWInternalMessage_Capsule() = default;
};
