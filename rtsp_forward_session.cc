#include <ctime>
#include <boost/algorithm/string.hpp>
#include "rtsp_forward_session.h"
#include "socket.h"
#include "log.h"
#include "sink.h"
#include "rtsp_sink.h"
#include "frame_buffer.h"
#include "rtsp_server_context.h"
#include "timestamp.h"

using simple_rtmp::rtsp_forward_session;
using namespace std::placeholders;

struct simple_rtmp::rtsp_forward_args
{
    std::string app;
    std::string stream;
    std::shared_ptr<rtsp_server_context> ctx;
};
static std::string make_session_id()
{
    std::atomic<uint64_t> id = 0xff1fcc;
    return std::to_string(id++);
}

rtsp_forward_session::rtsp_forward_session(simple_rtmp::executors::executor& ex) : ex_(ex), conn_(std::make_shared<tcp_connection>(ex_))
{
    LOG_DEBUG("create {}", static_cast<void*>(this));
};

rtsp_forward_session::~rtsp_forward_session()
{
    if (args_)
    {
        args_.reset();
    }
    LOG_DEBUG("destroy {}", static_cast<void*>(this));
}

boost::asio::ip::tcp::socket& rtsp_forward_session::socket()
{
    return conn_->socket();
}
void rtsp_forward_session::start()
{
    rtsp_server_context_handler handler;
    handler.on_options = std::bind(&rtsp_forward_session::on_options, this, _1);
    handler.on_describe = std::bind(&rtsp_forward_session::on_describe, this, _1);
    handler.on_setup = std::bind(&rtsp_forward_session::on_setup, this, _1, _2, _3);
    handler.on_play = std::bind(&rtsp_forward_session::on_play, this, _1, _2);
    handler.on_teardown = std::bind(&rtsp_forward_session::on_teardown, this, _1, _2);

    args_ = std::make_shared<simple_rtmp::rtsp_forward_args>();
    args_->ctx = std::make_shared<simple_rtmp::rtsp_server_context>(std::move(handler));
    channel_ = std::make_shared<simple_rtmp::channel>();
    channel_->set_output(std::bind(&rtsp_forward_session::channel_out, shared_from_this(), _1, _2));
    conn_->set_read_cb(std::bind(&rtsp_forward_session::on_read, shared_from_this(), _1, _2));
    conn_->set_write_cb(std::bind(&rtsp_forward_session::on_write, shared_from_this(), _1, _2));
    conn_->start();
}

void rtsp_forward_session::channel_out(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        LOG_ERROR("channel out {} failed {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
    // 收到编码后的数据包
    conn_->write_frame(frame);
}

void rtsp_forward_session::on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ec)
    {
        LOG_ERROR("read failed {} {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
    args_->ctx->input(frame);
}

void rtsp_forward_session::on_write(const boost::system::error_code& ec, std::size_t /*bytes*/)
{
    if (ec)
    {
        LOG_ERROR("read failed {} {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
}

void rtsp_forward_session::shutdown()
{
    ex_.post(std::bind(&rtsp_forward_session::safe_shutdown, shared_from_this()));
}

void rtsp_forward_session::safe_shutdown()
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
std::string rfc822_now_format()
{
    static const char* s_month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    static const char* s_week[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    time_t t = ::time(nullptr);
    struct tm* tm = gmtime(&t);
    char buf[64] = {0};
    snprintf(buf,
             sizeof(buf),
             "%s, %02d %s %04d %02d:%02d:%02d GMT",
             s_week[(unsigned int)tm->tm_wday % 7],
             tm->tm_mday,
             s_month[(unsigned int)tm->tm_mon % 12],
             tm->tm_year + 1900,
             tm->tm_hour,
             tm->tm_min,
             tm->tm_sec);
    return buf;
}
static simple_rtmp::fixed_frame_buffer::ptr make_461_response(int req)
{
    std::stringstream ss;
    ss << "RTSP/1.0 461 Unsupported Transport\r\n";
    ss << "CSeq: " << req << "\r\n";
    ss << "Date: " << rfc822_now_format().data() << "\r\n";
    ss << "User-Agent: Simple/Rtsp\r\n\r\n";
    std::string response = ss.str();
    return simple_rtmp::fixed_frame_buffer::create(response.data(), response.size());
}

int rtsp_forward_session::on_options(const std::string& url)
{
    LOG_INFO("options {}", url);

    const static char* option_fmt =
        "RTSP/1.0 200 OK\r\n"
        "CSeq: %d\r\n"
        "Date: %s\r\n"
        "Content-Length: 0\r\n"
        "Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, SET_PARAMETER\r\n\r\n";
    char buffer[1024] = {0};
    int n = snprintf(buffer, sizeof buffer, option_fmt, args_->ctx->seq(), rfc822_now_format().data());
    auto frame = fixed_frame_buffer::create(buffer, n);
    conn_->write_frame(frame);
    return 0;
}
int rtsp_forward_session::on_describe(const std::string& url)
{
    LOG_INFO("describe {}", url);
    std::vector<std::string> result;
    boost::algorithm::split(result, url, boost::is_any_of("/"));
    // host app stream
    // rtsp://127.0.0.1:8554/live/test
    if (result.size() < 5)
    {
        shutdown();
        return -1;
    }
    std::string sink_id = "rtsp_" + result[result.size() - 2] + "_" + result[result.size() - 1];
    auto s = simple_rtmp::sink::get(sink_id);
    if (s == nullptr)
    {
        shutdown();
        return -1;
    }
    auto rtsp_s = std::dynamic_pointer_cast<simple_rtmp::rtsp_sink>(s);
    if (rtsp_s == nullptr)
    {
        shutdown();
        return -1;
    }
    rtsp_s->tracks(std::bind(&rtsp_forward_session::on_track, shared_from_this(), url, _1));
    sink_ = s;
    return 0;
}
void rtsp_forward_session::on_track(const std::string& url, std::vector<rtsp_track::ptr> tracks)
{
    uint64_t now = timestamp::now().milliseconds();
    std::stringstream ss;
    ss << "v=0\n";
    ss << "0=- " << now << " " << now << "IN IP4 0.0.0.0\n";
    ss << "s=Simple\n";
    ss << "c=IN IP4 0.0.0.0\n";
    ss << "t=0 0\n";
    ss << "a=range:npt=now-\n";
    ss << "a=control:*\n";
    ss << "a=x-qt-text-nam:Simple/Media\n";
    ss << "a=x-qt-text-inf:" << url << "\n";
    for (const auto& track : tracks)
    {
        if (track == nullptr)
        {
            continue;
        }
        ss << track->sdp() << "\n";
        tracks_[track->id()] = track;
    }
    std::string sdp = ss.str();
    // clear stringstream
    ss.str("");
    ss << "RTSP/1.0 200 OK\r\n";
    ss << "CSeq: " << args_->ctx->seq() << "\r\n";
    ss << "Date: " << rfc822_now_format().data() << "\r\n";
    ss << "Content-Base: " << url << "\r\n";
    ss << "Content-Type: application/sdp\r\n";
    ss << "Content-Length: " << sdp.length() << "\r\n\r\n";
    ss << sdp;
    std::string response = ss.str();
    auto frame = fixed_frame_buffer::create(response.data(), response.size());
    conn_->write_frame(frame);
    LOG_DEBUG("{} describe response -> {}", url, response);
}
int rtsp_forward_session::on_setup(const std::string& url, const std::string& session, rtsp_transport* transport)
{
    LOG_INFO("setup {} session {} transport {}", url, session, transport->transport);
    std::vector<std::string> result;
    boost::algorithm::split(result, url, boost::is_any_of("/"));
    if (result.size() < 5)
    {
        shutdown();
        return -1;
    }
    std::string track_id = result.back();
    if (track_id != kRtspVideoTrackId && track_id != kRtspAudioTrackId)
    {
        LOG_ERROR("{} invalid track id {}", url, track_id);
        shutdown();
        return -1;
    }
    auto track = tracks_[track_id];
    if (track == nullptr)
    {
        LOG_ERROR("{} invalid track id {}", url, track_id);
        shutdown();
        return -1;
    }
    setup_tracks_[track_id] = track;
    if (transport->multicast)
    {
        auto frame = make_461_response(args_->ctx->seq());
        conn_->write_frame(frame);
        LOG_ERROR("{} no support multicast {}", url, track_id);
        shutdown();
        return -1;
    }
    if (transport->transport == 0)
    {
        auto frame = make_461_response(args_->ctx->seq());
        conn_->write_frame(frame);
        LOG_ERROR("{} no support udp {}", url, track_id);
        return -1;
    }
    static const int kRtpVideoChannel = 0;
    static const int kRtcpVideoChannel = 1;
    static const int kRtpAudioChannel = 2;
    static const int kRtcpAudioChannel = 3;

    std::stringstream ss;
    ss << "RTSP/1.0 200 OK\r\n";
    ss << "CSeq: " << args_->ctx->seq() << "\r\n";
    ss << "Date: " << rfc822_now_format().data() << "\r\n";
    ss << "Transport: RTP/AVP/TCP;unicast;destination=" << get_socket_local_ip(conn_->socket()) << ";";
    ss << "source=" << get_socket_remote_ip(conn_->socket()) << ";";
    if (track_id == kRtspVideoTrackId)
    {
        ss << "interleaved=" << kRtpVideoChannel << "-" << kRtcpVideoChannel << "\r\n";
    }
    else
    {
        ss << "interleaved=" << kRtpAudioChannel << "-" << kRtcpAudioChannel << "\r\n";
    }
    if (session_id_.empty())
    {
        session_id_ = make_session_id();
    }
    ss << "Session: " << session_id_ << ";timeout=65\r\n\r\n";
    std::string response = ss.str();
    auto frame = fixed_frame_buffer::create(response.data(), response.size());
    conn_->write_frame(frame);
    LOG_DEBUG("{} setup response -> {}", url, response);
    return 0;
}

int rtsp_forward_session::on_play(const std::string& url, const std::string& session)
{
    if (session != session_id_)
    {
        LOG_ERROR("play {} invalid session {}", url, session);
        shutdown();
        return -1;
    }
    auto s = sink_.lock();
    if (s == nullptr)
    {
        LOG_ERROR("play {} session sink not found {}", url, session);
        shutdown();
        return -1;
    }
    s->add_channel(channel_);
    std::stringstream ss;
    ss << "RTSP/1.0 200 OK\r\n";
    ss << "CSeq: " << args_->ctx->seq() << "\r\n";
    ss << "Date: " << rfc822_now_format().data() << "\r\n";
    ss << "Session: " << session_id_ << "\r\n";
    ss << "RTP-Info: url=" << url << "\r\n";
    std::string response = ss.str();
    auto frame = fixed_frame_buffer::create(response.data(), response.size());
    conn_->write_frame(frame);
    LOG_INFO("play {} session {}", url, session);
    return 0;
}
int rtsp_forward_session::on_teardown(const std::string& url, const std::string& session)
{
    LOG_INFO("play {} teardown {}", url, session);
    return 0;
}
