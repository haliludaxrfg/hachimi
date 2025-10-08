#pragma once
#include <string>
#include <fstream>
#include <mutex>

class Logger {
public:
    enum class Level { INFO, WARN, FAIL, DEBUG }; // 替换ERROR为FAIL

    static Logger& instance();

    void log(Level level, const std::string& message);

    // 快捷方法
    void info(const std::string& message) { log(Level::INFO, message); }
    void warn(const std::string& message) { log(Level::WARN, message); }
    void fail(const std::string& message) { log(Level::FAIL, message); } // 替换error为fail
    void debug(const std::string& message) { log(Level::DEBUG, message); }

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream logFile;
    std::mutex mtx;

    std::string getCurrentTime();
    std::string levelToString(Level level);
};