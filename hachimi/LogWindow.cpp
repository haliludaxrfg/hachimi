#include "LogWindow.h"
#include <QVBoxLayout>

LogWindow::LogWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("测试信息窗口");
    logEdit = new QPlainTextEdit(this);
    logEdit->setReadOnly(true);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(logEdit);
    setLayout(layout);
    resize(600, 400);
}

QPlainTextEdit* LogWindow::getLogEdit() {
    return logEdit;
}

void LogWindow::appendLog(const QString& msg) {
    logEdit->moveCursor(QTextCursor::End);
    logEdit->insertPlainText(msg);
}