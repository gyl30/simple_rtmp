#include <mutex>
#include <map>
#include "sink.h"
#include "log.h"

using simple_rtmp::sink;

std::map<std::string, sink::ptr> sink::sinks_;

std::mutex sink::sinks_mutex_;

sink::ptr sink::get(const std::string& id)
{
    std::lock_guard<std::mutex> const lock(sinks_mutex_);
    auto it = sinks_.find(id);
    if (it == sinks_.end())
    {
        LOG_DEBUG("not found sink {}", id);
        return nullptr;
    }
    LOG_DEBUG("found sink {}", id);
    return it->second;
}

void sink::add(sink::ptr& s)
{
    std::lock_guard<std::mutex> const lock(sinks_mutex_);
    LOG_DEBUG("add sink {}", s->id());
    sinks_[s->id()] = s;
}

void sink::del(const std::string& id)
{
    std::lock_guard<std::mutex> const lock(sinks_mutex_);
    LOG_DEBUG("del sink {}", id);
    sinks_.erase(id);
}
