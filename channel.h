#ifndef SIMPLE_RTMP_CHANNEL_H
#define SIMPLE_RTMP_CHANNEL_H

#include <memory>
#include <functional>
#include <boost/system/error_code.hpp>
#include "frame_buffer.h"

namespace simple_rtmp
{
class channel
{
   public:
    using ptr = std::shared_ptr<channel>;

   private:
    using output = std::function<void(const frame_buffer::ptr &, const boost::system::error_code &)>;

   public:
    void set_output(output &&out);
    void write(const frame_buffer::ptr &, const boost::system::error_code &ec);

   private:
    output output_;
};
}    // namespace simple_rtmp

#endif    //
