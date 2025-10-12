#include "LogWindow.h"
#include "logger.h"
#include <QTextCursor>
#include <QDateTime>
#include <iostream>
#include <QMetaObject>

LogWindow::LogWindow(QWidget* parent)
    : QWidget(parent)
    , logEdit(new QPlainTextEdit(this))
{
    setWindowTitle("测试信息窗口");
    logEdit->setReadOnly(true);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(logEdit);
    setLayout(layout);
    resize(600, 400);
}

QPlainTextEdit* LogWindow::getLogEdit() const {
    return logEdit;
}

void LogWindow::appendLog(const QString& msg) {
    // 将消息追加到末尾，保持自动滚动到最后
    logEdit->moveCursor(QTextCursor::End);
    logEdit->insertPlainText(msg);
    logEdit->moveCursor(QTextCursor::End);
}


