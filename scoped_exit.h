#ifndef SIMPLE_RTMP_SCOPED_EXIT_H
#define SIMPLE_RTMP_SCOPED_EXIT_H

#include <utility>

namespace simple_rtmp
{
template <typename Callback>
class scoped_exit
{
   public:
    template <typename C>
    explicit scoped_exit(C c) : callback(std::forward<C>(c))
    {
    }

    scoped_exit(scoped_exit&& mv) noexcept : callback(std::move(mv.callback)), canceled(mv.canceled)
    {
        mv.canceled = true;
    }

    scoped_exit(const scoped_exit&) = delete;
    scoped_exit& operator=(const scoped_exit&) = delete;

    ~scoped_exit()
    {
        if (!canceled)
        {
            try
            {
                callback();
            }
            catch (...)
            {
            }
        }
    }

    scoped_exit& operator=(scoped_exit&& mv) noexcept
    {
        if (this != &mv)
        {
            ~scoped_exit();
            callback = std::move(mv.callback);
            canceled = mv.canceled;
            mv.canceled = true;
        }

        return *this;
    }

    void cancel()
    {
        canceled = true;
    }

   private:
    Callback callback;
    bool canceled = false;
};

template <typename Callback>
scoped_exit<Callback> make_scoped_exit(Callback&& c)
{
    return scoped_exit<Callback>(std::forward<Callback>(c));
}
#define SCOPED_CONCAT_(x, y) x##y
#define SCOPED_CONCAT(x, y) SCOPED_CONCAT_(x, y)
#define SCOPED_UNIQUE_NAME(prefix) SCOPED_CONCAT(prefix, __LINE__)
#define DEFER(code) auto SCOPED_UNIQUE_NAME(scoped) = ::simple_rtmp::make_scoped_exit([&]() { code; })
}    // namespace simple_rtmp
#endif    //
