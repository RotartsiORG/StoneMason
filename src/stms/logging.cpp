//
// Created by grant on 1/26/20.
//

#include <sys/stat.h>
#include "stms/logging.hpp"

namespace stms {

    std::vector<Error> errorQueue;

    std::string getCurrentDatetime() {
        std::ostringstream oss;
        time_t rawTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        tm timeInfo{};
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
        localtime_s(&timeinfo, &rawtime);
#else
        localtime_r(&rawTime, &timeInfo);
#endif

        oss << std::put_time(&timeInfo, stms::timeFormat);
        return oss.str();
    }

    static void onSPDLogError(const std::string &msg) {
        std::cerr << "[** SPDLOG ERROR **]: " << msg << std::endl;
        STMS_WARN("[** SPDLOG ERROR **]: {}", msg);
    }


#ifdef STMS_ENABLE_LOGGING

    void initLogging() {
        try {
            spdlog::set_error_handler(onSPDLogError);

            if (asyncLogging) {
                spdlog::init_thread_pool(8192, 1);
            }
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

#ifdef __APPLE__
            auto syslog_sink = std::make_shared<spdlog::sinks::syslog_sink_mt>("ident", LOG_PID, LOG_USER, true);
#endif


#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
            auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#endif
            auto now = stms::getCurrentDatetime();

            std::vector<spdlog::sink_ptr> sink_list{console_sink};

            std::shared_ptr<spdlog::sinks::basic_file_sink_mt> latest_file_sink;
            std::shared_ptr<spdlog::sinks::basic_file_sink_mt> unique_file_sink;

            if (logToLatest) {
                latest_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("latest.log", true);
                sink_list.emplace_back(latest_file_sink);
            }

            if (logToUniqueFile) {
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
                mkdir(logDir);
#else
                mkdir(logDir, 0777);
#endif
                unique_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logDir + now + ".log",
                                                                                       true);
                sink_list.emplace_back(unique_file_sink);
            }

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
            sink_list.emplace_back(msvc_sink);
#endif

#ifdef __APPLE__
            sink_list.emplace_back(syslog_sink);
#endif

            std::shared_ptr<spdlog::logger> logger;

            if (asyncLogging) {
                logger = std::make_shared<spdlog::async_logger>("root",
                                                                sink_list.begin(), sink_list.end(),
                                                                spdlog::thread_pool(),
                                                                spdlog::async_overflow_policy::block);
            } else {
                logger = std::make_shared<spdlog::logger>("root", sink_list.begin(), sink_list.end());
            }

            spdlog::register_logger(logger);
            spdlog::set_default_logger(logger);
            spdlog::set_level(spdlog::level::trace);

            spdlog::set_pattern("%v");

            STMS_INFO("{:=^150}", " [NEW LOGGING SESSION | " + now + "] ");

            spdlog::set_pattern(logFormat);

            STMS_INFO("Started StoneMason {}! Compiled on {} at {}. The current time is {}", stms::versionString,
                      __DATE__,
                      __TIME__, now);
        }
        catch (const std::exception &ex) {
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
            STMS_WARN("Log initialization failed: {}", ex.what());
        }
    }

#else
    void initLogging() {};
#endif

}
