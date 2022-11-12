#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio.hpp>
#include <map>
#include <memory>
#include "rtmp_publish_server.h"
#include "tcp_server.h"
#include "log.h"
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;

awaitable<void> rtmp_publish_server()
{
    auto executor = co_await boost::asio::this_coro::executor;
    auto s = std::make_shared<tcp_server<rtmp_session>>(8899);
    co_await s->run();
    co_return;
}

int main()
{
    boost::asio::io_context io_context(1);

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);

    signals.async_wait([&](auto, auto) { io_context.stop(); });

    co_spawn(io_context, rtmp_publish_server(), detached);

    boost::system::error_code ec;

    io_context.run(ec);
}
