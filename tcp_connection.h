#ifndef SIMPLE_RTMP_TCP_CONNECTION_H
#define SIMPLE_RTMP_TCP_CONNECTION_H

#include <memory>
#include <functional>
#include "execution.h"
#include "channel.h"
#include "frame_buffer.h"

namespace simple_rtmp
{
class tcp_connection : public std::enable_shared_from_this<tcp_connection>
{
   public:
    explicit tcp_connection(simple_rtmp::executors::executor& ex);
    tcp_connection(simple_rtmp::executors::executor& ex, boost::asio::ip::tcp::socket socket);
    ~tcp_connection();

   public:
    void start();
    void shutdown();
    boost::asio::ip::tcp::socket& socket();

   public:
    using read_cb = std::function<void(const simple_rtmp::frame_buffer::ptr&, boost::system::error_code)>;
    using write_cb = std::function<void(boost::system::error_code, std::size_t)>;
    void set_read_cb(const read_cb& cb);
    void set_write_cb(const write_cb& cb);
    void write_frame(const simple_rtmp::frame_buffer::ptr& frame);

   private:
    void do_read();
    void on_read(const boost::system::error_code& ec, std::size_t bytes);
    void do_write(const frame_buffer::ptr& frame);
    void safe_write_frame(const simple_rtmp::frame_buffer::ptr& frame);
    void safe_do_write();
    void safe_on_write(const boost::system::error_code& ec, std::size_t bytes);
    void safe_shutdown();

   private:
    std::string local_addr_;
    std::string remote_addr_;

    const static int kBufSize = 64 * 1024;
    uint8_t buf_[kBufSize] = {0};
    read_cb read_cb_ = nullptr;
    write_cb write_cb_ = nullptr;
    simple_rtmp::executors::executor& ex_;
    boost::asio::ip::tcp::socket socket_{ex_};
    std::vector<frame_buffer::ptr> write_queue_;
    std::vector<frame_buffer::ptr> writing_queue_;
};

}    // namespace simple_rtmp

#endif
