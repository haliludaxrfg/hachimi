#include "hachimi.h"
#include <QtWidgets/QApplication>
#include <mysql.h>
#include <iostream>

int main(int argc, char *argv[])
{
    //QApplication app(argc, argv);

    // 初始化MySQL
    MYSQL *conn = mysql_init(nullptr);
    if (conn == nullptr) {
        std::cerr << "MySQL初始化失败" << std::endl;
        return 1;
    }

    // 连接MySQL服务器
    if (mysql_real_connect(conn, "localhost", "root", "a5B3#eF7hJ", "OOP_shopping", 3306, nullptr, 0) == nullptr) {
        std::cerr << "MySQL连接失败: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return 1;
    } else {
        std::cout << "MySQL连接成功" << std::endl;
    }

    //hachimi window;
    //window.show();

    //int result = app.exec();

    // 关闭MySQL连接
    mysql_close(conn);
	system("pause");
    //return result;
}
