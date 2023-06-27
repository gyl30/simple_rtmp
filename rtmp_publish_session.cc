#include "rtmp_publish_session.h"
#include "rtmp_server_context.h"
#include "socket.h"
#include "log.h"

using simple_rtmp::rtmp_publish_session;
using namespace std::placeholders;

struct simple_rtmp::publish_args
{
    rtmp_server_context* rtmp_ctx;
};

rtmp_publish_session::rtmp_publish_session(executors::executor& ex) : ex_(ex), conn_(std::make_shared<tcp_connection>(ex))
{
    LOG_DEBUG("create {}", static_cast<void*>(this));
}
rtmp_publish_session::~rtmp_publish_session()
{
    LOG_DEBUG("destroy {}", static_cast<void*>(this));
}

boost::asio::ip::tcp::socket& rtmp_publish_session::socket()
{
    return conn_->socket();
}

void rtmp_publish_session::start()
{
    startup();
    conn_->set_read_cb(std::bind(&rtmp_publish_session::on_read, shared_from_this(), _1, _2));
    conn_->set_write_cb(std::bind(&rtmp_publish_session::on_write, shared_from_this(), _1, _2));
    conn_->start();
}
void rtmp_publish_session::startup()
{
    simple_rtmp::rtmp_server_context_handler ctx_handler;
    ctx_handler.send = std::bind(&rtmp_publish_session::rtmp_do_send, shared_from_this(), _1);
    ctx_handler.onpublish = std::bind(&rtmp_publish_session::rtmp_on_publish, shared_from_this(), _1, _2, _3);
    ctx_handler.onscript = std::bind(&rtmp_publish_session::rtmp_on_script, shared_from_this(), _1);
    ctx_handler.onvideo = std::bind(&rtmp_publish_session::rtmp_on_video, shared_from_this(), _1);
    ctx_handler.onaudio = std::bind(&rtmp_publish_session::rtmp_on_audio, shared_from_this(), _1);
    args_ = std::make_shared<publish_args>();
    args_->rtmp_ctx = new rtmp_server_context(std::move(ctx_handler));
}

void rtmp_publish_session::on_write(const boost::system::error_code& ec, std::size_t /*bytes*/)
{
    if (ec)
    {
        LOG_ERROR("write failed {} {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
}

void rtmp_publish_session::on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ec)
    {
        shutdown();
        return;
    }
    args_->rtmp_ctx->rtmp_server_input(frame->data(), frame->size());
}

void rtmp_publish_session::shutdown()
{
    boost::asio::post(ex_, std::bind(&rtmp_publish_session::safe_shutdown, shared_from_this()));
}

void rtmp_publish_session::safe_shutdown()
{
    if (source_)
    {
        source_->write(nullptr, boost::asio::error::eof);
        source_.reset();
    }
    if (conn_)
    {
        conn_->shutdown();
        conn_.reset();
    }
}

//
int rtmp_publish_session::rtmp_do_send(const simple_rtmp::frame_buffer::ptr& frame)
{
    conn_->write_frame(frame);
    return static_cast<int>(frame->size());
}

int rtmp_publish_session::rtmp_on_publish(const std::string& app, const std::string& stream, const std::string& type)
{
    app_ = app;
    stream_ = stream;
    std::string const id = app + "_" + stream;
    source_ = std::make_shared<rtmp_source>(id, ex_);
    LOG_DEBUG("publish app {} stream {} type {}", app, stream, type);
    return 0;
}

int rtmp_publish_session::rtmp_on_script(const simple_rtmp::frame_buffer::ptr& frame)
{
    source_->write(frame, {});
    return 0;
}

int rtmp_publish_session::rtmp_on_video(const simple_rtmp::frame_buffer::ptr& frame)
{
    source_->write(frame, {});
    return 0;
}

int rtmp_publish_session::rtmp_on_audio(const simple_rtmp::frame_buffer::ptr& frame)
{
    source_->write(frame, {});
    return 0;
}
