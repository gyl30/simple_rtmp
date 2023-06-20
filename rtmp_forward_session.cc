#include "rtmp_forward_session.h"
#include "rtmp_codec.h"
#include "socket.h"
#include "log.h"
#include "sink.h"
#include "frame_buffer.h"
#include "rtmp_server_context.h"

using simple_rtmp::rtmp_forward_session;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
using std::placeholders::_5;

struct simple_rtmp::forward_args
{
    std::string app;
    std::string stream;
    rtmp_server_context* rtmp_ctx = nullptr;
};

rtmp_forward_session::rtmp_forward_session(simple_rtmp::executors::executor& ex) : ex_(ex), conn_(std::make_shared<tcp_connection>(ex_))
{
    LOG_DEBUG("create {}", static_cast<void*>(this));
};

rtmp_forward_session::~rtmp_forward_session()
{
    if (args_ && args_->rtmp_ctx != nullptr)
    {
        args_->rtmp_ctx->rtmp_server_stop();
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
    rtmp_server_context_handler ctx_handler;
    ctx_handler.send          = std::bind(&rtmp_forward_session::rtmp_server_send, shared_from_this(), _1);
    ctx_handler.onplay        = std::bind(&rtmp_forward_session::rtmp_server_onplay, shared_from_this(), _1, _2, _3, _4, _5);
    ctx_handler.onpause       = std::bind(&rtmp_forward_session::rtmp_server_onpause, shared_from_this(), _1, _2);
    ctx_handler.onseek        = std::bind(&rtmp_forward_session::rtmp_server_onseek, shared_from_this(), _1);
    ctx_handler.ongetduration = std::bind(&rtmp_forward_session::rtmp_server_ongetduration, shared_from_this(), _1, _2, _3);

    args_           = std::make_shared<simple_rtmp::forward_args>();
    args_->rtmp_ctx = new rtmp_server_context(std::move(ctx_handler));
    channel_        = std::make_shared<simple_rtmp::channel>();
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
    if (frame->media() == simple_rtmp::rtmp_tag::video)
    {
        args_->rtmp_ctx->rtmp_server_send_video(frame);
    }
    else if (frame->media() == simple_rtmp::rtmp_tag::audio)
    {
        args_->rtmp_ctx->rtmp_server_send_audio(frame);
    }
    else if (frame->media() == simple_rtmp::rtmp_tag::script)
    {
        args_->rtmp_ctx->rtmp_server_send_script(frame);
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
    args_->rtmp_ctx->rtmp_server_input(frame->data(), frame->size());
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

int rtmp_forward_session::rtmp_server_send(const simple_rtmp::frame_buffer::ptr& frame)
{
    conn_->write_frame(frame);
    return static_cast<int>(frame->size());
}

int rtmp_forward_session::rtmp_server_onplay(const std::string& app, const std::string& stream, double start, double duration, uint8_t reset)
{
    std::string id = "rtmp_" + app + "_" + stream;

    LOG_DEBUG("play app {} stream {} id {} start {} duration {} reset {} ", app, stream, id, start, duration, reset);

    auto s = sink::get(id);
    if (!s)
    {
        LOG_ERROR("not found sink {}", id);
        shutdown();
        return 0;
    }
    sink_         = s;
    stream_id_    = id;
    args_->app    = app;
    args_->stream = stream;
    s->add_channel(channel_);
    return 0;
}

int rtmp_forward_session::rtmp_server_onpause(int pause, uint32_t ms)
{
    LOG_DEBUG("pause {} {} {}ms ", stream_id_, pause, ms);
    return 0;
}

int rtmp_forward_session::rtmp_server_onseek(uint32_t ms)
{
    LOG_DEBUG("seek {} {}ms", stream_id_, ms);
    return 0;
}

int rtmp_forward_session::rtmp_server_ongetduration(const std::string& app, const std::string& stream, double* duration)
{
    LOG_DEBUG("get duration {} app {} stream {}", stream_id_, app, stream);
    *duration = 30 * 60;
    return 0;
}
