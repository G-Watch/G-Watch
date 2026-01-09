#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <string>
#include <cstring>
#include <filesystem>
#include <queue>
#include <atomic>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <numa.h>
#include <libwebsockets.h>


#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/timer.hpp"
#include "common/utils/system.hpp"
#include "common/utils/string.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/socket.hpp"
#include "common/utils/queue.hpp"
#include "capsule/event.hpp"
#include "capsule/capsule.hpp"
#include "capsule/metric.hpp"
#include "scheduler/serve/capsule_message.hpp"


gw_retval_t GWCapsule::__detect_capsule(gw_capsule_info_t &capsule_info){
    gw_retval_t retval = GW_SUCCESS;
    int sdk_retval;
    struct utsname buffer;
    std::ifstream file;
    std::string line;

    // obtain IP address
    capsule_info.ip_addr = GWUtilSocket::get_local_ip();

    // obtain pid
    capsule_info.pid = getpid();

    // obtain global id
    capsule_info.global_id = "capsule-" + capsule_info.ip_addr + "-" + std::to_string(capsule_info.pid);

    // obtain cpu global id
    capsule_info.cpu_global_id = "cpu-" + capsule_info.ip_addr;

    // obtain start time, start TSC
    auto [time_str, tsc_tick] = GWUtilTscTimer::get_time_and_tsc();
    capsule_info.start_time = time_str;
    capsule_info.start_tsc = tsc_tick;
    capsule_info.end_time = "";

    // obtain kernel version
    GW_IF_LIBC_FAILED(
        uname(&buffer),
        sdk_retval,
        {
            GW_WARN_C("failed to get kernel version");
            retval = GW_FAILED;
            goto exit;
        }
    );
    capsule_info.kernel_version = buffer.release;

    // obtain OS distribution
    file.open("/etc/os-release");
    if (!file) {
        GW_WARN_C("failed to get os distribution: failed to open /etc/os-release");
        retval = GW_FAILED;
        goto exit;
    }
    while(std::getline(file, line)) {
        if (line.find("PRETTY_NAME=") == 0) {
            capsule_info.os_distribution = line.substr(13, line.size() - 14);
            break;
        }
    }

    // obtain state
    capsule_info.state = GWCapsule::state_to_string(this->state);

    // obtain CUDA version
    #ifdef GW_BACKEND_CUDA
        GW_IF_CUDA_RUNTIME_FAILED(
            cudaRuntimeGetVersion(&capsule_info.cuda_rt_version),
            sdk_retval,
            {
                GW_WARN_C("failed to obtain runtime version");
                retval = GW_FAILED;
                goto exit;
            }
        );
        GW_IF_CUDA_RUNTIME_FAILED(
            cudaDriverGetVersion(&capsule_info.cuda_dv_version),
            sdk_retval,
            {
                GW_WARN_C("failed to obtain driver version");
                retval = GW_FAILED;
                goto exit;
            }
        );
    #endif

exit:
    if(file.is_open())
        file.close();
    return retval;
}


gw_retval_t GWCapsule::__detect_cpu_topology(gw_capsule_cpu_info_t &cpu_info){
    gw_retval_t retval = GW_SUCCESS;
    FILE *cpuinfo;
    struct sysinfo info;
    char cpu_name[gw_capsule_cpu_info_t::cpu_name_length] = {0};
    char line[256] = {0};

    // obtain IP address
    cpu_info.ip_addr = GWUtilSocket::get_local_ip();

    // obtain global id
    cpu_info.global_id = "cpu-" + cpu_info.ip_addr;

    // obtain TSC frequency
    cpu_info.tsc_freq = this->_tsc_timer.get_tsc_freq();

    // obtain CPU name and number of cores
    cpu_info.num_cpu_cores = 0;
    if(unlikely((cpuinfo = fopen("/proc/cpuinfo", "r")) == nullptr)){
        GW_WARN_C("failed to open /proc/cpuinfo");
        retval = GW_FAILED_SDK;
        goto exit;
    }
    while (fgets(line, sizeof(line), cpuinfo)) {
        if (strncmp(line, "model name", 10) == 0) {
            char *name_start = strchr(line, ':');
            if (name_start) {
                strncpy(
                    cpu_name,
                    name_start + 2,
                    gw_capsule_cpu_info_t::cpu_name_length - 1
                );
                cpu_name[strcspn(cpu_name, "\n")] = 0;
            }
        }
        if (strncmp(line, "processor", 9) == 0) {
            cpu_info.num_cpu_cores++;
        }
    }
    fclose(cpuinfo);
    cpu_info.cpu_name = cpu_name;

    // obtain NUMA node information
    if(numa_available() == -1){
        cpu_info.num_numa_nodes = 0;
        GW_WARN_C("failed to obtain NUMA information");
        retval = GW_FAILED_SDK;
        goto exit;
    }
    cpu_info.num_numa_nodes = numa_num_configured_nodes();

    // ontain DRAM information
    if (sysinfo(&info) != 0) {
        cpu_info.dram_size = 0;
        GW_WARN_C("failed to obtain DRAM information");
        retval = GW_FAILED_SDK;
        goto exit;
    }
    cpu_info.dram_size = info.totalram * info.mem_unit;

 exit:
    return retval;
}


gw_retval_t GWCapsule::__detect_gpu_topology(std::vector<gw_capsule_gpu_info_t> &list_gpu_info){
    gw_retval_t retval = GW_SUCCESS;
    gw_capsule_gpu_info_t gpu_info;
    int i, num_device;
    std::string ip_addr;

    ip_addr = GWUtilSocket::get_local_ip();

    #ifdef GW_BACKEND_CUDA
        // TODO(zhuobin): we need a function / macro to get the actual CUDA API
        cudaError_t curt_retval;
        cudaDeviceProp device_prop;
        char pcie_bus_id[16] = {0};
    
        GW_IF_CUDA_RUNTIME_FAILED(
            cudaGetDeviceCount(&num_device),
            curt_retval,
            {
                GW_WARN_C("failed to obtain number of GPUs");
                retval = GW_FAILED;
                goto exit;
            }
        );
        
        for(i=0; i<num_device; i++){
            GW_IF_CUDA_RUNTIME_FAILED(
                cudaGetDeviceProperties(&device_prop, i),
                curt_retval,
                {
                    GW_WARN_C("failed to obtain GPU properties");
                    continue;
                }
            );

            GW_IF_CUDA_RUNTIME_FAILED(
                cudaDeviceGetPCIBusId(pcie_bus_id, 16, i),
                curt_retval,
                {
                    GW_WARN_C("failed to obtain GPU PCIe bus id");
                    continue;
                }
            );

            gpu_info.global_id = "gpu-" + ip_addr + "-" + pcie_bus_id;
            gpu_info.cpu_global_id = "cpu-" + ip_addr;
            gpu_info.local_id = i;
            gpu_info.pcie_bus_id = pcie_bus_id;
            gpu_info.chip_name = device_prop.name;
            gpu_info.macro_arch = device_prop.major;
            gpu_info.micro_arch = device_prop.minor;
            gpu_info.num_SMs = device_prop.multiProcessorCount;
            gpu_info.hbm_size = device_prop.totalGlobalMem;
            list_gpu_info.push_back(gpu_info);
        }
    #endif

 exit:
    return retval;
}
