#ifndef __SIMPLE_DEMUXER_H__
#define __SIMPLE_DEMUXER_H__

#include <string>
#include <set>
#include <utility>
#include "demuxer.h"
#include "channel.h"
#include "buffer.h"

class simple_demuxer : public demuxer
{
   public:
    explicit simple_demuxer(std::string id) : id_(std::move(id)) {}
    ~simple_demuxer() override = default;

   public:
    std::string id() const override { return id_; }
    void write(const buffer::ptr &buff, std::error_code ec) override;
    void add_channel(const channel::ptr &ch) override { channels_.insert(ch); }

   private:
    std::string id_;
    uint32_t dts_ = 0;
    std::set<channel::ptr> channels_;
};

#endif    // __SIMPLE_DEMUXER_H__
