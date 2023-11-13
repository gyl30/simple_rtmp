#include <boost/algorithm/string.hpp>
#include <boost/utility/string_view.hpp>

#include "log.h"
#include "sink.h"
#include "flv-proto.h"
#include "flv-header.h"
#include "flv_forward_session.h"

using simple_rtmp::flv_forward_session;
using simple_rtmp::tcp_connection;

static simple_rtmp::frame_buffer::ptr make_flv_header()
{
    static const auto kFlvHeaderSize = 9;
    uint8_t header[kFlvHeaderSize + 4];
    flv_header_write(1, 1, header, kFlvHeaderSize);
    flv_tag_size_write(header + kFlvHeaderSize, 4, 0);
    return simple_rtmp::fixed_frame_buffer::create(header, sizeof(header));
}

flv_forward_session::flv_forward_session(std::string target, simple_rtmp::executors::executor& ex, boost::asio::ip::tcp::socket socket)
    : target_(std::move(target)), ex_(ex), conn_(std::make_shared<tcp_connection>(ex_, std::move(socket)))

{
}

void flv_forward_session::write(const frame_buffer::ptr& frame)
{
    conn_->write_frame(frame);
}

static std::string make_session_id_suffix(const std::string& target)
{
    //
    boost::string_view v(target);
    v.remove_suffix(4);
    std::vector<std::string> results;
    boost::split(results, v, boost::is_any_of("/"));
    if (results.size() < 2)
    {
        return "";
    }
    auto app_index = results.size() - 2;
    auto stream_index = results.size() - 1;
    std::string app = results[app_index];
    std::string stream = results[stream_index];
    return "flv_" + app + "_" + stream;
}

static std::string make_session_id_prefix(const std::string& target)
{
    //
    std::vector<std::string> results;
    boost::split(results, target, boost::is_any_of("/"));
    if (results.size() < 3)
    {
        return "";
    }
    auto app_index = results.size() - 2;
    auto stream_index = results.size() - 1;
    std::string app = results[app_index];
    std::string stream = results[stream_index];
    return "flv_" + app + "_" + stream;
}
void flv_forward_session::start()
{
    if (boost::ends_with(target_, ".flv"))
    {
        id_ = make_session_id_suffix(target_);
    }
    else if (boost::starts_with(target_, "/flv"))
    {
        id_ = make_session_id_prefix(target_);
    }
    else
    {
        LOG_ERROR("invalid flv target {}", target_);
        shutdown();
        return;
    }

    auto s = sink::get(id_);
    if (!s)
    {
        LOG_ERROR("{} not found sink {}", target_, id_);
        shutdown();
        return;
    }
    LOG_DEBUG("{} start", id_);
    sink_ = s;

    channel_ = std::make_shared<simple_rtmp::channel>();
    channel_->set_output(std::bind(&flv_forward_session::channel_out, shared_from_this(), std::placeholders::_1, std::placeholders::_2));

    conn_->set_read_cb(std::bind(&flv_forward_session::on_read, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->set_write_cb(std::bind(&flv_forward_session::on_write, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->start();
    sink_ = s;
    s->add_channel(channel_);
}

void flv_forward_session::on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec)
{
    (void)frame;
    if (ec)
    {
        LOG_ERROR("read failed {} {}", id_, ec.message());
        shutdown();
        return;
    }
}

void flv_forward_session::on_write(const boost::system::error_code& ec, std::size_t /*bytes*/)
{
    if (ec)
    {
        LOG_ERROR("{} write failed {}", id_, ec.message());
        shutdown();
        return;
    }
}
void flv_forward_session::shutdown()
{
    ex_.post(std::bind(&flv_forward_session::safe_shutdown, shared_from_this()));
}

boost::asio::ip::tcp::socket& flv_forward_session::socket()
{
    return conn_->socket();
}

void flv_forward_session::safe_shutdown()
{
    LOG_DEBUG("{} shutdown", id_);
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

void flv_forward_session::channel_out(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        LOG_ERROR("{} flv sink failed {}", id_, ec.message());
        shutdown();
        return;
    }
    auto type = FLV_TYPE_VIDEO;
    if (frame->media() == simple_rtmp::audio)
    {
        type = FLV_TYPE_AUDIO;
    }
    if (!first_frame_)
    {
        first_frame_ = true;
        auto frame = make_flv_header();
        write(frame);
    }

    static const auto kFlvTagHeaderSize = 11;
    uint8_t buf[kFlvTagHeaderSize + 4];
    struct flv_writer_t* flv;
    struct flv_tag_header_t tag;

    memset(&tag, 0, sizeof(tag));
    tag.size = frame->size();
    tag.type = type;
    tag.timestamp = frame->pts();
    flv_tag_header_write(&tag, buf, kFlvTagHeaderSize);
    flv_tag_size_write(buf + kFlvTagHeaderSize, 4, frame->size() + kFlvTagHeaderSize);

    auto tag_header = simple_rtmp::fixed_frame_buffer::create(buf, kFlvTagHeaderSize);
    auto tag_size = simple_rtmp::fixed_frame_buffer::create(buf + kFlvTagHeaderSize, 4);
    write(tag_header);
    write(frame);
    write(tag_size);
}
