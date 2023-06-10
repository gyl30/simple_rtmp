#ifndef SIMPLE_RTMP_RTMP_SINK_H
#define SIMPLE_RTMP_RTMP_SINK_H

#include <string>
#include <memory>
#include <set>
#include "frame_buffer.h"
#include "channel.h"
#include "sink.h"

namespace simple_rtmp
{
class rtmp_sink : public sink
{
   public:
    rtmp_sink(const std::string& id) : id_(id)
    {
    }

   public:
    std::string id() const override;
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec) override;
    void add_channel(const channel::ptr& ch) override;
    void del_channel(const channel::ptr& ch) override;

   private:
    std::string id_;
    std::set<channel::ptr> chs_;
};

}    // namespace simple_rtmp

#endif
