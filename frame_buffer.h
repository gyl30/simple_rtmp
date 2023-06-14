#ifndef SIMPLE_RTMP_FRAME_BUFFER_H
#define SIMPLE_RTMP_FRAME_BUFFER_H

#include <cstdint>
#include <vector>
#include <memory>
#include <any>

namespace simple_rtmp
{

struct frame_buffer
{
    using ptr     = std::shared_ptr<frame_buffer>;
    int32_t media = 0;
    int32_t codec = 0;
    int32_t flag  = 0;
    int64_t pts   = 0;
    int64_t dts   = 0;
    std::vector<uint8_t> payload;
    std::any context;
    //
    frame_buffer() = default;
    explicit frame_buffer(std::size_t size)
    {
        payload.reserve(size);
    }
    frame_buffer(const uint8_t* data, std::size_t size) : payload(data, data + size)
    {
    }

    void append(const ptr& f)
    {
        media   = f->media;
        codec   = f->codec;
        flag    = f->flag;
        pts     = f->pts;
        dts     = f->dts;
        context = f->context;
        append(f->payload);
    }
    void append(const uint8_t* data, size_t len)
    {
        payload.insert(payload.end(), data, data + len);
    }
    void append(const void* data, size_t len)
    {
        append(static_cast<const uint8_t*>(data), len);
    }

    void append(const std::vector<uint8_t>& data)
    {
        payload.insert(payload.end(), data.begin(), data.end());
    }
    void resize(size_t size)
    {
        payload.resize(size);
    }
};
}    // namespace simple_rtmp

#endif
