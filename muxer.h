#ifndef __MUXER_H__
#define __MUXER_H__

#include <memory>
#include <vector>
#include "buffer.h"
#include "channel.h"

class muxer : public std::enable_shared_from_this<muxer>
{
   public:
    using ptr = std::shared_ptr<muxer>;

   public:
    virtual ~muxer() = default;
    virtual std::string id() const = 0;

   public:
    virtual channel::ptr get_channel() = 0;
    virtual void write(const buffer::ptr &bytes, std::error_code ec) = 0;
};

#endif    // __MUXER_H__
