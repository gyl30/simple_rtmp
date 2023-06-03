#include "log.h"
#include "socket.h"
#include "rtmp_codec.h"
#include "rtmp_publish_session.h"
#include "rtmp-server.h"

using simple_rtmp::rtmp_publish_session;

struct simple_rtmp::publish_args
{
    rtmp_server_t* rtmp = nullptr;
};

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
    local_addr_  = simple_rtmp::get_socket_local_address(socket_);
    remote_addr_ = simple_rtmp::get_socket_remote_address(socket_);
    LOG_DEBUG("{} start local {} remote {}", static_cast<void*>(this), local_addr_, remote_addr_);
    startup();
    do_read();
}
void rtmp_publish_session::startup()
{
    struct rtmp_server_handler_t handler;
    memset(&handler, 0, sizeof(handler));
    handler.send      = rtmp_do_send;
    handler.onpublish = rtmp_on_publish;
    handler.onscript  = rtmp_on_script;
    handler.onvideo   = rtmp_on_video;
    handler.onaudio   = rtmp_on_audio;
    args_             = std::make_shared<publish_args>();
    args_->rtmp       = rtmp_server_create(this, &handler);
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
        LOG_DEBUG("{} write failed local {} remote {} {}", static_cast<void*>(this), local_addr_, remote_addr_, ec.message());
        shutdown();
        return;
    }

    LOG_TRACE("{} local {} remote {} write {} bytes", static_cast<void*>(this), local_addr_, remote_addr_, bytes);

    writing_queue_.clear();

    safe_do_write();
}
void rtmp_publish_session::do_read()
{
    auto fn = [this, self = shared_from_this()](const boost::system::error_code& ec, std::size_t bytes) { on_read(ec, bytes); };
    socket_.async_read_some(boost::asio::buffer(buffer_), fn);
}
void rtmp_publish_session::on_read(const boost::system::error_code& ec, std::size_t bytes)
{
    if (ec)
    {
        LOG_DEBUG("{} read failed local {} remote {} {}", static_cast<void*>(this), local_addr_, remote_addr_, ec.message());
        shutdown();
        return;
    }
    LOG_TRACE("{} local {} remote {} read {} bytes", static_cast<void*>(this), local_addr_, remote_addr_, bytes);
    rtmp_server_input(args_->rtmp, buffer_, bytes);
    //
    do_read();
}
void rtmp_publish_session::shutdown()
{
    LOG_DEBUG("{} shutdown local {} remote {}", static_cast<void*>(this), local_addr_, remote_addr_);
    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(), [this, self]() { safe_shutdown(); });
}

void rtmp_publish_session::safe_shutdown()
{
    LOG_DEBUG("{} safe shutdown local {} remote {}", static_cast<void*>(this), local_addr_, remote_addr_);
    boost::system::error_code ec;
    socket_.close(ec);
}

//
int rtmp_publish_session::rtmp_do_send(void* param, const void* header, size_t len, const void* data, size_t bytes)
{
    auto* self = static_cast<rtmp_publish_session*>(param);
    auto frame = std::make_shared<simple_rtmp::frame_buffer>(len + bytes);
    frame->append(header, len);
    frame->append(data, bytes);
    self->do_write(frame);
    return static_cast<int>(len) + static_cast<int>(bytes);
}

int rtmp_publish_session::rtmp_on_publish(void* param, const char* app, const char* stream, const char* type)
{
    auto* self    = static_cast<rtmp_publish_session*>(param);
    self->app_    = app;
    self->stream_ = stream;
    self->source_ = std::make_shared<rtmp_source>();
    LOG_DEBUG("{} local {} remote {} on publish app {} srteam {} type {}", param, self->local_addr_, self->remote_addr_, app, stream, type);
    return 0;
}

int rtmp_publish_session::rtmp_on_script(void* param, const void* script, size_t bytes, uint32_t timestamp)
{
    auto* self = static_cast<rtmp_publish_session*>(param);
    auto frame = std::make_shared<frame_buffer>(bytes);
    frame->append(script, bytes);
    frame->pts   = timestamp;
    frame->dts   = timestamp;
    frame->codec = simple_rtmp::rtmp_codec::script;
    self->source_->write(frame, {});
    return 0;
}

int rtmp_publish_session::rtmp_on_video(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
    auto* self = static_cast<rtmp_publish_session*>(param);
    auto frame = std::make_shared<frame_buffer>(bytes);
    frame->append(data, bytes);
    frame->pts   = timestamp;
    frame->dts   = timestamp;
    frame->codec = simple_rtmp::rtmp_codec::video;
    self->source_->write(frame, {});
    return 0;
}

int rtmp_publish_session::rtmp_on_audio(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
    auto* self = static_cast<rtmp_publish_session*>(param);
    auto frame = std::make_shared<frame_buffer>(bytes);
    frame->append(data, bytes);
    frame->pts   = timestamp;
    frame->dts   = timestamp;
    frame->codec = simple_rtmp::rtmp_codec::audio;
    self->source_->write(frame, {});
    return 0;
}
