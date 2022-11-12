#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio.hpp>

#include "rtmp_publish_server.h"
#include "rtmp-server.h"
#include "simple_demuxer.h"
#include "source.h"
#include "sink.h"
#include "log.h"

using boost::asio::ip::tcp;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;

struct rtmp_args
{
    rtmp_server_t* rtmp = nullptr;
    std::string app;
    std::vector<uint8_t> write_bytes;
};

rtmp_session::~rtmp_session()
{
    boost::system::error_code ec;
    socket_.close(ec);
}

boost::asio::awaitable<void> rtmp_session::start()
{
    struct rtmp_server_handler_t handler;
    memset(&handler, 0, sizeof(handler));
    handler.send = rtmp_server_send;
    handler.onpublish = rtmp_server_onpublish;
    handler.onscript = rtmp_server_onscript;
    handler.onvideo = rtmp_server_onvideo;
    handler.onaudio = rtmp_server_onaudio;
    args_ = std::make_shared<rtmp_args>();
    args_->rtmp = rtmp_server_create(this, &handler);
    //
    co_await do_start();
}

void rtmp_session::shutdown(std::error_code ec) {}

boost::asio::ip::tcp::socket& rtmp_session::socket() { return socket_; }

boost::asio::awaitable<void> rtmp_session::do_start()
{
    boost::system::error_code ec;
    while (true)
    {
        uint8_t data[4096] = {0};
        auto bytes = co_await socket_.async_read_some(boost::asio::buffer(data), redirect_error(use_awaitable, ec));
        if (ec)
        {
            break;
        }
        // LOG_DEBUG << "recv " << bytes << " bytes";
        rtmp_server_input(args_->rtmp, data, bytes);
        if (args_->write_bytes.empty())
        {
            continue;
        }
        LOG_DEBUG << "do write " << args_->write_bytes.size();
        // clang-format off
        co_await boost::asio::async_write(socket_, boost::asio::buffer(args_->write_bytes), redirect_error(use_awaitable, ec));
        // clang-format on
        if (ec)
        {
            LOG_DEBUG << "do write failed " << ec.message();
            break;
        }
        LOG_DEBUG << "do write " << args_->write_bytes.size() << " ok";
        args_->write_bytes.clear();
    }
}
//
int rtmp_session::rtmp_server_send(void* param, const void* header, size_t len, const void* data, size_t bytes)
{
    auto* self = static_cast<rtmp_session*>(param);
    // clang-format off
    self->args_->write_bytes.insert(self->args_->write_bytes.end(), static_cast<const uint8_t*>(header), static_cast<const uint8_t*>(header) + len);
    self->args_->write_bytes.insert(self->args_->write_bytes.end(), static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + bytes);
    // clang-format on
    LOG_DEBUG << "send " << len + bytes << " bytes";
    return static_cast<int>(len) + static_cast<int>(bytes);
}

int rtmp_session::rtmp_server_onpublish(void* param, const char* app, const char* stream, const char* type)
{
    auto* self = static_cast<rtmp_session*>(param);
    self->args_->app = app;
    self->source_ = std::make_shared<source>(app);
    demuxer::ptr d = std::make_shared<simple_demuxer>(app);

    auto s = std::make_shared<sink>(app);

    d->add_channel(s->channel());
    self->source_->set_demuxer(d);

    source::add(self->source_);
    sink::add(s);

    printf("rtmp_server_onpublish(%s, %s, %s)\n", app, stream, type);
    return 0;
}

int rtmp_session::rtmp_server_onscript(void* param, const void* script, size_t bytes, uint32_t timestamp)
{
    LOG_DEBUG << "on script bytes " << bytes << " timestamp " << timestamp;

    auto* self = static_cast<rtmp_session*>(param);

    auto buff = std::make_shared<buffer>(script, bytes, 0, 0, timestamp, timestamp);

    self->source_->write(buff, {});

    return 0;
}

int rtmp_session::rtmp_server_onvideo(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
    LOG_DEBUG << "on video bytes " << bytes << " timestamp " << timestamp;

    auto* self = static_cast<rtmp_session*>(param);

    auto buff = std::make_shared<buffer>(data, bytes, 0, 0, timestamp, timestamp);

    self->source_->write(buff, {});

    return 0;
}

int rtmp_session::rtmp_server_onaudio(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
    LOG_DEBUG << "on audio bytes " << bytes << " timestamp " << timestamp;

    auto* self = static_cast<rtmp_session*>(param);

    auto buff = std::make_shared<buffer>(data, bytes, 0, 0, timestamp, timestamp);

    self->source_->write(buff, {});

    return 0;
}
