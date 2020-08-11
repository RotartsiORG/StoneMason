//
// Created by grant on 1/26/20.
//

#include "stms/logging.hpp"
#ifdef STMS_ENABLE_LOGGING

#include <chrono>
#include <sys/stat.h>

namespace stms {

    LogRecord::LogRecord(LogLevel lvl, std::chrono::system_clock::time_point iTime, const char *iFile, unsigned int iLine)
                            : level(lvl), time(iTime), file(iFile), line(iLine) {}

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

        if (logPool != nullptr) {
            logPool->waitIdle(); // make sure all in-flight log records are processed!
            logPool->stop(true);
        }
    }

    void initLogging() {

        if (logToStdout) {
            logHooks.emplace_back([](LogRecord *, std::string *str) {
                printf("%s\n", str->c_str()); // or just use cout?
            });
        }

        logHooks.emplace_back([](LogRecord *, std::string *str) {
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

            logHooks.emplace_back([](LogRecord *, std::string *str) {
                std::fputs(str->c_str(), getLatestLogFile());
                std::fputc('\n', getLatestLogFile());
                // Don't call fflush!
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

            logHooks.emplace_back([](LogRecord *, std::string *str) {
                std::fputs(str->c_str(), getUniqueLogFile());
                std::fputc('\n', getUniqueLogFile());
                // Don't call fflush!
            });
        }

        for (const auto &func : logHooks) {
            // it is safe to pass in nullptr bc the only hooks registered so far should be our hooks,
            // and our hooks don't touch the LogRecord *
            func(nullptr, &header);
        }

        STMS_INFO("Initialized StoneMason {} (compiled on {} {})", versionString, __DATE__, __TIME__);
    }

    volatile bool logConsuming = false;
    std::mutex logQMtx = std::mutex();
    std::queue<LogRecord> logQueue = std::queue<LogRecord>(); // screw the non-catchable exceptions i hate my life
    ThreadPool *logPool = nullptr;
    std::vector<std::function<void(LogRecord *, std::string *)>> logHooks;

    const char *logLevelToString(const LogLevel &lvl) {
        switch (lvl) {
            case (eTrace):
                return "  \u001b[37mtrace\u001b[0m  "; // white
            case (eDebug):
                return "  \u001b[36mdebug\u001b[0m  "; // cyan
            case (eInfo):
                return "  \u001b[34minfo\u001b[0m   "; // blue
            case (eWarn):
                return " \u001b[1m\u001b[33mWARNING\u001b[0m "; // bold yellow
            case (eError):
                return "  \u001b[1m\u001b[31mERROR\u001b[0m  "; // bold red
            case (eFatal):
                return " \u001b[1m\u001b[4m\u001b[31m*FATAL*\u001b[0m "; // bold underlined red
            default:
                return "!!! INVALID LOG LEVEL !!!";
        }
    }

    void consumeLogs() {

        std::unique_lock<std::mutex> lg(logQMtx);

        bool empty = logQueue.empty();
        if (!empty) {
            LogRecord top = std::move(logQueue.front());
            logQueue.pop();

            lg.unlock();

            fmt::memory_buffer fileUrl;
            fmt::format_to(fileUrl, "file://{}:{}", top.file, top.line);

            time_t localtimeReady = std::chrono::system_clock::to_time_t(top.time);
            auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(top.time);
            auto ms = std::chrono::duration_cast<std::chrono::nanoseconds>(top.time - seconds);

            fmt::memory_buffer logMsg;
            fmt::format_to(logMsg, "[{0:%T}.{1:<12}] [{2:^72}] [{3:<8}]: {4}", *std::localtime(&localtimeReady),
                           ms.count(), fmt::to_string(fileUrl), logLevelToString(top.level), fmt::to_string(top.msg));

            std::string finalMsg = fmt::to_string(logMsg); // don't flush!

            for (const auto &func : logHooks) {
                func(&top, &finalMsg);
            }

            // Recurse. No need to check/set the consume flag as they are only modified on exit/enter.
            // This is better than just looping bc it breaks the consume task up into multiple submits
            // to the thread pool!
            if (logPool != nullptr) {
                logPool->submitTask(consumeLogs);

                if (!logPool->isRunning()) {
                    logPool->start();
                    std::cerr << "Logging thread pool was stopped! Starting it now!" << std::endl;
                }
            } else {
                consumeLogs();
            }
        } else {
            logConsuming = false;

            lg.unlock();
        }
    }
}

#endif // STMS_ENABLE_LOGGING
