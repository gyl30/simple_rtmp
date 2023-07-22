#include "rtsp_server_context.h"
#include <boost/algorithm/string.hpp>

using simple_rtmp::rtsp_server_context;

enum RTSP_PARSE_RESULT
{
    RTSP_PARSE_OK = 0,
    RTSP_PARSE_ERROR = -1,
    RTSP_PARSE_CONTINUE = -2
};
static const int kRtcpPrefixLength = 4;

rtsp_server_context::rtsp_server_context(rtsp_server_context_handler handler) : handler_(std::move(handler))
{
    cache_ = fixed_frame_buffer::create();
}

int rtsp_server_context::input(const simple_rtmp::frame_buffer::ptr& frame)
{
    cache_->append(frame);
    int ret = RTSP_PARSE_OK;
    while (!cache_->empty())
    {
        if (cache_->peek() == '$')
        {
            // rtcp
            ret = parse_rtcp_message(cache_);
        }
        else
        {
            ret = parse_rtsp_message(cache_);
        }
        // 需要更多数据
        if (ret == RTSP_PARSE_CONTINUE)
        {
            return 0;
        }
        if (ret == RTSP_PARSE_ERROR)
        {
            return -1;
        }
        assert(ret > 0);
        cache_->erase(ret);
    }
    return ret;
}

int rtsp_server_context::parse_rtsp_message(const simple_rtmp::frame_buffer::ptr& frame)
{
    int ret = parser_.input(frame);
    if (ret == RTSP_PARSE_ERROR)
    {
        return ret;
    }
    // 如果数据不够，重置本次解析状态，等待更多数据到来时继续解析
    if (ret == RTSP_PARSE_CONTINUE)
    {
        parser_.reset();
        return ret;
    }
    // 解析完成
    assert(parser_.complete());
    // 处理消息
    process_request(parser_);
    // 重置解析器
    parser_.reset();
    return ret;
}

int rtsp_server_context::parse_rtcp_message(const simple_rtmp::frame_buffer::ptr& frame)
{
    // 4 byte (1 prefix 1 interleaved 2 length)
    if (frame->size() < kRtcpPrefixLength)
    {
        // 数据不够
        return RTSP_PARSE_CONTINUE;
    }

    const uint8_t* data = frame->data();
    int interleaved = data[1];
    uint32_t length = (data[2] << 8) | data[3];
    if (length < 12)
    {
        return RTSP_PARSE_ERROR;
    }
    if (frame->size() < (length + 4))
    {
        // 需要更多数据
        return RTSP_PARSE_CONTINUE;
    }
    return length + kRtcpPrefixLength;
}

void rtsp_server_context::process_request(const simple_rtmp::rtsp_parser& parser)
{
    std::string method = parser_.method();
    seq_ = atoi(parser_.header("CSeq").data());
    if (method == "OPTIONS")
    {
        options_request(parser);
    }
    else if (method == "DESCRIBE")
    {
        describe_request(parser);
    }
    else if (method == "SETUP")
    {
        setup_request(parser);
    }
    else if (method == "PLAY")
    {
        play_request(parser);
    }
    else if (method == "TEARDOWN")
    {
        teardown_request(parser);
    }
}

void rtsp_server_context::options_request(const simple_rtmp::rtsp_parser& parser)
{
    if (handler_.on_options)
    {
        handler_.on_options(parser.url());
    }
}

void rtsp_server_context::describe_request(const simple_rtmp::rtsp_parser& parser)
{
    if (handler_.on_describe)
    {
        handler_.on_describe(parser.url());
    }
}

simple_rtmp::rtsp_transport parse_transport(const std::string& data)
{
    // Transport: RTP/AVP/TCP;unicast;interleaved=0-1
    simple_rtmp::rtsp_transport result;
    std::vector<std::string> tokens;
    boost::algorithm::split(tokens, data, boost::algorithm::is_any_of(";"));
    for (const auto& token : tokens)
    {
        if (token == "RTP/AVP/TCP")
        {
            result.transport = 1;
        }
        if (token == "multicast")
        {
            result.multicast = 1;
        }
        if (boost::starts_with(token, "interleaved"))
        {
            if (sscanf(token.data(), "interleaved=%d-%d", &result.interleaved1, &result.interleaved2) == 2)
            {
            }
            else if (sscanf(token.data(), "interleaved=%d", &result.interleaved1) == 1)
            {
                result.interleaved2 = result.interleaved1 + 1;
            }
        }

        if (boost::starts_with(token, "client_port"))
        {
            if (sscanf(token.data(), "client_port=%hu-%hu", &result.client_port1, &result.client_port2) == 2)
            {
            }
            else if (sscanf(token.data(), "client_port=%hu", &result.client_port1) == 1)
            {
                result.client_port1 = result.client_port1 / 2 * 2;    // RFC 3550 (p56)
                result.client_port2 = result.client_port1 + 1;
            }
        }
    }
    return result;
}

void rtsp_server_context::setup_request(const simple_rtmp::rtsp_parser& parser)
{
    if (!handler_.on_setup)
    {
        return;
    }
    std::string s = parser.header("Session");
    std::string t = parser.header("Transport");
    rtsp_transport transport = parse_transport(t);
    handler_.on_setup(parser.url(), s, &transport);
}

void rtsp_server_context::play_request(const simple_rtmp::rtsp_parser& parser)
{
    if (!handler_.on_play)
    {
        return;
    }
    std::string s = parser.header("Session");
    handler_.on_play(parser.url(), s);
}

void rtsp_server_context::teardown_request(const simple_rtmp::rtsp_parser& parser)
{
    if (!handler_.on_teardown)
    {
        return;
    }
    std::string s = parser.header("Session");
    handler_.on_teardown(parser.url(), s);
}
