#ifndef __DEMUXER_H__
#define __DEMUXER_H__

#include <memory>
#include <functional>
#include "channel.h"
#include "buffer.h"

class demuxer : public std::enable_shared_from_this<demuxer>
{
   public:
    using ptr = std::shared_ptr<demuxer>;

   public:
    virtual ~demuxer() = default;

   public:
    virtual std::string id() const = 0;
    virtual void add_channel(const channel::ptr &ch) = 0;
    virtual void write(const buffer::ptr &, std::error_code) = 0;
};

#endif    // __DEMUXER_H__
