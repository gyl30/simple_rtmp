#include <algorithm>
#include "timer_manger.h"
#include "timestamp.h"

namespace simple_rtmp
{
struct timer_manger::task_args
{
    timer_task task;         // 回调函数
    int repeat = -1;         // 重复次数，-1 一直重复
    bool invalid = false;    // 是否有效
    uint32_t delay = 0;      // 间隔多久一次 毫秒
    uint64_t next = 0;       // 下一次调用的时间
    uint64_t id = 0;
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

uint64_t timer_manger::add_task(int ms, timer_task&& task, int repeat)
{
    const uint64_t now_ms = timestamp::now().milliseconds();
    auto args = new task_args;
    args->next = now_ms + ms;
    args->delay = ms;
    args->task = std::move(task);
    args->repeat = repeat;
    std::lock_guard<std::mutex> const lock(mutex_);
    args->id = task_id_;
    task_id_++;
    tasks_.push_back(args);
    return args->id;
}

void timer_manger::del_task(uint64_t id)
{
    std::lock_guard<std::mutex> const lock(mutex_);
    invalid_ids_.push_back(id);
}

void timer_manger::update()
{
    std::vector<task_args*> tmp_tasks;
    std::vector<uint64_t> tmp_ids;
    {
        std::lock_guard<std::mutex> const lock(mutex_);
        tmp_tasks.swap(tasks_);
        tmp_ids.swap(invalid_ids_);
    }

    if (tmp_tasks.empty())
    {
        return;
    }

    const uint64_t now_ms = timestamp::now().milliseconds();

    for (auto&& args : tmp_tasks)
    {
        // 调用前 重复次数使用完
        if (args->repeat == 0)
        {
            args->invalid = true;
            continue;
        }
        if (args->invalid)
        {
            continue;
        }

        if (!tmp_ids.empty())
        {
            // 已经被删除
            auto it = std::find(tmp_ids.begin(), tmp_ids.end(), args->id);
            if (it != tmp_ids.end())
            {
                args->invalid = true;
                continue;
            }
        }
        // 时间未到
        if (args->next > now_ms)
        {
            continue;
        }
        // 触发回调
        args->task();
        if (args->repeat > 0)
        {
            args->repeat--;
        }
        // 调用后 重复次数使用完
        if (args->repeat == 0)
        {
            args->invalid = true;
            continue;
        }
        // 下次回调时间
        args->next += args->delay;
    }

    // 收集需要删除的回调
    std::vector<task_args*> invalid_tasks;
    for (auto&& args : tmp_tasks)
    {
        if (args->invalid)
        {
            invalid_tasks.push_back(args);
        }
    }

    // 删除无效的回调
    tmp_tasks.erase(std::remove_if(tmp_tasks.begin(), tmp_tasks.end(), [](const task_args* args) { return args->invalid; }), tmp_tasks.end());

    // 释放
    for (auto&& task : invalid_tasks)
    {
        delete task;
    }
    //
    {
        std::lock_guard<std::mutex> const lock(mutex_);
        std::vector<task_args*> tmp;
        tasks_.swap(tmp);
        tasks_.insert(tasks_.end(), tmp.begin(), tmp.end());
    }
}

};    // namespace simple_rtmp
