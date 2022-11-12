#include "thread_pool.h"

io_context_pool::io_context_pool(std::size_t pool_size) : ios(pool_size) {}
io_context_pool::~io_context_pool() { stop(); }

void io_context_pool::run()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &io : ios)
    {
        works.emplace_back(boost::asio::make_work_guard(io));
        threads.emplace_back([&io] { io.run(); });
    }
}
void io_context_pool::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    works.clear();
    for (auto &&io : ios)
    {
        io.stop();
    }
    for (auto &&thread : threads)
    {
        thread.join();
    }
    threads.clear();
}
boost::asio::io_context &io_context_pool::get_context()
{
    if (index == ios.size())
    {
        index = 0;
    }
    return ios[index++];
}
