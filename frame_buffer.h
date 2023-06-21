#ifndef SIMPLE_RTMP_FRAME_BUFFER_H
#define SIMPLE_RTMP_FRAME_BUFFER_H

#include <cstdint>
#include <vector>
#include <memory>
#include <any>

namespace simple_rtmp
{

class frame_buffer
{
   public:
    using ptr = std::shared_ptr<frame_buffer>;

   public:
    virtual ~frame_buffer() = default;

   public:
    virtual uint8_t* data()             = 0;
    virtual const uint8_t* data() const = 0;
    virtual std::size_t size() const    = 0;

   public:
    virtual int32_t media() const         = 0;
    virtual int32_t codec() const         = 0;
    virtual int32_t flag() const          = 0;
    virtual int64_t pts() const           = 0;
    virtual int64_t dts() const           = 0;
    virtual void set_media(int32_t media) = 0;
    virtual void set_codec(int32_t codec) = 0;
    virtual void set_flag(int32_t flag)   = 0;
    virtual void set_pts(int64_t pts)     = 0;
    virtual void set_dts(int64_t dts)     = 0;
};
class ref_frame_buffer : public frame_buffer
{
   public:
    using ptr = std::shared_ptr<ref_frame_buffer>;

   public:
    ref_frame_buffer(const uint8_t* data, std::size_t size, const std::shared_ptr<frame_buffer>& ref) : ref_data_(const_cast<uint8_t*>(data)), ref_data_size_(size), ref_(ref)
    {
    }
    ref_frame_buffer(uint8_t* data, std::size_t size, const std::shared_ptr<frame_buffer>& ref) : ref_data_(data), ref_data_size_(size), ref_(ref)
    {
    }

    ~ref_frame_buffer() override = default;

   public:
    static ptr create(uint8_t* data, std::size_t size, const std::shared_ptr<frame_buffer>& ref)
    {
        ptr f(new ref_frame_buffer(data, size, ref));
        return f;
    }
    static ptr create(const uint8_t* data, std::size_t size, const std::shared_ptr<frame_buffer>& ref)
    {
        ptr f(new ref_frame_buffer(data, size, ref));
        return f;
    }

   public:
    uint8_t* data() override
    {
        return ref_data_;
    }
    const uint8_t* data() const override
    {
        return ref_data_;
    }
    size_t size() const override
    {
        return ref_data_size_;
    }
    int32_t media() const override
    {
        return ref_->media();
    }
    int32_t codec() const override
    {
        return ref_->codec();
    }
    int32_t flag() const override
    {
        return ref_->flag();
    }
    int64_t pts() const override
    {
        return ref_->pts();
    }
    int64_t dts() const override
    {
        return ref_->dts();
    }
    void set_media(int32_t media) override
    {
        ref_->set_media(media);
    }
    void set_codec(int32_t codec) override
    {
        ref_->set_codec(codec);
    }
    void set_flag(int32_t flag) override
    {
        ref_->set_flag(flag);
    }
    void set_pts(int64_t pts) override
    {
        ref_->set_pts(pts);
    }
    void set_dts(int64_t dts) override
    {
        ref_->set_dts(dts);
    }

   private:
    uint8_t* ref_data_                 = nullptr;
    std::size_t ref_data_size_         = 0;
    std::shared_ptr<frame_buffer> ref_ = nullptr;
};

class fixed_frame_buffer : public frame_buffer
{
   public:
    using ptr = std::shared_ptr<fixed_frame_buffer>;

   private:
    int32_t media_ = 0;
    int32_t codec_ = 0;
    int32_t flag_  = 0;
    int64_t pts_   = 0;
    int64_t dts_   = 0;
    std::vector<uint8_t> payload_;

   private:
    //
    fixed_frame_buffer() = default;
    explicit fixed_frame_buffer(std::size_t size)
    {
        payload_.reserve(size);
    }
    fixed_frame_buffer(const uint8_t* data, std::size_t size) : payload_(data, data + size)
    {
    }
    fixed_frame_buffer(const void* data, std::size_t size) : fixed_frame_buffer(static_cast<const uint8_t*>(data), size)
    {
    }

   public:
    ~fixed_frame_buffer() override = default;

   public:
    static ptr create()
    {
        ptr f(new fixed_frame_buffer());
        return f;
    }
    static ptr create(std::size_t size)
    {
        ptr f(new fixed_frame_buffer(size));
        return f;
    }
    static ptr create(const uint8_t* data, std::size_t size)
    {
        ptr f(new fixed_frame_buffer(data, size));
        return f;
    }
    static ptr create(const void* data, std::size_t size)
    {
        ptr f(new fixed_frame_buffer(data, size));
        return f;
    }

    uint8_t* data() override
    {
        return payload_.data();
    }
    const uint8_t* data() const override
    {
        return payload_.data();
    }
    size_t size() const override
    {
        return payload_.size();
    }
    int32_t media() const override
    {
        return media_;
    }
    int32_t codec() const override
    {
        return codec_;
    }
    int32_t flag() const override
    {
        return flag_;
    }
    int64_t pts() const override
    {
        return pts_;
    }
    int64_t dts() const override
    {
        return dts_;
    }
    void set_media(int32_t media) override
    {
        media_ = media;
    }
    void set_codec(int32_t codec) override
    {
        codec_ = codec;
    }
    void set_flag(int32_t flag) override
    {
        flag_ = flag;
    }
    void set_pts(int64_t pts) override
    {
        pts_ = pts;
    }
    void set_dts(int64_t dts) override
    {
        dts_ = dts;
    }

    void append(const uint8_t* data, size_t len)
    {
        if (data == nullptr)
        {
            return;
        }
        payload_.insert(payload_.end(), data, data + len);
    }
    void append(const void* data, size_t len)
    {
        if (data == nullptr)
        {
            return;
        }

        append(static_cast<const uint8_t*>(data), len);
    }

    void append(const frame_buffer::ptr& frame)
    {
        if (!frame)
        {
            return;
        }
        media_ = frame->media();
        codec_ = frame->codec();
        pts_   = frame->pts();
        dts_   = frame->dts();
        flag_  = frame->flag();
        append(frame->data(), frame->size());
    }
    void append(const ptr& frame)
    {
        if (!frame)
        {
            return;
        }
        media_ = frame->media_;
        codec_ = frame->codec_;
        pts_   = frame->pts_;
        dts_   = frame->dts_;
        flag_  = frame->flag_;
        append(frame->payload_);
    }
    void append(const std::vector<uint8_t>& data)
    {
        if (data.empty())
        {
            return;
        }
        payload_.insert(payload_.end(), data.begin(), data.end());
    }
    void resize(size_t size)
    {
        payload_.resize(size);
    }
};
}    // namespace simple_rtmp

#endif
