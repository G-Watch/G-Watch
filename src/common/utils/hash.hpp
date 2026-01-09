#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <utility>

#include "common/common.hpp"

using gw_hash_u64_t = uint64_t;


/*!
 *  \brief  utilities to calculate hash value
 */
class GWUtilHash {
 public:
    /*!
     *  \brief  obtain a hash value of a list of strings
     *  \param  vec list of string to be hashed
     *  \return hash result
     */
    static gw_hash_u64_t cal(const std::vector<std::string>& vec){
        gw_hash_u64_t hash = 0;

        GW_ASSERT(vec.size() > 0);
        
        for (const auto& str : vec) {
            hash ^= std::hash<std::string>()(str) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }

    /*!
     *  \brief  obtain a hash value of a list of uint64_t
     *  \param  vec list of uint64_t to be hashed
     *  \return hash result
     */
    static gw_hash_u64_t cal(std::vector<uint64_t>& vec){
        static std::hash<uint64_t> hash_fn;
        gw_hash_u64_t hash = 0, tmp_hash;
        bool is_first = true;

        GW_ASSERT(vec.size() > 0);

        for (const auto& val : vec) {
            tmp_hash = hash_fn(val);
            if(is_first){ hash = tmp_hash; }
            else { hash ^= (tmp_hash << 1); }
        }

        return hash;
    }
};
