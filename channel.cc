#include "channel.h"
#include <utility>

void channel::set_output(output out) { output_ = std::move(out); }
void channel::write(const buffer::ptr& buff, std::error_code ec)
{
    if (output_)
    {
        output_(buff, ec);
    }
}
