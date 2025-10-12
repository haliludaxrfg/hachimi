#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QTextCursor>

class LogWindow : public QWidget {
    Q_OBJECT
public:
    explicit LogWindow(QWidget* parent = nullptr);
    QPlainTextEdit* getLogEdit() const;

public slots:
    void appendLog(const QString& msg);

private:
    QPlainTextEdit* logEdit;

};