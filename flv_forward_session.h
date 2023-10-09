#ifndef SIMPLE_RTMP_FLV_FORWARD_SESSION_H
#define SIMPLE_RTMP_FLV_FORWARD_SESSION_H

#include <queue>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include "execution.h"
#include "channel.h"
#include "frame_buffer.h"
#include "sink.h"
#include "tcp_connection.h"

namespace simple_rtmp
{
class flv_forward_session : public std::enable_shared_from_this<flv_forward_session>
{
   public:
    explicit flv_forward_session(simple_rtmp::executors::executor& ex);
    ~flv_forward_session();

   public:
    void start();
    void shutdown();
    boost::asio::ip::tcp::socket& socket();

   private:
    sink::weak sink_;
    channel::ptr channel_ = nullptr;
    simple_rtmp::executors::executor& ex_;
    std::shared_ptr<tcp_connection> conn_;
};
}    // namespace simple_rtmp

#endif
