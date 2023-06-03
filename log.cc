#include "log.h"
#include <cstdlib>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <boost/filesystem.hpp>

namespace simple_rtmp
{
static std::string make_log_path(const std::string& app)
{
    auto app_path        = boost::filesystem::canonical(app);
    std::string app_dir  = app_path.parent_path().string();
    std::string app_name = app_path.filename().string();
    std::string log_dir  = app_dir + "/log";
    std::string log_file = app_name + ".log";
    return log_dir + "/" + log_file;
}

void init_log(const std::string& app)
{
    constexpr auto kFileSize  = 50 * 1024 * 1024;
    constexpr auto kFileCount = 5;

    std::string log_file = make_log_path(app);

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file, kFileSize, kFileCount));
    auto logger = std::make_shared<spdlog::logger>("", begin(sinks), end(sinks));
    spdlog::set_default_logger(logger);
    spdlog::flush_every(std::chrono::seconds(3));
    spdlog::set_pattern("%Y%m%d %T.%f %t %L %v %s:%#");
    spdlog::set_level(spdlog::level::debug);
}
void shutdown_log()
{
    spdlog::default_logger()->flush();
    spdlog::shutdown();
}
}    // namespace simple_rtmp
