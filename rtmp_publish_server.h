#ifndef __RTMP_SESSION_H__
#define __RTMP_SESSION_H__

#include <memory>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/asio/use_awaitable.hpp>
#include "channel.h"
#include "buffer.h"
#include "source.h"

class rtmp_session : public std::enable_shared_from_this<rtmp_session>
{
   public:
    explicit rtmp_session(boost::asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}
    ~rtmp_session();

   public:
    boost::asio::awaitable<void> start();
    void shutdown(std::error_code ec);
    boost::asio::ip::tcp::socket& socket();

   private:
    boost::asio::awaitable<void> do_start();

   private:
    static int rtmp_server_send(void* param, const void* header, size_t len, const void* data, size_t bytes);
    static int rtmp_server_onpublish(void* param, const char* app, const char* stream, const char* type);
    static int rtmp_server_onscript(void* param, const void* script, size_t bytes, uint32_t timestamp);
    static int rtmp_server_onvideo(void* param, const void* data, size_t bytes, uint32_t timestamp);
    static int rtmp_server_onaudio(void* param, const void* data, size_t bytes, uint32_t timestamp);

   private:
    boost::asio::ip::tcp::socket socket_;
    //
    channel::ptr muxer_channel_;
    std::vector<buffer::ptr> write_queue_;
    //
    std::shared_ptr<struct rtmp_args> args_;
    source::ptr source_;
};

#endif    // __RTMP_SESSION_H__
