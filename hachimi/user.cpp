#include "user.h"


// 正确实现构造函数
User::User(std::string phone, std::string password, std::string address)
    : phone(phone), password(password), address(address) {}

// 正确实现成员函数
std::string User::getPhone() const{ return phone; }
std::string User::getPassword() const{ return password; }
std::string User::getAddress() const{ return address; }
void User::setPassword(std::string newpassword) { password = newpassword; }
void User::setAddress(std::string newaddress) { address = newaddress; }
bool User::isValidPhoneNumber(const std::string& phone) {
    if (phone.length() != 11) return false;
    return std::all_of(phone.begin(), phone.end(), ::isdigit);
}