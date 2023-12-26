#include <utility>
#include "gb28181_source.h"

using simple_rtmp::gb28181_source;

gb28181_source::gb28181_source(std::string id, simple_rtmp::executors::executor& ex) : id_(std::move(id)), ch_(std::make_shared<simple_rtmp::channel>())
{
    ch_->set_output(std::bind(&gb28181_source::on_frame, this, std::placeholders::_1, std::placeholders::_2));
};

void gb28181_source::on_codec(int codec, codec_option op)
{
}

void gb28181_source::on_frame(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
}

void gb28181_source::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
}

void simple_rtmp::gb28181_source::set_channel(const channel::ptr& ch)
{
    ch_ = ch;
}
