#ifndef SIMPLE_RTMP_RTMP_PUBLISH_SESSION_H
#define SIMPLE_RTMP_RTMP_PUBLISH_SESSION_H

#include <memory>
#include <string>
#include <vector>
#include "execution.h"
#include "rtmp_source.h"
#include "frame_buffer.h"
#include "tcp_connection.h"

namespace simple_rtmp
{
class rtmp_publish_session : public std::enable_shared_from_this<rtmp_publish_session>
{
   public:
    explicit rtmp_publish_session(executors::executor& ex);
    ~rtmp_publish_session();
    rtmp_publish_session(const rtmp_publish_session&)            = delete;
    rtmp_publish_session(rtmp_publish_session&&)                 = delete;
    rtmp_publish_session& operator=(const rtmp_publish_session&) = delete;
    rtmp_publish_session& operator=(rtmp_publish_session&&)      = delete;

   public:
    void start();
    void shutdown();
    boost::asio::ip::tcp::socket& socket();

   private:
    void startup();
    void on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec);
    void on_write(const boost::system::error_code& ec, std::size_t bytes);
    void safe_shutdown();

   private:
    int rtmp_do_send(const simple_rtmp::frame_buffer::ptr& frame);
    int rtmp_on_publish(const std::string& app, const std::string& stream, const std::string& type);
    int rtmp_on_script(const simple_rtmp::frame_buffer::ptr& frame);
    int rtmp_on_video(const simple_rtmp::frame_buffer::ptr& frame);
    int rtmp_on_audio(const simple_rtmp::frame_buffer::ptr& frame);

   private:
    std::string app_;
    std::string stream_;
    rtmp_source::prt source_;
    executors::executor& ex_;
    std::shared_ptr<tcp_connection> conn_;
    std::shared_ptr<struct publish_args> args_;
};
}    // namespace simple_rtmp

#endif
