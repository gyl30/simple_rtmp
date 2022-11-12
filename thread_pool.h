#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <boost/asio.hpp>

class io_context_pool
{
   public:
    explicit io_context_pool(std::size_t pool_size);
    ~io_context_pool();

   public:
    void run();
    void stop();
    boost::asio::io_context &get_context();

   private:
    using ioc_work_t = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    int index = 0;
    std::vector<std::thread> threads;
    std::vector<boost::asio::io_context> ios;
    std::vector<ioc_work_t> works;
    std::mutex mutex_;
};

#endif    // __THREAD_POOL_H__
