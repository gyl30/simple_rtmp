#ifndef SIMPLE_RTMP_RTMP_FORWARD_SESSION_H
#define SIMPLE_RTMP_RTMP_FORWARD_SESSION_H

#include <queue>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include "execution.h"
#include "channel.h"
#include "frame_buffer.h"
#include "sink.h"

namespace simple_rtmp
{
class rtmp_forward_session : public std::enable_shared_from_this<rtmp_forward_session>
{
   public:
    explicit rtmp_forward_session(simple_rtmp::executors::executor& ex);
    ~rtmp_forward_session();

   public:
    void start();
    void shutdown();
    boost::asio::ip::tcp::socket& socket();

   private:
    void startup();
    void do_write(const frame_buffer::ptr& frame);
    void safe_do_write();
    void safe_on_write(const boost::system::error_code& ec, std::size_t bytes);
    void channel_out(const frame_buffer::ptr& frame, const boost::system::error_code& ec);
    //
    void do_read();
    void on_read(const boost::system::error_code& ec, std::size_t bytes);
    void safe_shutdown();

   private:
    static int rtmp_server_send(void*, const void*, size_t, const void*, size_t);
    static int rtmp_server_onplay(void*, const char*, const char*, double, double, uint8_t);
    static int rtmp_server_onpause(void*, int, uint32_t);
    static int rtmp_server_onseek(void*, uint32_t);
    static int rtmp_server_ongetduration(void*, const char*, const char*, double*);

   private:
    uint64_t write_size_ = 0;
    std::string local_addr_;
    std::string remote_addr_;
    std::string stream_id_;
    sink::weak sink_;
    channel::ptr channel_ = nullptr;
    simple_rtmp::executors::executor& ex_;
    boost::asio::ip::tcp::socket socket_{ex_};
    std::vector<frame_buffer::ptr> write_queue_;
    std::vector<frame_buffer::ptr> writing_queue_;
    struct args;
    std::shared_ptr<args> args_;
};
}    // namespace simple_rtmp
#endif    //
