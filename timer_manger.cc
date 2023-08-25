#include <algorithm>
#include "timer_manger.h"
#include "timestamp.h"

namespace simple_rtmp
{
struct timer_manger::task_args
{
    timer_task task;       // 回调函数
    int repeat = -1;       // 重复次数，-1 一直重复
    uint32_t delay = 0;    // 间隔多久一次 毫秒
    uint64_t next = 0;     // 下一次调用的时间
};

void timer_manger::start()
{
}
void timer_manger::stop()
{
    std::lock_guard<std::mutex> const lock(mutex_);
    for (auto&& task : tasks_)
    {
        delete task;
    }
}

void timer_manger::add_task(int ms, timer_task&& task, int repeat)
{
    const uint64_t now_ms = timestamp::now().milliseconds();
    auto args = new task_args;
    args->next = now_ms + ms;
    args->delay = ms;
    args->task = std::move(task);
    args->repeat = repeat;
    std::lock_guard<std::mutex> const lock(mutex_);
    tasks_.push_back(args);
}

void timer_manger::update()
{
    std::vector<task_args*> tmp;
    {
        std::lock_guard<std::mutex> const lock(mutex_);
        if (tasks_.empty())
        {
            return;
        }
        const uint64_t now_ms = timestamp::now().milliseconds();
        for (auto&& args : tasks_)
        {
            if (args->next > now_ms || args->repeat == 0)
            {
                return;
            }
            args->task();
            if (args->repeat > 0)
            {
                args->repeat--;
            }
            args->next += args->delay;
        }
        //
        for (auto&& args : tasks_)
        {
            if (args->repeat == 0)
            {
                tmp.push_back(args);
            }
        }

        auto it = std::remove_if(tasks_.begin(), tasks_.end(), [](const task_args* args) { return args->repeat == 0; });
        tasks_.erase(it, tasks_.end());
    }
    for (auto&& task : tmp)
    {
        delete task;
    }
}

};    // namespace simple_rtmp
