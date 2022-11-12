#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <vector>
#include <memory>
#include <cstdint>
#include <any>

class buffer
{
   public:
    using ptr = std::shared_ptr<buffer>;

   public:
    buffer() = default;
    ~buffer() = default;

    buffer(const void* data, std::size_t size, int m_type, int s_type, uint32_t pts, uint32_t dts)
        : media_type_(m_type), stream_type_(s_type), pts_(pts), dts_(dts),
          data_(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size)
    {
    }

   public:
    [[nodiscard]] std::size_t size() const;
    uint8_t* data();
    [[nodiscard]] const uint8_t* data() const;
    void erase(ssize_t pos, ssize_t size);
    void clear();

   public:
    void set_stream_type(int type) { stream_type_ = type; }
    void set_media_type(int type) { media_type_ = type; }
    void set_pts(uint32_t pts) { pts_ = pts; }
    void set_dts(uint32_t dts) { dts_ = dts; }
    void set_flag(uint32_t flag) { flag_ = flag; }
    void set_content(const std::any& c) { content_ = c; }

    [[nodiscard]] int stream_type() const { return stream_type_; }
    [[nodiscard]] int media_type() const { return media_type_; }
    [[nodiscard]] uint32_t pts() const { return pts_; }
    [[nodiscard]] uint32_t dts() const { return dts_; }
    [[nodiscard]] uint32_t flag() const { return flag_; }
    [[nodiscard]] std::any get_content() const { return content_; }

   private:
    int media_type_ = -1;
    int stream_type_ = 1;
    uint32_t pts_ = 0;
    uint32_t dts_ = 0;
    uint32_t type_ = 0;
    uint32_t flag_ = 0;
    std::vector<uint8_t> data_;
    std::any content_;
};

#endif
