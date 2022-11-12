#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <memory>
#include <string>
#include <vector>
#include "thread_pool.h"
#include "source.h"
#include "log.h"

template <typename Session>
class tcp_server : public std::enable_shared_from_this<tcp_server<Session>>
{
    static std::string error_to_string(int err)
    {
        char buff[4096] = {0};
        strerror_r(err, buff, sizeof buff);
        return buff;
    }

   public:
    explicit tcp_server(uint16_t port) : port_(port) {}
    ~tcp_server() { LOG_INFO << "tcp server " << port_ << " quit"; }
    boost::asio::awaitable<void> run() { co_await listener(); }

   private:
    boost::asio::awaitable<void> rtmp_session(boost::asio::ip::tcp::socket socket)
    {
        auto session = std::make_shared<Session>(std::move(socket));
        co_await session->start();
        co_return;
    }

    boost::asio::awaitable<void> listener()
    {
        boost::system::error_code ec;
        auto executor = co_await boost::asio::this_coro::executor;
        boost::asio::ip::tcp::acceptor acceptor(executor, {boost::asio::ip::tcp::v4(), port_});
        for (;;)
        {
            auto socket = co_await acceptor.async_accept(redirect_error(boost::asio::use_awaitable, ec));
            if (ec)
            {
                continue;
            }
            co_spawn(executor, rtmp_session(std::move(socket)), boost::asio::detached);
        }
    }

   private:
    uint16_t port_ = 0;
    uint32_t count_ = 0;
};

#endif    // __TCP_SERVER_H__
