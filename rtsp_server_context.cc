
#include "rtsp_server_context.h"

using simple_rtmp::rtsp_server_context;

rtsp_server_context::rtsp_server_context(rtsp_server_context_handler handler) : handler_(std::move(handler))
{
}

int rtsp_server_context::input(const simple_rtmp::frame_buffer::ptr& frame)
{
    int ret = parser_.input(frame);
    if (ret < 0)
    {
        return ret;
    }
    if (ret == 0)
    {
        process_request(parser_);
    }
    return ret;
}
void rtsp_server_context::process_request(const simple_rtmp::rtsp_parser& parser)
{
    std::string method = parser_.method();
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
}

void rtsp_server_context::describe_request(const simple_rtmp::rtsp_parser& parser)
{
}

void rtsp_server_context::setup_request(const simple_rtmp::rtsp_parser& parser)
{
}

void rtsp_server_context::play_request(const simple_rtmp::rtsp_parser& parser)
{
}

void rtsp_server_context::teardown_request(const simple_rtmp::rtsp_parser& parser)
{
}
