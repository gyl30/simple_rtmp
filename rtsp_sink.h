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

namespace simple_rtmp
{
class rtsp_track
{
   public:
    using ptr = std::shared_ptr<rtsp_track>;
    virtual ~rtsp_track() = default;

   public:
    virtual std::string sdp() const = 0;
    virtual std::string id() const = 0;
};

class rtsp_video_track : public rtsp_track
{
   public:
    ~rtsp_video_track() override = default;

   public:
    std::string sdp() const override;
    std::string id() const override;
};

class rtsp_audio_track : public rtsp_track
{
   public:
    ~rtsp_audio_track() override = default;

   public:
    std::string sdp() const override;
    std::string id() const override;
};

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
