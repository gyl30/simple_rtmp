#include <iostream>
#include <map>
#include <memory>
#include "http_session.h"
#include "rtmp_publish_session.h"
#include "rtmp_forward_session.h"
#include "rtsp_forward_session.h"
#include "timestamp.h"
#include "execution.h"
#include "tcp_server.h"
#include "scoped_exit.h"
#include "timer_manger.h"
#include "log.h"
#include "api.h"

using simple_rtmp::rtmp_publish_session;
using simple_rtmp::rtmp_forward_session;
using simple_rtmp::rtsp_forward_session;
using simple_rtmp::timer_manger_singleton;
using simple_rtmp::http_session;
using simple_rtmp::tcp_server;

static const std::string kRtmpServerName = "rtmp publish";
static const std::string kRtmpForwardServerName = "rtmp forward";
static const std::string kRtspForwardServerName = "rtsp forward";
static const std::string kHttpServerName = "http service";
static const uint16_t kRtmpPublishPort = 1935;
static const uint16_t kRtmpForwardPort = 1936;
static const uint16_t kRtspForwardPort = 8554;
static const uint16_t kHttpServerPort = 8081;

int main(int argc, char* argv[])
{
    simple_rtmp::init_log(argv[0]);

    DEFER(simple_rtmp::shutdown_log());

    LOG_INFO("simple_rtmp start on {}", simple_rtmp::timestamp::now().fmt_micro_string());

    uint32_t thread_num = std::thread::hardware_concurrency();

    simple_rtmp::executors exs(thread_num);

    std::atomic<bool> stop{false};

    boost::system::error_code ec;

    boost::asio::signal_set signals(exs.get_executor());
    signals.add(SIGINT, ec);
    if (ec)
    {
        LOG_ERROR("add signal failed");
        return -1;
    }

    signals.async_wait([&stop](boost::system::error_code, int) { stop = true; });

    std::make_shared<tcp_server<rtmp_publish_session>>(kRtmpPublishPort, kRtmpServerName, exs.get_executor(), exs)->run();
    std::make_shared<tcp_server<rtmp_forward_session>>(kRtmpForwardPort, kRtmpForwardServerName, exs.get_executor(), exs)->run();
    std::make_shared<tcp_server<rtsp_forward_session>>(kRtspForwardPort, kRtspForwardServerName, exs.get_executor(), exs)->run();
    std::make_shared<tcp_server<http_session>>(kHttpServerPort, kHttpServerName, exs.get_executor(), exs)->run();

    simple_rtmp::register_api();

    timer_manger_singleton::instance().start();
    exs.run();

    while (!stop)
    {
        timer_manger_singleton::instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    exs.stop();
    timer_manger_singleton::instance().stop();

    LOG_INFO("simple_rtmp finish on {}", simple_rtmp::timestamp::now().fmt_micro_string());
    return 0;
}
