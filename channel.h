#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <memory>
#include <functional>
#include <system_error>
#include "buffer.h"

class channel
{
   public:
    using ptr = std::shared_ptr<channel>;

   private:
    using output = std::function<void(const buffer::ptr&, std::error_code)>;

   public:
    void set_output(output out);
    void write(const buffer::ptr&, std::error_code ec);

   private:
    bool ready_ = false;
    output output_;
};

#endif    // __CHANNEL_H__
