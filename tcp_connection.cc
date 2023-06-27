#include "tcp_connection.h"
#include "socket.h"
#include "log.h"

using simple_rtmp::tcp_connection;
using namespace std::placeholders;
tcp_connection::tcp_connection(simple_rtmp::executors::executor& ex) : ex_(ex)
{
    LOG_DEBUG("create {}", static_cast<void*>(this));
}

tcp_connection::~tcp_connection()
{
    LOG_DEBUG("destroy {}", static_cast<void*>(this));
}

boost::asio::ip::tcp::socket& tcp_connection::socket()
{
    return socket_;
}

void tcp_connection::start()
{
    local_addr_ = get_socket_local_address(socket_);
    remote_addr_ = get_socket_remote_address(socket_);
    LOG_DEBUG("start {} <--> {}", local_addr_, remote_addr_);
    do_read();
}

void tcp_connection::shutdown()
{
    LOG_DEBUG("shutdown {}", static_cast<void*>(this));
    ex_.post(std::bind(&tcp_connection::safe_shutdown, shared_from_this()));
}

void tcp_connection::safe_shutdown()
{
    if (socket_.is_open())
    {
        LOG_DEBUG("safe shutdown {} <--> {}", local_addr_, remote_addr_);
        boost::system::error_code ec;
        socket_.close(ec);
    }
    else
    {
        LOG_DEBUG("safe shutdown {}", static_cast<void*>(this));
    }
    read_cb_ = nullptr;
    write_cb_ = nullptr;
}

void tcp_connection::set_read_cb(const read_cb& cb)
{
    read_cb_ = cb;
}

void tcp_connection::set_write_cb(const write_cb& cb)
{
    write_cb_ = cb;
}

void tcp_connection::do_read()
{
    socket_.async_read_some(boost::asio::buffer(buf_, kBufSize), std::bind(&tcp_connection::on_read, shared_from_this(), _1, _2));
}

void tcp_connection::on_read(const boost::system::error_code& ec, std::size_t bytes)
{
    auto frame = fixed_frame_buffer::create();
    if (!ec)
    {
        frame->append(buf_, bytes);
    }

    if (read_cb_)
    {
        read_cb_(frame, ec);
    }
    else if (ec)
    {
        LOG_DEBUG("{} <--> {} read failed {}", local_addr_, remote_addr_, ec.message());
        shutdown();
        return;
    }

    LOG_TRACE("{} <--> {} read {} bytes", local_addr_, remote_addr_, bytes);
    do_read();
}

void tcp_connection::write_frame(const simple_rtmp::frame_buffer::ptr& frame)
{
    ex_.post(std::bind(&tcp_connection::safe_write_frame, shared_from_this(), frame));
}

void tcp_connection::safe_write_frame(const simple_rtmp::frame_buffer::ptr& frame)
{
    write_queue_.push_back(frame);
    safe_do_write();
}

void tcp_connection::safe_do_write()
{
    if (!writing_queue_.empty())
    {
        return;
    }
    if (write_queue_.empty())
    {
        return;
    }
    auto self = shared_from_this();
    writing_queue_.swap(write_queue_);
    std::vector<boost::asio::const_buffer> bufs;
    bufs.reserve(writing_queue_.size());
    for (const auto& frame : writing_queue_)
    {
        bufs.emplace_back(boost::asio::buffer(frame->data(), frame->size()));
    }
    boost::asio::async_write(socket_, bufs, std::bind(&tcp_connection::safe_on_write, self, _1, _2));
}

void tcp_connection::safe_on_write(const boost::system::error_code& ec, std::size_t bytes)
{
    writing_queue_.clear();

    if (write_cb_)
    {
        write_cb_(ec, bytes);
    }
    else if (ec)
    {
        LOG_DEBUG("{} <--> {} write failed {}", local_addr_, remote_addr_, ec.message());
        shutdown();
        return;
    }
    LOG_TRACE("{} <--> {} write {} bytes", local_addr_, remote_addr_, bytes);
    safe_do_write();
}
