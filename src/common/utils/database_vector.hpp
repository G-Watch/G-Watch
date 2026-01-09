#pragma once

#include <iostream>
#include <map>
#include <string>
#include <mutex>
#include <functional>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/database.hpp"

#include <nlohmann/json.hpp>


/*!
 *  \brief  parameter to start a vector database
 */
typedef struct {
    std::string embedding_url = "https://localhost:8000";
} gw_util_vector_database_config_t;


/*!
 *  \brief  vector database
 */
class GWUtilVectorDatabase final : public GWUtilDatabase {
 public:
    GWUtilVectorDatabase(gw_util_vector_database_config_t config);
    ~GWUtilVectorDatabase();

 private:
    gw_util_vector_database_config_t _config = {};
};
