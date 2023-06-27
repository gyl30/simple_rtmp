#include "socket.h"
#include "log.h"

namespace simple_rtmp
{

std::string get_endpoint_address(const boost::asio::ip::udp::endpoint& ed)
{
    boost::system::error_code ec;
    const std::string ip = ed.address().to_string(ec);
    if (ec)
    {
        return "";
    }
    const uint16_t port = ed.port();
    return ip + ":" + std::to_string(port);
}
std::string get_endpoint_address(const boost::asio::ip::tcp::endpoint& ed)
{
    boost::system::error_code ec;
    const std::string ip = ed.address().to_string(ec);
    if (ec)
    {
        return "";
    }
    const uint16_t port = ed.port();
    return ip + ":" + std::to_string(port);
}
std::string get_socket_remote_address(boost::asio::ip::tcp::socket& socket)
{
    boost::system::error_code ec;
    auto ed = socket.remote_endpoint(ec);
    if (ec)
    {
        return "";
    }
    return get_endpoint_address(ed);
}
std::string get_socket_local_address(boost::asio::ip::tcp::socket& socket)
{
    boost::system::error_code ec;
    auto ed = socket.local_endpoint(ec);
    if (ec)
    {
        return "";
    }
    return get_endpoint_address(ed);
}
std::string get_socket_local_address(boost::asio::ip::udp::socket& socket)
{
    boost::system::error_code ec;
    auto ed = socket.local_endpoint(ec);
    if (ec)
    {
        return "";
    }
    return get_endpoint_address(ed);
}
boost::asio::ip::tcp::socket change_socket_io_context(boost::asio::ip::tcp::socket sock, boost::asio::io_context& io)
{
    std::string local = get_socket_local_address(sock);
    std::string remote = get_socket_remote_address(sock);
    boost::system::error_code ec;
    boost::asio::ip::tcp::socket tmp(io);
    auto fd = sock.release(ec);
    if (ec)
    {
        LOG_ERROR("{}<-->{} change thread failed {}", local, remote, ec.message());
        return tmp;
    }
    tmp.assign(boost::asio::ip::tcp::v4(), fd);
    return tmp;
}
}    // namespace simple_rtmp
