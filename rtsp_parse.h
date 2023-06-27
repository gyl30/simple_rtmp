#ifndef SIMPLE_RTMP_RTSP_PARSE_H
#define SIMPLE_RTMP_RTSP_PARSE_H

#include <string>
#include <map>
#include <boost/optional.hpp>
#include "http_parser.h"
#include "frame_buffer.h"
#include "log.h"

namespace simple_rtmp
{
class rtsp_parse
{
   public:
    rtsp_parse()
    {
        reset();
    }
    ~rtsp_parse() = default;

   private:
    const static int kParseComplete = 1;

   public:
    int input(const frame_buffer::ptr& frame)
    {
        size_t consumed = http_parser_execute(&parser_, &settings_, data, length);
        auto err_code   = static_cast<enum http_errno>(parser_.http_errno);
        if (err_code != HPE_OK)
        {
            return -1;
        }
        if (parse_step_ != kParseComplete)
        {
            // 1 need more data
            return 1;
        }
        return 0;
    }
    std::string url() const
    {
        return url_;
    }
    std::string body() const
    {
        return body_;
    }
    std::string schema() const
    {
        return url_schema_;
    }
    uint16_t port() const
    {
        return url_port_;
    }
    std::string path()
    {
        return url_path_;
    }
    std::string host()
    {
        return url_host_;
    }

    boost::optional<std::string> header(const std::string& key) const
    {
        auto it = headers_.find(key);
        if (it == headers_.end())
        {
            return "";
        }
        return it->second;
    }

   private:
    void reset()
    {
        http_parser_init(&parser_, HTTP_BOTH);
        parser_.data = this;
        // parser_.flags = F_SKIPBODY;
        http_parser_settings_init(&settings_);
        settings_.on_message_begin    = on_message_begin;
        settings_.on_url              = on_url;
        settings_.on_status           = on_status;
        settings_.on_header_field     = on_header_field;
        settings_.on_header_value     = on_header_value;
        settings_.on_headers_complete = on_headers_complete;
        settings_.on_body             = on_body;
        settings_.on_message_complete = on_message_complete;
        field_.clear();
        value_.clear();
        headers_.clear();
        url_.clear();
        parse_step_ = 0;
    }

   private:
    static int on_message_begin(http_parser* parser)
    {
        LOG_DEBUG("on message begin");
        return 0;
    }
    static int on_headers_complete(http_parser* parser)
    {
        LOG_DEBUG("on headers complete");
        auto* self = reinterpret_cast<rtsp_parse*>(parser->data);
        return 0;
    }
    static int on_message_complete(http_parser* parser)
    {
        LOG_DEBUG("on message complete");
        auto* self        = reinterpret_cast<rtsp_parse*>(parser->data);
        self->parse_step_ = kParseComplete;
        return 0;
    }
    static int on_url(http_parser* parser, const char* at, size_t length)
    {
        auto* self = reinterpret_cast<rtsp_parse*>(parser->data);
        if (length > 0)
        {
            self->url_.append(at, at + length);
        }

        return 0;
    }
    static int on_header_field(http_parser* parser, const char* at, size_t length)
    {
        auto* self = reinterpret_cast<rtsp_parse*>(parser->data);
        if (!self->value_.empty())
        {
            self->headers_.emplace(self->field_, self->value_);
            self->field_.clear();
            self->value_.clear();
        }
        if (length > 0)
        {
            std::string str(at, at + length);
            self->field_ += str;
        }
        return 0;
    }
    static int on_header_value(http_parser* parser, const char* at, size_t length)
    {
        auto* self = reinterpret_cast<rtsp_parse*>(parser->data);
        if (length > 0)
        {
            std::string str(at, at + length);
            self->value_ += str;
        }
        return 0;
    }
    static int on_body(http_parser* parser, const char* at, size_t length)
    {
        auto* self = reinterpret_cast<rtsp_parse*>(parser->data);
        if (length > 0)
        {
            std::string str(at, at + length);
            self->body_ += str;
        }
        return 0;
    }

   private:
    int parse_step_ = 0;
    uint16_t url_port_;
    std::string field_;
    std::string value_;
    std::string url_;
    std::string url_schema_;
    std::string url_host_;
    std::string url_path_;
    std::string body_;
    http_parser parser_;
    http_parser_settings settings_;
    std::map<std::string, std::string> headers_;
};

};    // namespace simple_rtmp

#endif
