#include "user.h"
using namespace std;

// 正确实现构造函数
user::user(string phone, string password, string address)
    : phone(phone), password(password), address(address) {}

// 正确实现成员函数
string user::getPhone() { return phone; }
string user::getPassword() { return password; }
string user::getAddress() { return address; }
void user::setPassword(string newpassword) { password = newpassword; }
void user::setAddress(string newaddress) { address = newaddress; }