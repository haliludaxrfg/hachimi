#include "logger.h"
#include <iostream>
#include <ctime>
#include <iomanip>

Logger::Logger() {
    logFile.open("server.log", std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file!" << std::endl;
    }
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

std::string Logger::getCurrentTime() {
    std::time_t now = std::time(nullptr);
    std::tm tm;

#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif

    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buffer);
}

std::string Logger::levelToString(Level level) {
    switch (level) {
    case Level::INFO: return "INFO";
    case Level::WARN: return "WARN";
    case Level::FAIL: return "FAIL"; // 替换ERROR为FAIL
    case Level::DEBUG: return "DEBUG";
    default: return "UNKNOWN";
    }
}

void Logger::log(Level level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mtx);

    if (!logFile.is_open()) {
        std::cerr << "Log file is not open!" << std::endl;
        return;
    }

    std::string timeStr = getCurrentTime();
    std::string levelStr = levelToString(level);

    logFile << "[" << timeStr << "][" << levelStr << "] " << message << std::endl;
    logFile.flush();

    std::cout << "[" << timeStr << "][" << levelStr << "] " << message << std::endl;
}