#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <mutex>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/database_kv.hpp"

#include <nlohmann/json.hpp>


GWUtilKVDatabase::GWUtilKVDatabase() : GWUtilDatabase()
{}


GWUtilKVDatabase::~GWUtilKVDatabase()
{}


gw_retval_t GWUtilKVDatabase::set_resource(const std::string uri, const nlohmann::json& resource){
    gw_retval_t retval = GW_SUCCESS;
    std::vector<gw_db_subscribe_context_t*> list_subs_ctx = {};
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);
    
    // warnings
    if(uri.length() == 0 || uri.front() != '/'){
        GW_WARN_C("invalid uri to insert");
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }
    if(unlikely(this->_map_uri_resource.count(uri) > 0)){
        GW_WARN_C("resource already exists, udpate: uri(%s)", uri.c_str());
    }

    // execute callback
    this->__get_all_subscribers(nullptr, "", uri, /* uri_precise_match */ false, list_subs_ctx);
    for(auto& subs_ctx : list_subs_ctx){
        GW_IF_FAILED(
            subs_ctx->callback(
                /* subscribe_cxt */ subs_ctx,
                /* new_value */ resource,
                /* old_value */ this->_map_uri_resource.count(uri) > 0
                                ? this->_map_uri_resource[uri]
                                : ""
            ),
            retval,
            {
                GW_WARN_C("failed to execute callback, set failed: uri(%s)", uri.c_str());
                continue;
            }
        );
    }

    // update the resource
    this->_map_uri_resource[uri] = resource;
    GW_DEBUG_C("resource set: uri(%s), resource(%s)", uri.c_str(), resource.dump().c_str())

exit:
    return retval;
}


gw_retval_t GWUtilKVDatabase::set_resource(const std::vector<std::string> uri, const nlohmann::json& resource){
    std::string uri_str = "/";
    uint64_t i;

    if(uri.size() == 0){
        GW_WARN_C("empty uri to insert");
        return GW_FAILED_INVALID_INPUT;
    }

    for (i = 0; i < uri.size(); ++i) {
        uri_str += uri[i];
        if (i != uri.size() - 1) {
            uri_str += "/";
        }
    }

    return this->set_resource(uri_str, resource);
}


gw_retval_t GWUtilKVDatabase::get_resource(const std::string uri, nlohmann::json& resource){
    gw_retval_t retval = GW_SUCCESS;
    std::string uri_prefix;
    std::vector<nlohmann::json> resource_list;
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);

    // precise query
    if(this->_map_uri_resource.count(uri) > 0){
        resource = this->_map_uri_resource[uri];
        goto exit;
    } else if(uri.back() != '*'){
        resource = ""; // empty result
    }

    // wildcard query
    if(uri.back() == '*'){
        uri_prefix = uri.substr(0, uri.length()-1);
        for(auto &uri_resource : this->_map_uri_resource){
            if(uri_resource.first.find(uri_prefix) == 0){
                resource_list.push_back(uri_resource.second);
            }
        }
        resource = resource_list;
        goto exit;
    }

exit:
    return retval;
}


gw_retval_t GWUtilKVDatabase::get_resource(const std::vector<std::string> uri, nlohmann::json& resource){
    std::string uri_str = "";
    for(auto &uri_part : uri){
        uri_str += uri_part + "/";
    }
    return this->get_resource(uri_str, resource);
}


gw_retval_t GWUtilKVDatabase::delete_resource(const std::string uri){
    gw_retval_t retval = GW_SUCCESS;
    std::string uri_prefix = "";
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);

    // precise query
    if(this->_map_uri_resource.count(uri) > 0){
        this->_map_uri_resource.erase(uri);
        goto exit;
    } else if(uri.back() != '*'){
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }

    // wildcard query
    if(uri.back() == '*'){
        uri_prefix = uri.substr(0, uri.length()-1);
        for(auto &uri_resource : this->_map_uri_resource){
            if(uri_resource.first.find(uri_prefix) == 0){
                this->_map_uri_resource.erase(uri_resource.first);
            }
        }
        goto exit;
    }

exit:
    return retval;
}


gw_retval_t GWUtilKVDatabase::delete_resource(const std::vector<std::string> uri){
    std::string uri_str = "";
    uint64_t i = 0;

    for (i = 0; i < uri.size(); ++i) {
        uri_str += uri[i];
        if (i != uri.size() - 1) {
            uri_str += "/";
        }
    }

    return this->delete_resource(uri_str);
}
