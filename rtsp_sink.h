#ifndef SIMPLE_RTMP_RTSP_SINK_H
#define SIMPLE_RTMP_RTSP_SINK_H

#include <string>
#include <memory>
#include <set>
#include <vector>
#include <utility>
#include <functional>
#include "frame_buffer.h"
#include "channel.h"
#include "sink.h"
#include "execution.h"
#include "rtsp_encoder.h"
#include "rtsp_track.h"

namespace simple_rtmp
{

class rtsp_sink : public sink
{
   public:
    rtsp_sink(std::string id, simple_rtmp::executors::executor& ex);
    ~rtsp_sink() override = default;

   public:
    std::string id() const override;
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec) override;
    void add_channel(const channel::ptr& ch) override;
    void del_channel(const channel::ptr& ch) override;
    void add_codec(int codec) override;

   public:
    using track_cb = std::function<void(std::vector<rtsp_track::ptr>)>;
    void tracks(track_cb cb);

   private:
    std::string id_;
    simple_rtmp::executors::executor& ex_;
    std::set<channel::ptr> chs_;
    rtsp_track::ptr video_track_;
    rtsp_track::ptr audio_track_;
    std::shared_ptr<rtsp_encoder> video_encoder_;
    std::shared_ptr<rtsp_encoder> audio_encoder_;
};
}    // namespace simple_rtmp
#endif
