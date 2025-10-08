#include "user.h"


// 正确实现构造函数
User::User(std::string phone, std::string password, std::string address)
    : phone(phone), password(password), address(address) {}

// 正确实现成员函数
std::string User::getPhone() { return phone; }
std::string User::getPassword() { return password; }
std::string User::getAddress() { return address; }
void User::setPassword(std::string newpassword) { password = newpassword; }
void User::setAddress(std::string newaddress) { address = newaddress; }