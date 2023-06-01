#include <iostream>
#include "log.h"
#include "scoped_exit.h"

int main(int argc, char *argv[])
{
    simple_rtmp::init_log(argv[0]);

    DEFER(simple_rtmp::shutdown_log());

    LOG_INFO("simple_rtmp");

    return 0;
}
