#include "log.h"
#include "rtmp_publish_session.h"

using simple_rtmp::rtmp_publish_session;

rtmp_publish_session::rtmp_publish_session(executors::executor& io) : io_(io)
{
    LOG_DEBUG("create {}", static_cast<void*>(this));
}
rtmp_publish_session::~rtmp_publish_session()
{
    LOG_DEBUG("destory {}", static_cast<void*>(this));
}

boost::asio::ip::tcp::socket& rtmp_publish_session::socket()
{
    return socket_;
}

void rtmp_publish_session::start()
{
    LOG_DEBUG("{} start", static_cast<void*>(this));

    startup();
    do_read();
}
void rtmp_publish_session::startup()
{
}

void rtmp_publish_session::do_write(const frame_buffer::ptr& buff)
{
    write_queue_.push_back(buff);
    safe_do_write();
}
void rtmp_publish_session::safe_do_write()
{
    // 有数据正在写
    if (!writing_queue_.empty())
    {
        return;
    }
    // 没有数据需要写
    if (write_queue_.empty())
    {
        return;
    }
    writing_queue_.swap(write_queue_);
    std::vector<boost::asio::const_buffer> bufs;
    bufs.reserve(writing_queue_.size());
    for (const auto& frame : writing_queue_)
    {
        bufs.emplace_back(boost::asio::buffer(frame->payload));
    }

    auto self = shared_from_this();
    auto fn   = [bufs, this, self](const boost::system::error_code& ec, std::size_t bytes) { on_write(ec, bytes); };
    boost::asio::async_write(socket_, bufs, fn);
}
void rtmp_publish_session::on_write(const boost::system::error_code& ec, std::size_t bytes)
{
    if (ec)
    {
        LOG_ERROR("{} write failed {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }

    LOG_TRACE("{} write {} bytes", static_cast<void*>(this), bytes);

    writing_queue_.clear();

    safe_do_write();
}
void rtmp_publish_session::do_read()
{
    // clang-format off
    auto fn = [this, self = shared_from_this()](const boost::system::error_code &ec, std::size_t bytes) { on_read(ec, bytes); };
    socket_.async_read_some(boost::asio::buffer(buffer_), fn);
    // clang-format on
}
void rtmp_publish_session::on_read(const boost::system::error_code& ec, std::size_t bytes)
{
    if (ec)
    {
        shutdown();
        return;
    }
    //
    do_read();
}
void rtmp_publish_session::shutdown()
{
    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(), [this, self]() { safe_shutdown(); });
}

void rtmp_publish_session::safe_shutdown()
{
    LOG_WARN("{} shutdown", static_cast<void*>(this));
    boost::system::error_code ec;
    socket_.close(ec);
}
