//
// Created by grant on 1/26/20.
//

#include "stms/logging.hpp"

#include <chrono>
#include <sys/stat.h>

namespace stms {

    LogRecord::LogRecord(LogLevel lvl, std::chrono::system_clock::time_point iTime, const char *iFile, unsigned int iLine)
                            : level(lvl), time(iTime), file(iFile), line(iLine) {}

#ifdef STMS_ENABLE_LOGGING
    static FILE *&getLatestLogFile() {
        static FILE *fp = nullptr;
        return fp;
    }

    static FILE *&getUniqueLogFile() {
        static FILE *fp = nullptr;
        return fp;
    }

    void quitLogging() {
        if (logToLatestLog) {
            std::fclose(getLatestLogFile());
        }

        if (logToUniqueFile) {
            std::fclose(getUniqueLogFile());
        }
    }

    void initLogging() {

        if (logToStdout) {
            getLogHooks().emplace_back([](LogRecord *, std::string *str) {
                printf("%s\n", str->c_str()); // or just use cout?
            });
        }

        getLogHooks().emplace_back([](LogRecord *, std::string *str) {
            // now remove all text formatting (this is expensive).
            auto start = str->find('\u001b');
            while (start != std::string::npos) {
                auto end = str->find('m', start);
                str->erase(start, (end - start) + 1);
                start = str->find('\u001b');
            }
        });

        if (logToLatestLog) {
            getLatestLogFile() = std::fopen("./latest.log", "w");

            getLogHooks().emplace_back([](LogRecord *, std::string *str) {
                std::fputs(str->c_str(), getLatestLogFile());
                std::fputc('\n', getLatestLogFile());
                std::fflush(getLatestLogFile()); // This is expensive.
            });
        }

        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::string ctimeStr = fmt::format("{:%a %b %d %T %Y}", *std::localtime(&now));
        std::string header = fmt::format("{0:=<32} [ NEW LOGGING SESSION AT {1} ] {0:=<32}",
                                         "", ctimeStr);

        if (logToUniqueFile) {
            mkdir(logsDir, 0777);

            ctimeStr = "/" + ctimeStr + ".log";
            ctimeStr = logsDir + ctimeStr;
            getUniqueLogFile() = fopen(ctimeStr.c_str(), "w");

            getLogHooks().emplace_back([](LogRecord *, std::string *str) {
                std::fputs(str->c_str(), getUniqueLogFile());
                std::fputc('\n', getUniqueLogFile());
                std::fflush(getUniqueLogFile()); // This is expensive.
            });
        }

        for (const auto &func : getLogHooks()) {
            // it is safe to pass in nullptr bc the only hooks registered so far should be our hooks,
            // and our hooks don't touch the LogRecord *
            func(nullptr, &header);
        }

        STMS_INFO("Initialized StoneMason {} (compiled on {} {})", versionString, __DATE__, __TIME__);
    }

#else
    void initLogging() { /* no-op */ };

    void quitLogging() { /* no-op */ };
#endif
}
