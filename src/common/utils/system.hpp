#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <fstream>

#include "common/common.hpp"
#include "common/log.hpp"


/*!
 *  \brief  utilities to process string
 */
class GWUtilSystem {
 public:
    /*!
     *  \brief  obtain an environment variable
     *  \param  env_name    environment variable name
     * \param   env_value   environment variable value
     *  \return GW_SUCCESS if success, GW_FAILED_NOT_EXIST if not exist
     */
    static gw_retval_t get_env_variable(const std::string& env_name, std::string& env_value){
        const char* __env_value = std::getenv(env_name.c_str());
        if (__env_value == nullptr) {
            return GW_FAILED_NOT_EXIST;
        }

        env_value = std::string(__env_value);
        return GW_SUCCESS;
    }

    
    /*!
     *  \brief  obtain the segment length in the memory map
     *  \param  ptr     pointer to the memory segment
     *  \return length of the memory segment
     */
    static uint64_t lookup_mapping_length(const void* ptr) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        std::ifstream maps("/proc/self/maps");
        std::string line;
        while (std::getline(maps, line)) {
            // format: "00400000-0040b000 r-xp ..."
            uintptr_t start, end;
            char perms[5];
            if (sscanf(line.c_str(), "%lx-%lx %4s", &start, &end, perms) != 3)
                continue;
            if (addr >= start && addr < end) {
                return end - start;
            }
        }
        return 0;
    }


    /*!
     *  \brief  obtain the process name from /proc/self/comm
     *  \return process name
     */
    static std::string get_process_name_from_proc_comm() {
        std::ifstream f("/proc/self/comm");
        std::string name = "";

        if (!f) { goto exit; }
        std::getline(f, name);

    exit:
        return name;
    }
};
