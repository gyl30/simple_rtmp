#ifndef SIMPLE_RTMP_TCP_SERVER_H
#define SIMPLE_RTMP_TCP_SERVER_H

#include <memory>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include "execution.h"
#include "error.h"
#include "log.h"

namespace simple_rtmp
{
template <typename Session>
class tcp_server : public std::enable_shared_from_this<tcp_server<Session>>
{
   public:
    tcp_server(uint16_t port, execution::executors::executor &io, execution::executors &pool) : port_(port), acceptor_(io), pool_(pool)
    {
        LOG_INFO("tcp server :{} create", port_);
    }

    ~tcp_server()
    {
        LOG_INFO("tcp server :{} destroy", port_);
    }

   public:
    void run()
    {
        open();
        do_accept();
    }

   private:
    void shutdown()
    {
        boost::system::error_code ec;
        acceptor_.close(ec);
    }

   private:
    void open()
    {
        boost::asio::ip::tcp::acceptor acceptor(acceptor_.get_executor());
        std::string ip = "0.0.0.0";
        boost::system::error_code ec;
        boost::asio::ip::address addr = boost::asio::ip::make_address(ip, ec);
        if (ec)
        {
            LOG_ERROR("tcp server address :{} error {}", port_, ec.message());
            return;
        }
        acceptor.open(boost::asio::ip::tcp::v4(), ec);
        if (ec)
        {
            LOG_ERROR("tcp server :{} open error {}", port_, ec.message());
            return;
        }
        int one  = 1;
        int flag = SO_REUSEADDR;

#ifdef __linux__
        flag |= SO_REUSEPORT;
#endif

        int ret = setsockopt(acceptor.native_handle(), SOL_SOCKET, flag, &one, sizeof(one));
        if (ret == -1)
        {
            LOG_WARN("tcp server :{} setsockopt error {}", port_, errno_to_str());
        }
        acceptor.bind(boost::asio::ip::tcp::endpoint(addr, port_), ec);
        if (ec)
        {
            LOG_ERROR("tcp server :{} bind error {}", port_, ec.message());
            return;
        }
        acceptor.listen(boost::asio::ip::tcp::socket::max_listen_connections, ec);
        if (ec)
        {
            LOG_ERROR("tcp server :{} listen error {}", port_, ec.message());
            return;
        }
        acceptor_ = std::move(acceptor);
        LOG_INFO("tcp server :{} listen", port_);
    }
    void do_accept()
    {
        if (!acceptor_.is_open())
        {
            return;
        }
        LOG_INFO("tcp server :{} accept count ", port_, count_++);
        session_  = std::make_shared<Session>(pool_.get_executor());
        auto self = tcp_server<Session>::shared_from_this();
        acceptor_.async_accept(session_->socket(), [this, self](boost::system::error_code ec) { on_accept(ec); });
    }
    void on_accept(boost::system::error_code &ec)
    {
        if (ec == boost::system::errc::operation_canceled)
        {
            return;
        }
        if (ec)
        {
            LOG_ERROR("tcp server :{} accept error {}", port_, ec.message());
            return;
        }
        session_->start();
        do_accept();
    }

   private:
    uint16_t port_  = 0;
    uint32_t count_ = 0;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<Session> session_ = nullptr;
    simple_rtmp::executors &pool_;
};
}    // namespace simple_rtmp
#endif    //
