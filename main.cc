#include <iostream>
#include "log.h"
#include "timestamp.h"
#include "scoped_exit.h"

int main(int argc, char *argv[])
{
    simple_rtmp::init_log(argv[0]);

    DEFER(simple_rtmp::shutdown_log());

    LOG_INFO("simple_rtmp start on {}", simple_rtmp::timestamp::now().fmt_micro_string());

    LOG_INFO("simple_rtmp finish on {}", simple_rtmp::timestamp::now().fmt_micro_string());
    return 0;
}
