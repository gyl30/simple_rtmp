#ifndef __SOURCE_H__
#define __SOURCE_H__

#include <memory>
#include <unordered_map>
#include "demuxer.h"
#include "buffer.h"

class source : public std::enable_shared_from_this<source>
{
   public:
    using ptr = std::shared_ptr<source>;

   public:
    explicit source(std::string id) : id_(std::move(id)) {}
    virtual ~source() = default;

   public:
    std::string id() const { return id_; };

   public:
    static void del(const source::ptr&);
    static void add(const source::ptr&);
    static ptr get(const std::string&);

   public:
    void set_demuxer(demuxer::ptr& d);
    void write(const buffer::ptr& bytes, std::error_code ec);

   private:
    std::string id_;
    demuxer::ptr demuxer_;
    static std::unordered_map<std::string, ptr> demuxers;
};

#endif    // __SOURCE_H__
