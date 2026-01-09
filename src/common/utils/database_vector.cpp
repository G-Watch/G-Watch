#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <mutex>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/database_vector.hpp"

#include <nlohmann/json.hpp>


GWUtilVectorDatabase::GWUtilVectorDatabase(gw_util_vector_database_config_t config)
    : GWUtilDatabase(), _config(config)
{
    // start vector db instance
    
    // detect embedding url
}


GWUtilVectorDatabase::~GWUtilVectorDatabase() {
    // close vector db instance
}
