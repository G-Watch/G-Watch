#include <iostream>
#include <thread>
#include <mutex>
#include <map>
#include <string>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <libwebsockets.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/socket.hpp"


GWUtilWebSocketInstance::GWUtilWebSocketInstance(struct lws *wsi, volatile bool *do_exit)
    : _recv_buf_offset(0), _wsi(wsi), _do_exit(do_exit)
{
    GW_CHECK_POINTER(this->_send_buf = new char[LWS_PRE+GW_WEBSOCKET_MAX_MESSAGE_LENGTH])
    GW_CHECK_POINTER(this->_recv_buf = new char[GW_WEBSOCKET_MAX_MESSAGE_LENGTH])
}


GWUtilWebSocketInstance::~GWUtilWebSocketInstance(){
    if(this->_send_buf!= nullptr){
        delete this->_send_buf;
    }
    if(this->_recv_buf != nullptr){
        delete this->_recv_buf;
    }
    GW_CHECK_POINTER(this->_wsi);
    // lws_set_wsi_user(this->_wsi, NULL);
    // this would trigger the connection to be closed
    lws_callback_on_writable(this->_wsi);
}


gw_retval_t GWUtilWebSocketInstance::send(
    std::map<std::string, nlohmann::json> &&message_map
){
    gw_retval_t retval = GW_SUCCESS;
    nlohmann::json json_obj;
    std::string message;

    GW_CHECK_POINTER(message);

    for (const auto& pair : message_map) {
        json_obj[pair.first] = pair.second;
    }
    message = json_obj.dump();
    
    retval = this->send(message);

    return retval;
}


gw_retval_t GWUtilWebSocketInstance::send(std::string message){
    gw_retval_t retval = GW_SUCCESS;
    std::lock_guard<std::mutex> lock_guard(this->_send_mutex);

    // TODO(zhuobin): we need to change to async send

    this->send_queue.push(message);
    this->send_offset_queue.push_back(0);
    lws_callback_on_writable(this->_wsi);

    return retval;
}


gw_retval_t GWUtilWebSocketInstance::sync_send(){
    gw_retval_t retval = GW_SUCCESS;
    while(!this->send_queue.empty() and !this->_do_exit){ continue; }
    return retval;
}


gw_retval_t GWUtilWebSocketInstance::wsi_send_chunk(GWUtilWebSocketInstance* ws_instance){
    gw_retval_t retval = GW_SUCCESS;
    int send_bytes;
    std::string send_message;
    uint64_t send_offset;
    uint64_t send_size;
    char *send_buf = nullptr;
    int write_mode;
    bool is_first_chunk, is_only_chunk, is_last_chunk;

    GW_CHECK_POINTER(ws_instance);

    std::lock_guard<std::mutex> lock_guard(ws_instance->_send_mutex);

    // check queue length
    if(unlikely(ws_instance->send_queue.empty())){
        GW_DEBUG("send queue is empty, ignored");
        // retval = GW_FAILED_NOT_EXIST;
        goto exit;
    }

    // obtain the message chunk to be sent
    send_message = ws_instance->send_queue.front();
    send_offset = ws_instance->send_offset_queue.front();
    send_size = (send_message.length()-send_offset > GW_WEBSOCKET_CHUNK_SIZE) 
        ? GW_WEBSOCKET_CHUNK_SIZE : (send_message.length()-send_offset);

    // check some flags
    if(send_message.length() <= GW_WEBSOCKET_CHUNK_SIZE){
        is_only_chunk = true;
    } else {
        is_only_chunk = false;
    }
    if(send_offset + GW_WEBSOCKET_CHUNK_SIZE >= send_message.length()) {
        is_last_chunk = true;
    } else {
        is_last_chunk = false;
    }
    if(send_offset == 0){
        is_first_chunk = true;
    } else {
        is_first_chunk = false;
    }

    // make the write mode
    if (is_first_chunk) {
        if (is_only_chunk) {
            write_mode = (int)LWS_WRITE_TEXT;
        } else {
            write_mode = (int)LWS_WRITE_TEXT | (int)LWS_WRITE_NO_FIN;
        }
    } else {
        if (is_last_chunk) {
            write_mode = (int)LWS_WRITE_CONTINUATION;
        } else {
            write_mode = (int)LWS_WRITE_CONTINUATION | (int)LWS_WRITE_NO_FIN;
        }
    }

    // make sendbuf
    GW_CHECK_POINTER(send_buf = new char[send_size+LWS_PRE]);
    memset(send_buf, 0, send_size+LWS_PRE);
    memcpy(send_buf+LWS_PRE, send_message.c_str()+send_offset, send_size);

    // send
    send_bytes = lws_write(
        ws_instance->_wsi,
        (unsigned char*)(send_buf+LWS_PRE),
        send_size,
        (enum lws_write_protocol)write_mode
    );
    if(unlikely(send_bytes < 0)){
        GW_WARN("failed to send out websocket");
        retval = GW_FAILED;
        goto exit;
    }
    GW_ASSERT(send_bytes == send_size);

    // update the send queue
    if(is_last_chunk){
        ws_instance->send_queue.pop();
        ws_instance->send_offset_queue.pop_front();
    } else {
        ws_instance->send_offset_queue.front() = send_offset + send_size;
    }

exit:
    if(send_buf != nullptr)
        delete[] send_buf;
    if(!ws_instance->send_queue.empty()){
        lws_callback_on_writable(ws_instance->_wsi);
    }
    return retval;
}


gw_retval_t GWUtilWebSocketInstance::recv_to_buf(void* in, size_t len){
    gw_retval_t retval = GW_SUCCESS;

    if(this->_recv_buf_offset + len > GW_WEBSOCKET_MAX_MESSAGE_LENGTH){
        GW_WARN_C("receive buffer is full");
        retval = GW_FAILED;
        goto exit;
    }

    memcpy(this->_recv_buf+this->_recv_buf_offset, in, len);
    this->_recv_buf_offset += len;

exit:
    return retval;
}


char* GWUtilWebSocketInstance::export_recv_buf(){
    return this->_recv_buf;
}


void GWUtilWebSocketInstance::reset_recv_buf(){
    memset(this->_recv_buf, 0, this->_recv_buf_offset);
    this->_recv_buf_offset = 0;
}
