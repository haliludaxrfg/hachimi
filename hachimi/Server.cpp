#include "Server.h"
#
Server::Server(int port)
    : port(port)
{
    // 初始化数据库管理对象
    dbManager = new DatabaseManager("localhost", "root", "a5B3#eF7hJ", "remake", 3306);
    if (!dbManager->initialize()) {
        std::cerr << "数据库连接失败！" << std::endl;
        // 你可以抛异常或设置标志
    }
    else {
        std::cout << "数据库连接成功！" << std::endl;
    }
}

Server::~Server() {
    if (dbManager) {
        delete dbManager;
        dbManager = nullptr;
    }
}

void Server::start() {
    Logger::instance().info("服务器正在启动，监听端口: " + std::to_string(port));
    // 或者
    // std::cout << "服务器正在启动，监听端口: " << port << std::endl;

    if (dbManager && dbManager->isConnected()) {
        Logger::instance().info("数据库已连接。");
    }
    else {
        Logger::instance().fail("数据库未连接！");
    }

    // 这里写你的socket监听/主循环等
}

void Server::stop() {
    std::cout << "服务器停止" << std::endl;
    // 这里可以写你的socket关闭/线程回收等逻辑
}