#include "flv_forward_session.h"

using simple_rtmp::flv_forward_session;
using simple_rtmp::tcp_connection;

flv_forward_session::flv_forward_session(simple_rtmp::executors::executor& ex) : ex_(ex), conn_(std::make_shared<tcp_connection>(ex_))
{
}

void flv_forward_session::start()
{
}

void flv_forward_session::shutdown()
{
}

boost::asio::ip::tcp::socket& flv_forward_session::socket()
{
    return conn_->socket();
}
