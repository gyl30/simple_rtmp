#ifndef SIMPLE_RTMP_RTSP_FORWARD_SESSION_H
#define SIMPLE_RTMP_RTSP_FORWARD_SESSION_H

#include <queue>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <map>
#include "execution.h"
#include "channel.h"
#include "frame_buffer.h"
#include "rtsp_track.h"
#include "sink.h"
#include "tcp_connection.h"

namespace simple_rtmp
{
struct rtsp_transport;

class rtsp_forward_session : public std::enable_shared_from_this<rtsp_forward_session>
{
   public:
    explicit rtsp_forward_session(simple_rtmp::executors::executor& ex);
    ~rtsp_forward_session();

   public:
    void start();
    void shutdown();
    boost::asio::ip::tcp::socket& socket();

   private:
    void startup();
    void on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec);
    void on_write(const boost::system::error_code& ec, std::size_t bytes);
    void channel_out(const frame_buffer::ptr& frame, const boost::system::error_code& ec);
    void safe_shutdown();
    void send_video_rtcp(const frame_buffer::ptr& frame);
    void send_audio_rtcp(const frame_buffer::ptr& frame);
   private:
    int on_options(const std::string& url);
    int on_describe(const std::string& url);
    int on_setup(const std::string& url, const std::string& session, rtsp_transport* transport);
    int on_play(const std::string& url, const std::string& session);
    int on_teardown(const std::string& url, const std::string& session);
    void on_track(const std::string& url, std::vector<rtsp_track::ptr> track);
    int on_rtcp(int channel, const simple_rtmp::frame_buffer::ptr& frame);

   private:
    std::string stream_id_;
    std::string session_id_;
    sink::weak sink_;
    channel::ptr channel_ = nullptr;
    std::map<std::string, rtsp_track::ptr> tracks_;
    rtsp_track::ptr audio_track_ = nullptr;
    rtsp_track::ptr video_track_ = nullptr;
    void* video_rtcp_ctx_ = nullptr;
    void* audio_rtcp_ctx_ = nullptr;
    uint64_t video_rtcp_time_ = 0;
    uint64_t audio_rtcp_time_ = 0;
    simple_rtmp::executors::executor& ex_;
    std::shared_ptr<tcp_connection> conn_;
    std::vector<frame_buffer::ptr> write_queue_;
    std::vector<frame_buffer::ptr> writing_queue_;
    std::shared_ptr<struct rtsp_forward_args> args_;
};

}    // namespace simple_rtmp

#endif
