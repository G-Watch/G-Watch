/*
 * Copyright 2025 The PhoenixOS Authors. All rights reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <iostream>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/_internals/readerwriter_queue/readerwriterqueue.h"

#define GW_LOCKLESS_QUEUE_LEN  8192


/*!
 *  \brief  lock-free queue
 *  \tparam T   elemenet type
 */
template<typename T>
class GWUtilsLockFreeQueue {
 public:
    GWUtilsLockFreeQueue(
        std::function<gw_retval_t(T)> destructor_callback = [](T element){return GW_SUCCESS;}
    ) : _is_enqueue_locked(false),
        _is_dequeue_locked(false),
        _destructor_callback(destructor_callback)
    {
        _q = new moodycamel::ReaderWriterQueue<T, GW_LOCKLESS_QUEUE_LEN>();
        GW_CHECK_POINTER(_q);
    }


    ~GWUtilsLockFreeQueue(){
        this->drain();
    }


    /*!
     *  \brief  generate a new queue node and append to the tail of it
     *  \param  data    the payload that the newly added node points to
     */
    void push(T element){
        if(this->_is_enqueue_locked == false)
            _q->enqueue(element); 
    }


    /*!
     *  \brief  obtain a pointer which points to the payload that the 
     *          head element points to
     *  \param  element reference to the variable to stored dequeued element (if any)
     *  \return GW_SUCCESS for successfully dequeued
     *          GW_FAILED_NOT_READY for empty queue
     */
    gw_retval_t dequeue(T& element){
        if(this->_is_dequeue_locked == false){
            if(_q->try_dequeue(element)){ return GW_SUCCESS; }
            else { return GW_FAILED_NOT_READY; }
        } else {
            return GW_FAILED_NOT_READY;
        }
    }


    /*!
     *  \brief  removes the front element from the queue, if any, without returning it
     *  \return true if an element is successfully removed, false if the queue is empty
     */
    inline bool pop(){ return _q->pop(); }


    /*!
     *  \brief  return the pointer to the front element of the queue
     *  \return pointer points to the front element
     *          nullptr if the queue is empty
     */
    T* peek(){
        return _q->peek();
    }


    /*!
     *  \brief  obtain the length in this queue
     *  \return length of the queue
     */
    inline uint64_t len(){ return _q->size_approx(); }


    /*!
     *  \brief  clear the queue
     */
    inline void drain(){
        uint64_t i, len;
        T element;

        this->lock_enqueue();
        this->lock_dequeue();
        
        len = this->len();
        for(i=0; i<len; i++){
            while(GW_SUCCESS == this->dequeue(element)){
                this->_destructor_callback(element);
            }
        }

        this->unlock_enqueue();
        this->unlock_dequeue();
    }


    /*!
     *  \brief  lock the queue
     */
    inline void lock_enqueue(){ this->_is_enqueue_locked = true; }
    inline void lock_dequeue(){ this->_is_dequeue_locked = true; }
    inline void lock(){ 
        this->_is_enqueue_locked = true;
        this->_is_dequeue_locked = true; 
    }


    /*!
     *  \brief  unlock the queue
     */
    inline void unlock_enqueue(){ this->_is_enqueue_locked = false; }
    inline void unlock_dequeue(){ this->_is_dequeue_locked = false; }
    inline void unlock(){ 
        this->_is_enqueue_locked = false;
        this->_is_dequeue_locked = false; 
    }


 private:
    // queue object
    moodycamel::ReaderWriterQueue<T, GW_LOCKLESS_QUEUE_LEN> *_q;

    // identify whether this queue is locked, if locked, nothing would be enqueued/dequeued
    bool _is_enqueue_locked;
    bool _is_dequeue_locked;
    
    // destructor for remaining elements in the queue while this queue is destroyed
    std::function<gw_retval_t(T)> _destructor_callback;
};
