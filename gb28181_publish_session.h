#ifndef SIMPLE_RTMP_GB28181_PUBLISH_SESSION_H
#define SIMPLE_RTMP_GB28181_PUBLISH_SESSION_H

#include <memory>
#include <string>
#include <vector>
#include "execution.h"
#include "frame_buffer.h"
#include "tcp_connection.h"

namespace simple_rtmp
{
class gb28181_publish_session : public std::enable_shared_from_this<gb28181_publish_session>
{
   public:
    explicit gb28181_publish_session(executors::executor& ex);
    ~gb28181_publish_session();

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
    executors::executor& ex_;
    std::shared_ptr<tcp_connection> conn_;
};
}    // namespace simple_rtmp
#endif
