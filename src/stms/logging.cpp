//
// Created by grant on 1/26/20.
//

#include "stms/logging.hpp"

#include <chrono>
#include <sys/stat.h>

namespace stms {

    LogRecord::LogRecord(LogLevel lvl, std::chrono::system_clock::time_point iTime, const char *iFile, unsigned int iLine)
                            : level(lvl), time(iTime), file(iFile), line(iLine) {}

#   ifdef STMS_ENABLE_LOGGING
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

        if (getLogPool() != nullptr) {
            getLogPool()->waitIdle(1000); // make sure all in-flight log records are processed!
            getLogPool()->stop(false);
        }
    }

    void initLogging() {

        if (logToStdout) {
            getLogHooks().emplace_back([](LogRecord *, std::string *str) {
                fputs(str->c_str(), stdout);
                fputc('\n', stdout);
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

            getLogHooks().emplace_back([](LogRecord *, std::string *str) {
                std::fputs(str->c_str(), getUniqueLogFile());
                std::fputc('\n', getUniqueLogFile());
                // Don't call fflush!
            });
        }

        for (const auto &func : getLogHooks()) {
            // it is safe to pass in nullptr bc the only hooks registered so far should be our hooks,
            // and our hooks don't touch the LogRecord *
            func(nullptr, &header);
        }

        STMS_INFO("Initialized StoneMason {} (compiled on {} {})", versionString, __DATE__, __TIME__);
    }

    static volatile bool logConsuming = false;
    static std::mutex logQMtx = std::mutex();

    static inline std::queue<std::unique_ptr<LogRecord>> &getLogQueue() {
        static std::queue<std::unique_ptr<LogRecord>> queue;
        return queue;
    }

    static inline const char *logLevelToString(const LogLevel &lvl) {
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

        bool empty = getLogQueue().empty();
        if (!empty) {
            std::unique_ptr<LogRecord> top = std::move(getLogQueue().front());
            getLogQueue().pop();

            lg.unlock();

            fmt::memory_buffer fileUrl;
            fmt::format_to(fileUrl, "file://{}:{}", top->file, top->line);

            time_t localtimeReady = std::chrono::system_clock::to_time_t(top->time);
            auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(top->time);
            auto ms = std::chrono::duration_cast<std::chrono::nanoseconds>(top->time - seconds);

            fmt::memory_buffer logMsg;
            fmt::format_to(logMsg, "[{0:%T}.{1:<12}] [{2:^72}] [{3:<8}]: {4}", *std::localtime(&localtimeReady),
                           ms.count(), fmt::to_string(fileUrl), logLevelToString(top->level), fmt::to_string(top->msg));

            std::string finalMsg = fmt::to_string(logMsg); // don't flush!

            for (const auto &func : getLogHooks()) {
                func(top.get(), &finalMsg);
            }

            // Recurse. No need to check/set the consume flag as they are only modified on exit/enter.
            // This is better than just looping bc it breaks the consume task up into multiple submits
            // to the thread pool!
            if (getLogPool() != nullptr) {
                getLogPool()->submitTask(consumeLogs);

                if (!getLogPool()->isRunning()) {
                    getLogPool()->start();
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

    void insertImpl(std::unique_ptr<LogRecord> rec) {
        std::unique_lock<std::mutex> lg(logQMtx);

        getLogQueue().emplace(std::move(rec));

        if (getLogPool() != nullptr && !logConsuming) {
            logConsuming = true;

            lg.unlock();

            getLogPool()->submitTask(consumeLogs);

            if (!getLogPool()->isRunning()) {
                getLogPool()->start();
                std::cerr << "Logging thread pool was stopped! Starting it now!" << std::endl;
            }
        } else if (!logConsuming) {
            logConsuming = true;

            lg.unlock();

            consumeLogs(); // no async? just do it here.
        } else {
            lg.unlock();
        }
    }

#   else // STMS_ENABLE_LOGGING
    void consumeLogs() {};
    void initLogging() {};
    void quitLogging() {};
#   endif //STMS_ENABLE_LOGGING
}
