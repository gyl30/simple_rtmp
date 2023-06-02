#ifndef SIMPLE_RTMP_RTMP_PUBLISH_SESSION_H
#define SIMPLE_RTMP_RTMP_PUBLISH_SESSION_H

#include <memory>
#include <string>
#include <vector>
#include "execution.h"
#include "frame_buffer.h"

namespace simple_rtmp
{
class rtmp_publish_session : public std::enable_shared_from_this<rtmp_publish_session>
{
   public:
    explicit rtmp_publish_session(executors::executor& io);
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
    //
    void do_read();
    void on_read(const boost::system::error_code& ec, std::size_t bytes);
    void on_write(const boost::system::error_code& ec, std::size_t bytes);
    void do_write(const frame_buffer::ptr& frame);
    void safe_do_write();
    void safe_shutdown();

   private:
    std::string stream_;
    executors::executor& io_;
    boost::asio::ip::tcp::socket socket_{io_};
    uint8_t buffer_[1024 * 64] = {0};
    std::vector<frame_buffer::ptr> write_queue_;
    std::vector<frame_buffer::ptr> writing_queue_;
    std::shared_ptr<struct publish_args> args_;
};
}    // namespace simple_rtmp

#endif
