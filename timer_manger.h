#ifndef SIMPLE_RTMP_TIMER_MANGER_H
#define SIMPLE_RTMP_TIMER_MANGER_H

#include <memory>
#include <functional>
#include <vector>
#include <mutex>
#include "singleton.h"

namespace simple_rtmp
{
class timer_manger
{
    using timer_task = std::function<void(void)>;

   public:
    timer_manger() = default;
    ~timer_manger() = default;

   public:
    void start();
    void stop();
    void update();
    // repeat 重复次数，-1 一直重复
    void add_task(int ms, timer_task &&task, int repeat);

   private:
    struct task_args;
    std::mutex mutex_;
    std::vector<task_args *> tasks_;
};
using timer_manger_singleton = singleton<timer_manger>;
}    // namespace simple_rtmp
#endif
