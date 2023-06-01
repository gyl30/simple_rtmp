#ifndef SIMPLE_RTMP_LOG_H
#define SIMPLE_RTMP_LOG_H

#define SPDLOG_SHORT_LEVEL_NAMES {"TRC", "DBG", "INF", "WRN", "ERR", "CTL", "OFF"};

#include <string>
#include <spdlog/spdlog.h>

namespace simple_rtmp
{
void init_log(const std::string& app);
void shutdown_log();
}    // namespace simple_rtmp

#define LOG_TRACE(...) SPDLOG_LOGGER_CALL(spdlog::default_logger_raw(), spdlog::level::trace, __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_CALL(spdlog::default_logger_raw(), spdlog::level::debug, __VA_ARGS__)
#define LOG_INFO(...) SPDLOG_LOGGER_CALL(spdlog::default_logger_raw(), spdlog::level::info, __VA_ARGS__)
#define LOG_WARN(...) SPDLOG_LOGGER_CALL(spdlog::default_logger_raw(), spdlog::level::warn, __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_CALL(spdlog::default_logger_raw(), spdlog::level::err, __VA_ARGS__)

#endif
