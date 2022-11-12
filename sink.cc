#include "sink.h"

std::unordered_map<std::string, sink::ptr> sink::sinks;
void sink::add(sink::ptr& s) { sink::sinks.emplace(s->id(), s); }
void sink::del(sink::ptr& s) { sink::sinks.erase(s->id()); }
sink::ptr sink::get(const std::string& id)
{
    auto it = sink::sinks.find(id);
    if (it == sink::sinks.end())
    {
        return {};
    }
    return it->second;
}

////////////////////////////////////////////////////////////////////

std::string sink::id() const { return id_; }
channel::ptr sink::channel() { return channel_; }
void sink::add_muxer(const muxer::ptr& m) { muxers_.insert(m); }
void sink::del_muxer(const muxer::ptr& m) { muxers_.erase(m); }

void sink::write(const buffer::ptr& bytes, std::error_code ec)
{
    for (const auto& muxer : muxers_)
    {
        muxer->write(bytes, ec);
    }
}
