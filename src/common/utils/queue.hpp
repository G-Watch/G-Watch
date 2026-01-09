#pragma once


#include <iostream>
#include <vector>
#include <mutex>
#include <functional>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/timer.hpp"
#include "common/utils/exception.hpp"


#define GW_UTILS_QUEUE_DEFAULT_CAPACITY 8192


template<typename T>
class GWUtilsQueue {
 public:
    /*!
     *  \brief  constructor
     */
    GWUtilsQueue(
        uint64_t reserve_len = GW_UTILS_QUEUE_DEFAULT_CAPACITY,
        std::function<gw_retval_t(T)> destructor_callback = [](T element){return GW_SUCCESS;}
    )   :   _is_lock(false),
            _destructor_callback(destructor_callback)
    {
        this->queue.reserve(reserve_len);
    }


    /*!
     *  \brief  destructor
     */
    ~GWUtilsQueue(){
        for(auto& element : this->queue){
            this->_destructor_callback(element);
        }
        this->queue.clear();
    }


    /*!
     *  \brief  push an element to the queue
     *  \param  element element to be pushed\
     *  \param  block   whether to block if the queue is locked
     *  \return position of the pushed element
     */
    inline gw_retval_t push(const T& element, bool block = false){
        gw_retval_t retval = GW_SUCCESS;
        std::lock_guard<std::mutex> lock(this->_mutex);
    
        while(this->_is_lock){
            if(!block){
                retval = GW_FAILED_NOT_READY; goto exit;
            }
        }

        this->queue.push_back(element);

    exit:
        return retval;
    }


    /*!
     *  \brief  pop an element from the queue
     *  \param  element element to be popped
     *  \param  block   whether to block if the queue is locked
     *  \return element popped
     */
    inline gw_retval_t pop(T& element, bool block = false){
        gw_retval_t retval = GW_SUCCESS;
        std::lock_guard<std::mutex> lock(this->_mutex);
        
        while(this->_is_lock){
            if(!block){
                retval = GW_FAILED_NOT_READY; goto exit;
            }
        }

        if(this->queue.empty()){
            retval = GW_FAILED_NOT_EXIST;
            goto exit;
        }

        element = this->queue.front();
        this->queue.erase(this->queue.begin());

    exit:
        return retval;
    }


    /*!
     *  \brief  pop an element from the queue
     *  \param  block   whether to block if the queue is locked
     *  \return element popped
     */
    inline gw_retval_t pop(bool block = false){
        gw_retval_t retval = GW_SUCCESS;
        std::lock_guard<std::mutex> lock(this->_mutex);
        
        while(this->_is_lock){
            if(!block){
                retval = GW_FAILED_NOT_READY; goto exit;
            }
        }

        if(this->queue.empty()){
            retval = GW_FAILED_NOT_EXIST;
            goto exit;
        }

        this->queue.erase(this->queue.begin());

    exit:
        return retval;
    }


    /*!
     *  \brief  check whether the queue is empty
     *  \return true if the queue is empty
     */
    inline bool empty(){
        std::lock_guard<std::mutex> lock(this->_mutex);
        return this->queue.empty();
    }


    /*!
     *  \brief  get the size of the queue
     *  \return size of the queue
     */
    inline size_t size(){
        std::lock_guard<std::mutex> lock(this->_mutex);
        return this->queue.size();
    }


    /*!
     *  \brief  get an element by its id
     *  \param  id id of the element
     *  \return element
     */
    inline T& get_by_id(uint64_t id){
        std::lock_guard<std::mutex> lock(this->_mutex);
        if(unlikely(id >= this->queue.size())){
            throw GWException(
                "GWUtilsQueue::get_by_id, id out of range: size(%lu), id(%lu)",
                this->queue.size(), id
            );
        }
        return this->queue[id];
    }


    /*!
     *  \brief  lock /unlock the queue
     */
    inline void lock(){
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_is_lock = true;
    }
    inline void unlock(){
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_is_lock = false;
    }


 private:
    // mutex
    std::mutex _mutex;

    // lock for the queue
    bool _is_lock;

    // queue
    std::vector<T> queue;

    // destructor for remaining elements in the queue while this queue is destroyed
    std::function<gw_retval_t(T)> _destructor_callback;
};
