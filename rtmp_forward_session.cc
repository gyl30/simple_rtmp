#include "rtmp_forward_session.h"
#include "rtmp_codec.h"
#include "socket.h"
#include "log.h"
#include "sink.h"
#include "frame_buffer.h"
#include "rtmp-server.h"

using simple_rtmp::rtmp_forward_session;

struct simple_rtmp::forward_args
{
    std::string app;
    std::string stream;
    rtmp_server_t* rtmp = nullptr;
};

rtmp_forward_session::rtmp_forward_session(simple_rtmp::executors::executor& ex) : ex_(ex), conn_(std::make_shared<tcp_connection>(ex_)), args_(std::make_shared<simple_rtmp::forward_args>())
{
    LOG_DEBUG("create {}", static_cast<void*>(this));
};

rtmp_forward_session::~rtmp_forward_session()
{
    if (args_->rtmp != nullptr)
    {
        rtmp_server_destroy(args_->rtmp);
    }
    if (args_)
    {
        args_.reset();
    }
    LOG_DEBUG("destroy {}", static_cast<void*>(this));
}

boost::asio::ip::tcp::socket& rtmp_forward_session::socket()
{
    return conn_->socket();
}

void rtmp_forward_session::start()
{
    struct rtmp_server_handler_t handler;
    memset(&handler, 0, sizeof(handler));
    handler.send          = rtmp_server_send;
    handler.onplay        = rtmp_server_onplay;
    handler.onpause       = rtmp_server_onpause;
    handler.onseek        = rtmp_server_onseek;
    handler.ongetduration = rtmp_server_ongetduration;

    args_->rtmp = rtmp_server_create(this, &handler);
    channel_    = std::make_shared<simple_rtmp::channel>();
    channel_->set_output(std::bind(&rtmp_forward_session::channel_out, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->set_read_cb(std::bind(&rtmp_forward_session::on_read, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->set_write_cb(std::bind(&rtmp_forward_session::on_write, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->start();
}

void rtmp_forward_session::channel_out(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        LOG_ERROR("channel out {} failed {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
    if (frame->media == simple_rtmp::rtmp_tag::video)
    {
        rtmp_server_send_video(args_->rtmp, frame->payload.data(), frame->payload.size(), frame->pts);
    }
    else if (frame->media == simple_rtmp::rtmp_tag::audio)
    {
        rtmp_server_send_audio(args_->rtmp, frame->payload.data(), frame->payload.size(), frame->pts);
    }
    else if (frame->media == simple_rtmp::rtmp_tag::script)
    {
        rtmp_server_send_script(args_->rtmp, frame->payload.data(), frame->payload.size(), frame->pts);
    }
}

void rtmp_forward_session::on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ec && ec == boost::asio::error::bad_descriptor)
    {
        return;
    }

    if (ec)
    {
        LOG_ERROR("read failed {} {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
    rtmp_server_input(args_->rtmp, frame->payload.data(), frame->payload.size());
}

void rtmp_forward_session::on_write(const boost::system::error_code& ec, std::size_t /*bytes*/)
{
    if (ec && ec == boost::asio::error::bad_descriptor)
    {
        return;
    }

    if (ec)
    {
        LOG_ERROR("read failed {} {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
}

void rtmp_forward_session::shutdown()
{
    ex_.post(std::bind(&rtmp_forward_session::safe_shutdown, shared_from_this()));
}

void rtmp_forward_session::safe_shutdown()
{
    LOG_DEBUG("shutdown {}", static_cast<void*>(this));
    auto s = sink_.lock();
    if (s)
    {
        s->del_channel(channel_);
    }
    if (channel_)
    {
        channel_.reset();
    }

    if (conn_)
    {
        conn_->shutdown();
        conn_.reset();
    }
}

int rtmp_forward_session::rtmp_server_send(void* param, const void* header, size_t len, const void* data, size_t bytes)
{
    auto* self = static_cast<rtmp_forward_session*>(param);
    auto frame = std::make_shared<simple_rtmp::frame_buffer>(len + bytes);
    frame->append(header, len);
    frame->append(data, bytes);
    self->conn_->write_frame(frame);
    return static_cast<int>(len + bytes);
}

int rtmp_forward_session::rtmp_server_onplay(void* param, const char* app, const char* stream, double start, double duration, uint8_t reset)
{
    auto* self = static_cast<rtmp_forward_session*>(param);

    char id[1024] = {0};
    snprintf(id, sizeof id, "rtmp_%s_%s", app, stream);

    LOG_DEBUG("play {} app {} stream {} id {} start {} duration {} reset {} ", param, app, stream, id, start, duration, reset);

    auto s = sink::get(id);
    if (!s)
    {
        LOG_ERROR("not found sink {}", id);
        self->shutdown();
        return 0;
    }
    self->sink_         = s;
    self->stream_id_    = id;
    self->args_->app    = app;
    self->args_->stream = stream;
    s->add_channel(self->channel_);
    return 0;
}

int rtmp_forward_session::rtmp_server_onpause(void* param, int pause, uint32_t ms)
{
    auto* self = static_cast<rtmp_forward_session*>(param);
    LOG_DEBUG("pause {} {} {}ms ", param, pause, ms);
    return 0;
}

int rtmp_forward_session::rtmp_server_onseek(void* param, uint32_t ms)
{
    auto* self = static_cast<rtmp_forward_session*>(param);
    LOG_DEBUG("seek {} {}ms", param, ms);
    return 0;
}

int rtmp_forward_session::rtmp_server_ongetduration(void* param, const char* app, const char* stream, double* duration)
{
    auto* self = static_cast<rtmp_forward_session*>(param);

    LOG_DEBUG("get duration {} app {} stream {}", param, app, stream);
    *duration = 30 * 60;
    return 0;
}
