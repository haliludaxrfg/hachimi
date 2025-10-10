#include "logger.h"
#include <QTextCursor>
#include <QDateTime>
#include <iostream>

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() {
    logFile.open("log.txt", std::ios::app);
}

Logger::~Logger() {
    restoreCout();
    if (logFile.is_open()) logFile.close();
}

void Logger::setLogEdit(QPlainTextEdit* edit) {
    logEdit = edit;
    if (!qtLogStream) {
        qtLogStream = new QtLogStream(this);
    }
}

void Logger::log(Level level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string time = getCurrentTime();
    std::string levelStr = levelToString(level);
    std::string logMsg = "[" + time + "][" + levelStr + "] " + message + "\n";
    if (logFile.is_open()) logFile << logMsg;
    if (logEdit) {
        emit logAppended(QString::fromStdString(logMsg));
    }
}

std::string Logger::getCurrentTime() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();
}

std::string Logger::levelToString(Level level) {
    switch (level) {
    case Level::INFO: return "INFO";
    case Level::WARN: return "WARN";
    case Level::FAIL: return "FAIL";
    case Level::DEBUG: return "DEBUG";
    }
    return "UNKNOWN";
}

// QtLogStream实现
Logger::QtLogStream::QtLogStream(Logger* logger) : logger(logger) {}

int Logger::QtLogStream::overflow(int c) {
    if (c != EOF) {
        buffer += static_cast<char>(c);
        if (c == '\n') {
            logger->info(buffer); // 这里用info级别
            buffer.clear();
        }
    }
    return c;
}
int Logger::QtLogStream::sync() {
    if (!buffer.empty()) {
        logger->info(buffer);
        buffer.clear();
    }
    return 0;
}

void Logger::redirectCout() {
    if (!qtLogStream) qtLogStream = new QtLogStream(this);
    oldCout = std::cout.rdbuf(qtLogStream);
}
void Logger::restoreCout() {
    if (oldCout) std::cout.rdbuf(oldCout);
}