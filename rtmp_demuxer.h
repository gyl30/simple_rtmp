#ifndef SIMPLE_RTMP_RTMP_DEMUXER_H
#define SIMPLE_RTMP_RTMP_DEMUXER_H

#include <memory>
#include <string>
#include <boost/system/error_code.hpp>
#include "frame_buffer.h"
#include "channel.h"
#include "rtmp_decoder.h"

namespace simple_rtmp
{
class rtmp_demuxer : public std::enable_shared_from_this<rtmp_demuxer>
{
   public:
    using prt = std::shared_ptr<rtmp_demuxer>;

   public:
    explicit rtmp_demuxer(std::string& id);

   public:
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec);
    void on_codec(const std::function<void(int)>& codec_cb);
    void set_channel(const channel::ptr& ch);

   private:
    void on_frame(const frame_buffer::ptr& frame);
    void demuxer_script(const frame_buffer::ptr& frame);
    void demuxer_video(const frame_buffer::ptr& frame);
    void demuxer_audio(const frame_buffer::ptr& frame);

   private:
    std::string id_;
    channel::ptr ch_;
    std::function<void(int)> codec_cb_;
    std::shared_ptr<rtmp_decoder> audio_decoder_;
    std::shared_ptr<rtmp_decoder> video_decoder_;
};

}    // namespace simple_rtmp

#endif
