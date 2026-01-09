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


class GWUtilKVDatabase final : public GWUtilDatabase {
 public:
    GWUtilKVDatabase();
    ~GWUtilKVDatabase();


    /*!
     *  \brief  set resource
     *  \param  uri         the uri of the resource
     *  \param  resource    the resource
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t set_resource(const std::string uri, const nlohmann::json& resource);
    gw_retval_t set_resource(const std::vector<std::string> uri, const nlohmann::json& resource);


    /*!
     *  \brief  get resource
     *  \param  uri         the uri of the resource
     *  \param  resource    the resource
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t get_resource(const std::string uri, nlohmann::json& resource);
    gw_retval_t get_resource(const std::vector<std::string> uri, nlohmann::json& resource);


    /*!
     *  \brief  delete resource
     *  \param  uri         the uri of the resource
     *  \return GW_SUCCESS if successful, GW_FAILED otherwise
     */
    gw_retval_t delete_resource(const std::string uri);
    gw_retval_t delete_resource(const std::vector<std::string> uri);


 private:
    // map to store resources
    std::map<std::string, nlohmann::json> _map_uri_resource;
};
