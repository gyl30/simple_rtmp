#ifndef __SINK_H__
#define __SINK_H__

#include <utility>
#include <vector>
#include <memory>
#include <set>
#include <unordered_map>
#include "muxer.h"
#include "channel.h"

class sink final : public std::enable_shared_from_this<sink>
{
   public:
    using ptr = std::shared_ptr<sink>;

   public:
    explicit sink(std::string id) : id_(std::move(id)) ,channel_(std::make_shared<::channel>()){}
    ~sink() = default;

   public:
    static sink::ptr get(const std::string& id);
    static void add(sink::ptr& s);
    static void del(sink::ptr& s);

   public:
    std::string id() const;
    channel::ptr channel();
    void write(const buffer::ptr& bytes,std::error_code ec);
    void add_muxer(const muxer::ptr& m);
    void del_muxer(const muxer::ptr& m);

   private:
    std::string id_;
    channel::ptr channel_;
    std::set<muxer::ptr> muxers_;
    static std::unordered_map<std::string, sink::ptr> sinks;
};

#endif    // __SINK_H__
