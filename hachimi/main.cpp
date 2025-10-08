#define NOMINMAX
#define byte win_byte_override
#include <windows.h>
#undef byte
#include <QtWidgets/QApplication>
#include <mysql.h>
#include <iostream>
//#include "hachimi.h"
//ccbccbccbc4nmb
#include "cartItem.h"
#include "Client.h"
#include "databaseManager.h"
#include "good.h"
#include "logger.h"
#include "order.h"
#include "orderItem.h"
#include "promotion.h"
#include "Server.h"
#include "TemporaryCart.h"
#include "user.h"
#include "userManager.h"


int main()
{
	Server server(8888);
	server.start();
	// 这里可以添加代码来保持服务器运行，处理信号等
	std::cout << "按回车键停止服务器..." << std::endl;
	std::cin.get();
	server.stop();
	std::cout << "服务器已停止。" << std::endl;
	system("pause");
	return 0;
}
