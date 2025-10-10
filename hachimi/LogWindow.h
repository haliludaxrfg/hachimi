#pragma once
#include <QWidget>
#include <QPlainTextEdit>

class LogWindow : public QWidget {
    Q_OBJECT
public:
    explicit LogWindow(QWidget* parent = nullptr);
    QPlainTextEdit* getLogEdit();
    void appendLog(const QString& msg);
private:
    QPlainTextEdit* logEdit;
};
