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
    // 仅保存指针并确保 qtLogStream 存在（不做文件写入连接）
    logEdit = edit;
    if (!qtLogStream) {
        qtLogStream = new QtLogStream(this);
    }
}

void Logger::log(Level level, const std::string& message) {
    // 先构造消息
    std::string time = getCurrentTime();
    std::string levelStr = levelToString(level);
    std::string logMsg = "[" + time + "][" + levelStr + "] " + message + "\n";

    // 写文件在锁内，emit 在锁外
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (logFile.is_open()) {
            logFile << logMsg;
            logFile.flush();
        }
    }

    // 将 std::string 转为 QString 后发出信号（不在锁内）
    emit logAppended(QString::fromStdString(logMsg));
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

// QtLogStream 实现：将 std::cout 的整行转为 Logger::info
Logger::QtLogStream::QtLogStream(Logger* logger) : logger(logger) {}

int Logger::QtLogStream::overflow(int c) {
    if (c != EOF) {
        buffer += static_cast<char>(c);
        if (c == '\n') {
            // 使用 info 将整行写入日志（log 会写文件并 emit）
            logger->info(buffer);
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