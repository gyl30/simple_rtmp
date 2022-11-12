#include "simple_demuxer.h"

void simple_demuxer::write(const buffer::ptr& buff, std::error_code ec)
{
    for (const auto& ch : channels_)
    {
        ch->write(buff, ec);
    }
}
