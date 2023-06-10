#include "rtmp_forward_session.h"
#include "rtmp_codec.h"
#include "socket.h"
#include "log.h"
#include "sink.h"
#include "frame_buffer.h"
#include "rtmp-server.h"

using simple_rtmp::rtmp_forward_session;

const static auto kRecvMaxSize = 64 * 1024;

struct simple_rtmp::rtmp_forward_session::args
{
    std::string app;
    std::string stream;
    rtmp_server_t* rtmp          = nullptr;
    uint8_t buffer[kRecvMaxSize] = {0};
};

rtmp_forward_session::rtmp_forward_session(simple_rtmp::executors::executor& ex) : ex_(ex), args_(std::make_shared<simple_rtmp::rtmp_forward_session::args>())
{
    LOG_DEBUG("rtmp forward session create {}", static_cast<void*>(this));
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
    auto s = sink_.lock();
    if (s)
    {
        s->del_channel(channel_);
    }
    LOG_DEBUG("rtmp forward session destroy {}", static_cast<void*>(this));
}

boost::asio::ip::tcp::socket& rtmp_forward_session::socket()
{
    return socket_;
}

void rtmp_forward_session::start()
{
    local_addr_  = get_socket_local_address(socket_);
    remote_addr_ = get_socket_remote_address(socket_);

    LOG_DEBUG("rtmp forward session start {} {} <--> {}", static_cast<void*>(this), local_addr_, remote_addr_);

    struct rtmp_server_handler_t handler;
    memset(&handler, 0, sizeof(handler));
    handler.send          = rtmp_server_send;
    handler.onplay        = rtmp_server_onplay;
    handler.onpause       = rtmp_server_onpause;
    handler.onseek        = rtmp_server_onseek;
    handler.ongetduration = rtmp_server_ongetduration;

    auto self = shared_from_this();

    args_->rtmp = rtmp_server_create(this, &handler);
    channel_    = std::make_shared<simple_rtmp::channel>();
    channel_->set_output(std::bind(&rtmp_forward_session::channel_out, self, std::placeholders::_1, std::placeholders::_2));
    do_read();
}

void rtmp_forward_session::channel_out(const frame_buffer::ptr& buf, const boost::system::error_code& ec)
{
    if (ec)
    {
        LOG_ERROR("rtmp forward session channel_out {} failed {}", static_cast<void*>(this), ec.message());
        return;
    }
    auto self = shared_from_this();
    ex_.post(std::bind(&rtmp_forward_session::do_write, self, buf));
}

void rtmp_forward_session::do_write(const frame_buffer::ptr& buff)
{
    write_queue_.push_back(buff);
    safe_do_write();
}

void rtmp_forward_session::safe_do_write()
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
        bufs.emplace_back(boost::asio::buffer(frame->payload));
    }
    auto fn = [bufs, this, self](boost::system::error_code ec, std::size_t bytes) { safe_on_write(ec, bytes); };
    boost::asio::async_write(socket_, bufs, fn);
}

void rtmp_forward_session::safe_on_write(const boost::system::error_code& ec, std::size_t bytes)
{
    if (ec)
    {
        LOG_ERROR("rtmp forward session write failed {} {} <--> {} {}", static_cast<void*>(this), local_addr_, remote_addr_, ec.message());
        shutdown();
        return;
    }
    LOG_DEBUG("rtmp forward session {} {} <--> {} write {} bytes", static_cast<void*>(this), local_addr_, remote_addr_, bytes);

    writing_queue_.clear();
    write_size_ += bytes;
    safe_do_write();
}

void rtmp_forward_session::do_read()
{
    auto fn = [this, self = shared_from_this()](boost::system::error_code ec, std::size_t bytes) { on_read(ec, bytes); };
    socket_.async_read_some(boost::asio::buffer(args_->buffer, kRecvMaxSize), fn);
}

void rtmp_forward_session::on_read(const boost::system::error_code& ec, std::size_t bytes)
{
    if (ec)
    {
        LOG_ERROR("rtmp forward session read failed {} {} <--> {} {}", static_cast<void*>(this), local_addr_, remote_addr_, ec.message());
        shutdown();
        return;
    }
    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(), [self, this, ec, bytes]() { safe_on_read(ec, bytes); });
}

void rtmp_forward_session::safe_on_read(const boost::system::error_code& ec, std::size_t bytes)
{
    if (ec)
    {
        shutdown();
        return;
    }
    rtmp_server_input(args_->rtmp, args_->buffer, bytes);
    do_read();
}

void rtmp_forward_session::shutdown()
{
    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(), [this, self]() { safe_shutdown(); });
}

void rtmp_forward_session::safe_shutdown()
{
    LOG_DEBUG("rtmp forward session shutdown {} {} <--> {}", static_cast<void*>(this), local_addr_, remote_addr_);

    boost::system::error_code ec;
    socket_.close(ec);
}

int rtmp_forward_session::rtmp_server_send(void* param, const void* header, size_t len, const void* data, size_t bytes)
{
    auto* self = static_cast<rtmp_forward_session*>(param);
    auto frame = std::make_shared<simple_rtmp::frame_buffer>(len + bytes);
    frame->append(header, len);
    frame->append(data, bytes);
    self->do_write(frame);
    return len + bytes;
}

int rtmp_forward_session::rtmp_server_onplay(void* param, const char* app, const char* stream, double start, double duration, uint8_t reset)
{
    auto* self = static_cast<rtmp_forward_session*>(param);

    char id[1024] = {0};
    snprintf(id, sizeof id, "rtmp_%s_%s", app, stream);
    LOG_DEBUG("rtmp forward session on play {} {} <--> {} app {} stream {} id {} start {} duration {} reset {} ", param, self->local_addr_, self->remote_addr_, app, stream, id, start, duration, reset);

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
    LOG_DEBUG("rtmp forward session on pause {} {} <--> {} pause {} ms {} ", param, self->local_addr_, self->remote_addr_, pause, ms);
    return 0;
}

int rtmp_forward_session::rtmp_server_onseek(void* param, uint32_t ms)
{
    auto* self = static_cast<rtmp_forward_session*>(param);
    LOG_DEBUG("rtmp forward session on seek {} {} <--> {} ms {} ", param, self->local_addr_, self->remote_addr_, ms);
    return 0;
}

int rtmp_forward_session::rtmp_server_ongetduration(void* param, const char* app, const char* stream, double* duration)
{
    auto* self = static_cast<rtmp_forward_session*>(param);

    LOG_DEBUG("rtmp forward session on get duration {} {} <--> {} app {} stream {}", param, self->local_addr_, self->remote_addr_, app, stream);
    *duration = 30 * 60;
    return 0;
}
