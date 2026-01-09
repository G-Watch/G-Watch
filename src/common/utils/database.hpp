#pragma once

#include <iostream>
#include <map>
#include <string>
#include <mutex>
#include <functional>

#include "common/common.hpp"
#include "common/log.hpp"

#include <nlohmann/json.hpp>


class GWUtilDatabase;
typedef struct gw_db_subscribe_context gw_db_subscribe_context_t;


using gw_db_subscribe_init_t = std::function<
    gw_retval_t(
        /* db */ GWUtilDatabase*,
        /* subscribe_cxt */ gw_db_subscribe_context_t*
    )
>;


using gw_db_subscribe_callback_t = std::function<
    gw_retval_t(
        /* subscribe_cxt */ gw_db_subscribe_context_t*,
        /* new_value */ const nlohmann::json,
        /* old_value */ const nlohmann::json
    )
>;


typedef struct gw_db_subscribe_context {
    // instance that subscribe the certain database resource
    void* instance = nullptr;

    // index of the subscription within the instance
    std::string subscribe_id = "";

    // identify the subscribed database resource
    std::string uri = "";

    // user data to invoke the callback
    void* user_data = nullptr;

    // callback function
    gw_db_subscribe_callback_t callback;

    bool operator==(const gw_db_subscribe_context& other) const {
        return this->instance == other.instance
            && this->subscribe_id  == other.subscribe_id
            && this->uri == other.uri
            && this->user_data == other.user_data;
    }
} gw_db_subscribe_context_t;


class GWUtilDatabase {
 public:
    GWUtilDatabase(){};
    ~GWUtilDatabase(){};


    /*!
     *  \brief  register callback function for subscribing change of a resource
     *  \param  instance        the instance that registers the callback function
     *  \param  subscribe_id    the id of the subscription
     *  \param  uri             the URI of the resource to be subscribed
     *  \param  callback        the callback function
     *  \param  init            the initialization function after successful subscribtion
     *  \param  user_data       the user data to be passed to the callback function
     *  \return GW_SUCCESS if the registering successful, GW_FAILED otherwise
     */
    gw_retval_t subscribe(
        void* instance, std::string subscribe_id, std::string uri, gw_db_subscribe_callback_t callback, gw_db_subscribe_init_t init = nullptr, void* user_data = nullptr
    );


    /*!
     *  \brief  unregister callback function for subscribing change of a resource
     *  \param  instance            the instance that unregisters the callback function
     *  \param  subscribe_id        the id of the subscription
     *  \param  uri                 the URI of the resource to be unsubscribed
     *  \param  need_free_user_data whether to free the user data
     *  \return GW_SUCCESS if the unregistering successful, GW_FAILED otherwise
     */
    gw_retval_t unsubscribe(void* instance, std::string subscribe_id, std::string uri, bool need_free_user_data = false);


 protected:
    /*!
     *  \brief  get all subscribers of a resource
     *  \note   this function should be execute under the protection of _callback_mutex
     *  \param  instance            the instance that subscribes the resource
     *  \param  subscribe_id        the id of the subscription
     *  \param  uri                 the URI of the resource to be subscribed
     *  \param  uri_precise_match   whether to match the uri precisely
     *  \param  list_subs_ctx       the list of subscribers
     *  \return GW_SUCCESS if the getting successful, GW_FAILED otherwise
     */
    gw_retval_t __get_all_subscribers(
        void* instance, std::string subscribe_id, std::string uri, bool uri_precise_match,
        std::vector<gw_db_subscribe_context_t*>& list_subs_ctx
    );

    // mutex to read and write DB
    std::mutex _db_mutex;

    // mutex to execute callback
    std::mutex _callback_mutex;

    // list of subscribe contexts
    std::vector<gw_db_subscribe_context_t*> _list_subscribe_cxt;
    
    // map to record the subscribe ctx: <instance, <subscribe_id, <uri, cxt>>>
    std::map<void*, std::map<std::string, std::map<std::string, gw_db_subscribe_context_t*>>> _map_ctx;

    // map to store callback functions: <instance, <uri, callback>>
    std::map<void*, std::map<std::string, gw_db_subscribe_callback_t>> _map_instance_uri_callback;
    
    // map to store user data to run callback functions <instance, <uri, user_data>>
    std::map<void*, std::map<std::string, void*>> _map_instance_uri_callback_user_data;
};
