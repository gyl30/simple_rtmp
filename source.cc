#include <map>
#include <mutex>
#include <utility>
#include "source.h"

std::unordered_map<std::string, source::ptr> source::demuxers;
void source::del(const source::ptr& s) { demuxers.erase(s->id()); }
void source::add(const source::ptr& s) { demuxers.emplace(s->id(), s); }
source::ptr source::get(const std::string& id)
{
    auto it = demuxers.find(id);
    if (it == demuxers.end())
    {
        return {};
    }
    return it->second;
}

////////////////////////////////////////////////////////////////////

void source::set_demuxer(demuxer::ptr& d) { demuxer_ = d; }

void source::write(const buffer::ptr& bytes, std::error_code ec) { demuxer_->write(bytes, ec); }
