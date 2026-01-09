#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <mutex>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/database.hpp"

#include <nlohmann/json.hpp>


gw_retval_t GWUtilDatabase::subscribe(
    void* instance, std::string subscribe_id, std::string uri, gw_db_subscribe_callback_t callback, gw_db_subscribe_init_t init, void* user_data
){
    gw_retval_t retval = GW_SUCCESS;
    gw_db_subscribe_context_t *subs_ctx = nullptr;
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);

    // check duplicate
    if(this->_map_ctx.count(instance) > 0){
        if(this->_map_ctx[instance].count(subscribe_id) > 0){
            if(this->_map_ctx[instance][subscribe_id].count(uri) > 0){
                GW_CHECK_POINTER(subs_ctx = this->_map_ctx[instance][subscribe_id][uri]);
                GW_WARN_C("duplicate subscription, overwrite: instance(%p), subscribe_id(%s), uri(%s)", instance, subscribe_id.c_str(), uri.c_str());
            }
        }
    }

    // allocate new context if needed
    if(subs_ctx == nullptr){
        GW_CHECK_POINTER(subs_ctx = new gw_db_subscribe_context_t());
    }

    // assign
    subs_ctx->instance = instance;
    subs_ctx->subscribe_id = subscribe_id;
    subs_ctx->uri = uri;
    subs_ctx->user_data = user_data;
    subs_ctx->callback = callback;
    this->_list_subscribe_cxt.push_back(subs_ctx);
    this->_map_ctx[instance][subscribe_id][uri] = subs_ctx;

    // init
    if(init != nullptr){
        GW_IF_FAILED(
            init(this, subs_ctx),
            retval,
            {
                GW_WARN_C(
                    "failed to init subscription: instance(%p), subscribe_id(%s), uri(%s)", instance, subscribe_id.c_str(), uri.c_str()
                );
                goto exit;
            }
        );
    }

    GW_DEBUG_C("subscribed database: instance(%p), subscribe_id(%s), uri(%s)", instance, subscribe_id.c_str(), uri.c_str());

exit:
    return retval;
}


gw_retval_t GWUtilDatabase::unsubscribe(
    void* instance, std::string subscribe_id, std::string uri, bool need_free_user_data
){
    gw_retval_t retval = GW_SUCCESS;
    gw_db_subscribe_context_t *subs_ctx = nullptr;
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);

    // check existance
    if(this->_map_ctx.count(instance) > 0){
        if(this->_map_ctx[instance].count(subscribe_id) > 0){
            if(this->_map_ctx[instance][subscribe_id].count(uri) > 0){
                GW_CHECK_POINTER(subs_ctx = this->_map_ctx[instance][subscribe_id][uri]);
            }
        }
    }

    if(unlikely(subs_ctx == nullptr)){
        GW_WARN_C("failed to unsubscribe, subscription not exist: instance(%p), subscribe_id(%s), uri(%s)", instance, subscribe_id.c_str(), uri.c_str());
        retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }

    // erase
    this->_map_ctx[instance][subscribe_id].erase(uri);
    for(auto it = this->_list_subscribe_cxt.begin(); it != this->_list_subscribe_cxt.end(); it++){
        if(*it == subs_ctx){
            this->_list_subscribe_cxt.erase(it);
            break;
        }
    }

    // free user_data if needed
    if(need_free_user_data && subs_ctx->user_data != nullptr){
        free(subs_ctx->user_data);
    }

    // remove subscribtion context
    delete subs_ctx;

exit:
    return retval;
}


gw_retval_t GWUtilDatabase::__get_all_subscribers(
    void* instance, std::string subscribe_id, std::string uri, bool uri_precise_match, std::vector<gw_db_subscribe_context_t*>& list_subs_ctx
){
    gw_retval_t retval = GW_SUCCESS;

    for(auto it = this->_list_subscribe_cxt.begin(); it != this->_list_subscribe_cxt.end(); it++){
        if(
            (instance == nullptr or (*it)->instance == instance)
            && (subscribe_id == "" or (*it)->subscribe_id == subscribe_id)
            && (uri == "" or (uri_precise_match ? (*it)->uri == uri : uri.find((*it)->uri) != std::string::npos))
        ){
            list_subs_ctx.push_back(*it);
        }
    }

exit:
    return retval;
}
