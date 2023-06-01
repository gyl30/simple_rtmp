#include <thread>
#include <mutex>
#include "execution.h"

using simple_rtmp::executors;

executors::executors(std::size_t pool_size) : exs_(pool_size)
{
}

executors::~executors()
{
    stop();
}

void executors::run()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &io : exs_)
    {
        works_.emplace_back(boost::asio::make_work_guard(io));
        threads_.emplace_back([&io] { io.run(); });
    }
}

void executors::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    works_.clear();
    for (auto &&io : exs_)
    {
        io.stop();
    }
    for (auto &&thread : threads_)
    {
        thread.join();
    }
    threads_.clear();
}

boost::asio::io_context &executors::get_executor()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (index_ == exs_.size())
    {
        index_ = 0;
    }
    return exs_[index_++];
}
