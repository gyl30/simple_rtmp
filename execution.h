#ifndef SIMPLE_RTMP_EXECUTION_H
#define SIMPLE_RTMP_EXECUTION_H

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <boost/asio.hpp>

namespace simple_rtmp
{
class executors
{
   public:
    using executor = boost::asio::io_context;

   public:
    explicit executors(std::size_t pool_size);
    ~executors();

   public:
    void run();
    void stop();
    executor &get_executor();

   private:
    using exec_work_t = boost::asio::executor_work_guard<executor::executor_type>;
    uint32_t index_ = 0;
    std::vector<std::thread> threads_;
    std::vector<executor> exs_;
    std::vector<exec_work_t> works_;
    std::mutex mutex_;
};
}    // namespace simple_rtmp

#endif
