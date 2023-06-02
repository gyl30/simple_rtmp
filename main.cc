#include <iostream>
#include <map>
#include <string>
#include "timestamp.h"
#include "scoped_exit.h"
#include "execution.h"
#include "tcp_server.h"
#include "log.h"

int main(int argc, char* argv[])
{
    simple_rtmp::init_log(argv[0]);

    DEFER(simple_rtmp::shutdown_log());

    LOG_INFO("simple_rtmp start on {}", simple_rtmp::timestamp::now().fmt_micro_string());

    uint32_t thread_num = std::thread::hardware_concurrency();

    simple_rtmp::executors exs(thread_num);
    std::atomic<bool> stop{false};
    static const std::map<int, std::string> kSignals = {
        {SIGABRT, "SIGABRT"},
        {SIGFPE, "SIGFPE"},
        {SIGILL, "SIGILL"},
        {SIGSEGV, "SIGSEGV"},
        {SIGTERM, "SIGTERM"},
        {SIGINT, "SIGINT"},
    };
    boost::asio::signal_set sig(exs.get_executor());
    boost::system::error_code ec;
    for (const auto& pair : kSignals)
    {
        sig.add(pair.first, ec);
        if (ec)
        {
            LOG_WARN("add {} -> {} signal failed {}", pair.first, pair.second, ec.message());
        }
        else
        {
            LOG_INFO("add {} -> {} signal", pair.first, pair.second);
        }
    }
    // 注册退出信号
    int signal_number = 0;
    sig.async_wait(
        [&stop, &signal_number](boost::system::error_code, int s)
        {
            stop          = true;
            signal_number = s;
        });

    exs.run();

    while (!stop)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    if (signal_number != 0)
    {
        LOG_WARN("recv {} signal", signal_number);
    }

    exs.stop();

    LOG_INFO("simple_rtmp finish on {}", simple_rtmp::timestamp::now().fmt_micro_string());
    return 0;
}
