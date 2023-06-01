#include <cstdio>
#include "error.h"

std::string errno_to_str()
{
    char buf[BUFSIZ] = {0};
    int ret          = strerror_r(errno, buf, sizeof buf);
    if (ret != 0)
    {
        return "unknown error";
    }
    return buf;
}
