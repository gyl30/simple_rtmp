#include "api.h"
#include "http_session.h"

using simple_rtmp::http_session_ptr;
using simple_rtmp::http_request_ptr;

void hello_world(http_session_ptr& session, http_request_ptr& request)
{
    auto response = session->create_response(request, 200, "Hello World");
    session->write(request, response);
}

void simple_rtmp::register_api()
{
    simple_rtmp::http_session::register_request_cb("/api/v1/hello", std::bind(hello_world, std::placeholders::_1, std::placeholders::_2));
}
