#ifndef SIMPLE_RTMP_RTMP_SINK_H
#define SIMPLE_RTMP_RTMP_SINK_H

#include <string>
#include <memory>
#include <set>
#include <utility>
#include "frame_buffer.h"
#include "channel.h"
#include "sink.h"
#include "execution.h"
#include "rtmp_encoder.h"

namespace simple_rtmp
{
class rtmp_sink : public sink
{
   public:
    rtmp_sink(std::string id, simple_rtmp::executors::executor& ex);
    ~rtmp_sink() override = default;

   public:
    std::string id() const override;
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec) override;
    void add_channel(const channel::ptr& ch) override;
    void del_channel(const channel::ptr& ch) override;
    void add_codec(int codec) override;

   private:
    void on_frame(const frame_buffer::ptr& frame, const boost::system::error_code& ec);
    void on_video_frame(const frame_buffer::ptr& frame);
    void on_audio_frame(const frame_buffer::ptr& frame);

    void safe_add_channel(const channel::ptr& ch);
    void safe_del_channel(const channel::ptr& ch);

   private:
    std::string id_;
    simple_rtmp::executors::executor& ex_;
    std::set<channel::ptr> chs_;
    frame_buffer::ptr video_config_;
    frame_buffer::ptr audio_config_;
    std::vector<frame_buffer::ptr> gop_cache_;
    std::shared_ptr<rtmp_encoder> video_encoder_;
    std::shared_ptr<rtmp_encoder> audio_encoder_;
};

}    // namespace simple_rtmp

#endif
