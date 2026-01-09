#pragma once

#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <deque>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <libwebsockets.h>
#include <nlohmann/json.hpp>


/* ================= TCP/IP Socket ================= */

typedef struct gw_socket_addr {
    struct sockaddr_in sockaddr;

    gw_socket_addr(struct sockaddr_in sockaddr_){
        sockaddr = sockaddr_;
    }

    std::string str(){
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &this->sockaddr.sin_addr, ip_str, INET_ADDRSTRLEN);
        return std::string(ip_str) + ":" + std::to_string(sockaddr.sin_port);
    }

    bool operator==(const gw_socket_addr& other) const {
        return (sockaddr.sin_addr.s_addr == other.sockaddr.sin_addr.s_addr)
            && (sockaddr.sin_port == other.sockaddr.sin_port);
    }

    bool operator<(const gw_socket_addr& other) const {
        if (sockaddr.sin_addr.s_addr != other.sockaddr.sin_addr.s_addr) {
            return sockaddr.sin_addr.s_addr < other.sockaddr.sin_addr.s_addr;
        }
        return sockaddr.sin_port < other.sockaddr.sin_port;
    }
} gw_socket_addr_t;


/*!
 *  \brief  utilities to process unix/network socket
 */
class GWUtilSocket {
 public:
    static void set_socket_non_blocking(int fd){
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    static std::string get_local_ip() {
        struct ifaddrs *ifAddrStruct = nullptr;
        struct ifaddrs *ifa = nullptr;
        void *tmpAddrPtr = nullptr;
        std::string ip_address;

        getifaddrs(&ifAddrStruct);

        for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) {
                continue;
            }
            if (ifa->ifa_addr->sa_family == AF_INET) { // IPv4
                tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                
                // except loopback
                if (strcmp(ifa->ifa_name, "lo") != 0) {
                    ip_address = addressBuffer;
                    break; // obtain the first non-loopback address
                }
            }
        }
        if (ifAddrStruct != nullptr) {
            freeifaddrs(ifAddrStruct);
        }
        
        return ip_address;
    }
};

/* ================= TCP/IP Socket ================= */


/* ================= WebSocket ================= */
class GWScheduler;
typedef struct gw_websocket_desp gw_websocket_desp_t;

using gw_websocket_cb_t = int(struct lws*, enum lws_callback_reasons, void *, void *, size_t);
using gw_websocket_serve_func_t = void(GWScheduler*, gw_websocket_desp_t*);

typedef struct gw_websocket_desp {
    std::thread *serve_thread;
    gw_websocket_cb_t *wb_callback;
    gw_websocket_serve_func_t *wb_serve_func;
    struct lws_context *wb_context;
    std::vector<struct lws_protocols> list_wb_protocols;
    uint16_t port;

    gw_websocket_desp& operator=(const gw_websocket_desp& other) {
        if (this != &other) {
            serve_thread = other.serve_thread;
            wb_callback = other.wb_callback;
            wb_serve_func = other.wb_serve_func;
            wb_context = other.wb_context;
            list_wb_protocols = other.list_wb_protocols;
            port = other.port;
        }
        return *this;
    }
} gw_websocket_desp_t;


class GWUtilWebSocketInstance {
 public:
    GWUtilWebSocketInstance(struct lws *wsi, volatile bool *do_exit);
    ~GWUtilWebSocketInstance();

    /*!
     *  \brief  send a message to the other side of the connection
     *  \param  message_map  a map of message to send
     *  \return GW_SUCCESS if the message is sent successfully, GW_FAILED otherwise
     */
    gw_retval_t send(std::map<std::string, nlohmann::json> &&message_map);
    gw_retval_t send(std::string message);

    /*!
     *  \brief  sync all send message to be finished
     */
    gw_retval_t sync_send();

    static gw_retval_t wsi_send_chunk(GWUtilWebSocketInstance* ws_instance);

    /*!
     *  \brief  receive buffer operation
     */
    gw_retval_t recv_to_buf(void* in, size_t len);
    char* export_recv_buf();
    void reset_recv_buf();

    // send queue
    std::queue<std::string> send_queue;
    std::deque<uint64_t> send_offset_queue;

 private:
    // send buffer
    char *_send_buf;
    uint64_t _send_len;

    // receive buffer
    char* _recv_buf;
    uint64_t _recv_buf_offset;

    std::mutex _send_mutex;
    struct lws *_wsi;

    volatile bool *_do_exit;
};
/* ================= WebSocket ================= */
