#pragma once
#include "user.h"
#include <string>
#include <vector>

class UserManager {
public:
    // 注册新用户
    static bool registerUser(std::vector<User>& users, const std::string& phone, const std::string& password);

    // 用户登录
    static int loginUser(const std::vector<User>& users, const std::string& phone, const std::string& password);

    // 切换用户
    static int switchUser(const std::vector<User>& users, int currentUser);

    // 显示用户管理菜单
    static void displayUserMenu();

    // 处理用户管理选项
    static bool handleUserOption(char option, std::vector<User>& users, int& currentUser);
};