#include "flv_encoder.h"

using simple_rtmp::flv_encoder;

struct flv_encoder::args
{
};

flv_encoder::flv_encoder(std::string id) : id_(std::move(id))
{
}

std::string flv_encoder::id()
{
    return id_;
};

void flv_encoder::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    //
}

void flv_encoder::set_output(const channel::ptr& ch)
{
    //
}
