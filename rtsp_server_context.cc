
#include "rtsp_server_context.h"

using simple_rtmp::rtsp_server_context;

rtsp_server_context::rtsp_server_context(rtsp_server_context_handler handler) : handler_(std::move(handler))
{
}

void rtsp_server_context::input(const simple_rtmp::frame_buffer::ptr& frame)
{
}
