#include "api.h"
#include "http_session.h"
#include "webrtc_sdp.h"
#include "WebRtcPlayer.hpp"

using simple_rtmp::http_session_ptr;
using simple_rtmp::http_request_ptr;

void hello_world(http_session_ptr& session, http_request_ptr& request)
{
    auto response = session->create_response(request, 200, "Hello World");
    std::string body = request->body();
    auto sdp = std::make_shared<simple_rtmp::webrtc_sdp>(body);
    auto player = std::make_shared<WebRtcPlayer>(sdp);
    player->exchangeSdp();
    session->write(request, response);
}

void simple_rtmp::register_api()
{
    simple_rtmp::http_session::register_request_cb("/api/v1/hello", std::bind(hello_world, std::placeholders::_1, std::placeholders::_2));
}
