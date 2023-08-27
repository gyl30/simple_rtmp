#ifndef SIMPLE_RTMP_TIMER_MANGER_H
#define SIMPLE_RTMP_TIMER_MANGER_H

#include <memory>
#include <functional>
#include <vector>
#include <mutex>
#include "singleton.h"

namespace simple_rtmp
{
class timer_task_manger
{
    using timer_task = std::function<void(void)>;

   public:
    timer_task_manger() = default;
    ~timer_task_manger() = default;

   public:
    void start();
    void stop();
    void update();
    // repeat 重复次数，-1 一直重复
    // return id
    uint64_t add_task(int ms, timer_task &&task, int repeat);
    void del_task(uint64_t id);

   private:
    struct task_args;
    std::mutex mutex_;
    uint64_t task_id_ = 0;
    std::vector<uint64_t> invalid_ids_;
    std::vector<task_args *> tasks_;
};
// timer task manger signleton
using ttms = singleton<timer_task_manger>;
}    // namespace simple_rtmp
#endif
