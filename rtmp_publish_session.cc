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

rtmp_publish_session::rtmp_publish_session(executors::executor& ex) : ex_(ex), conn_(std::make_shared<tcp_connection>(ex))
{
    LOG_DEBUG("create {}", static_cast<void*>(this));
}
rtmp_publish_session::~rtmp_publish_session()
{
    LOG_DEBUG("destory {}", static_cast<void*>(this));
}

boost::asio::ip::tcp::socket& rtmp_publish_session::socket()
{
    return conn_->socket();
}

void rtmp_publish_session::start()
{
    startup();
    conn_->set_read_cb(std::bind(&rtmp_publish_session::on_read, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->set_write_cb(std::bind(&rtmp_publish_session::on_write, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->start();
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

void rtmp_publish_session::on_write(const boost::system::error_code& ec, std::size_t /*bytes*/)
{
    if (ec && ec == boost::asio::error::bad_descriptor)
    {
        return;
    }

    if (ec)
    {
        shutdown();
        return;
    }
}

void rtmp_publish_session::on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ec && ec == boost::asio::error::bad_descriptor)
    {
        return;
    }

    if (ec)
    {
        shutdown();
        return;
    }
    rtmp_server_input(args_->rtmp, frame->payload.data(), frame->payload.size());
}

void rtmp_publish_session::shutdown()
{
    boost::asio::post(ex_, std::bind(&rtmp_publish_session::safe_shutdown, shared_from_this()));
}

void rtmp_publish_session::safe_shutdown()
{
    if (source_)
    {
        source_.reset();
    }
    if (conn_)
    {
        conn_->shutdown();
        conn_.reset();
    }
}

//
int rtmp_publish_session::rtmp_do_send(void* param, const void* header, size_t len, const void* data, size_t bytes)
{
    auto* self = static_cast<rtmp_publish_session*>(param);
    auto frame = std::make_shared<simple_rtmp::frame_buffer>(len + bytes);
    frame->append(header, len);
    frame->append(data, bytes);
    self->conn_->write_frame(frame);
    return static_cast<int>(len) + static_cast<int>(bytes);
}

int rtmp_publish_session::rtmp_on_publish(void* param, const char* app, const char* stream, const char* type)
{
    auto* self    = static_cast<rtmp_publish_session*>(param);
    self->app_    = app;
    self->stream_ = stream;
    char id[256]  = {0};
    snprintf(id, sizeof(id), "%s_%s", app, stream);
    self->source_ = std::make_shared<rtmp_source>(id, self->ex_);
    LOG_DEBUG("publish app {} stream {} type {}", app, stream, type);
    return 0;
}

int rtmp_publish_session::rtmp_on_script(void* param, const void* script, size_t bytes, uint32_t timestamp)
{
    auto* self = static_cast<rtmp_publish_session*>(param);
    auto frame = std::make_shared<frame_buffer>(bytes);
    frame->append(script, bytes);
    frame->pts   = timestamp;
    frame->dts   = timestamp;
    frame->codec = simple_rtmp::rtmp_tag::script;
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
    frame->codec = simple_rtmp::rtmp_tag::video;
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
    frame->codec = simple_rtmp::rtmp_tag::audio;
    self->source_->write(frame, {});
    return 0;
}
