#ifndef SIMPLE_RTMP_SINK_H
#define SIMPLE_RTMP_SINK_H

#include <memory>
#include <map>
#include <mutex>
#include <string>
#include "frame_buffer.h"
#include "channel.h"
#include "rtmp_codec.h"

namespace simple_rtmp
{

class sink : public std::enable_shared_from_this<sink>
{
   public:
    using ptr = std::shared_ptr<sink>;
    using weak = std::weak_ptr<sink>;

   public:
    static ptr get(const std::string& id);
    static void add(ptr& s);
    static void del(const std::string& id);

   public:
    sink() = default;
    virtual ~sink() = default;

   public:
    virtual std::string id() const = 0;
    virtual void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec) = 0;
    virtual void add_channel(const channel::ptr& ch) = 0;
    virtual void del_channel(const channel::ptr& ch) = 0;
    virtual void add_codec(int codec, codec_option op) = 0;

   private:
    static std::map<std::string, sink::ptr> sinks_;
    static std::mutex sinks_mutex_;
};

}    // namespace simple_rtmp

#endif    //
