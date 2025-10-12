#pragma once
#include <QPlainTextEdit>
#include <mutex>
#include <fstream>
#include <streambuf>
#include <string>
#include <QObject>

class Logger : public QObject {
    Q_OBJECT
public:
    enum class Level { INFO, WARN, FAIL, DEBUG };

    static Logger& instance();

    void log(Level level, const std::string& message);

    void setLogEdit(QPlainTextEdit* edit); // 新增

    void info(const std::string& message) { log(Level::INFO, message); }
    void warn(const std::string& message) { log(Level::WARN, message); }
    void fail(const std::string& message) { log(Level::FAIL, message); }
    void debug(const std::string& message) { log(Level::DEBUG, message); }

signals:
    void logAppended(const QString& msg);

private:
    bool fileWriterConnected = 0;
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream logFile;
    std::mutex mtx;
    QPlainTextEdit* logEdit = nullptr; // 新增

    std::string getCurrentTime();
    std::string levelToString(Level level);

    class QtLogStream : public std::streambuf {
    public:
        QtLogStream(Logger* logger);
    protected:
        int overflow(int c) override;
        int sync() override;
    private:
        Logger* logger;
        std::string buffer;
    };

    QtLogStream* qtLogStream = nullptr;
    std::streambuf* oldCout = nullptr;
public:
    void redirectCout(); // 新增
    void restoreCout();  // 新增
};