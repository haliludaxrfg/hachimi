#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QVBoxLayout>

class AdminWindow : public QWidget {
    Q_OBJECT
public:
    AdminWindow(QWidget* parent = nullptr);

signals:
    void backRequested(); // 返回信号

private:
    QTabWidget* tabWidget;
    QWidget* userTab;
    QWidget* goodTab;
    QWidget* orderTab;
    QPushButton* backBtn;
};