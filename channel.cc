#include "channel.h"

using simple_rtmp::channel;

void channel::set_output(output&& out)
{
    output_ = out;
}

void channel::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (output_)
    {
        output_(frame, ec);
    }
}
