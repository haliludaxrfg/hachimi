#pragma once
#include <string>
using namespace std;
class User {

	string phone;//(11位)==id
	string password; //5-25
	string address;
public:
	User(string phone, string password, string address);
	string getPhone();
	string getPassword();
	string getAddress();
	void setPassword(string newpassword);
	void setAddress(string newaddress);
};
#include "user.h"
using namespace std;

// 正确实现构造函数
User::User(string phone, string password, string address)
    : phone(phone), password(password), address(address) {}

// 正确实现成员函数
string User::getPhone() { return phone; }
string User::getPassword() { return password; }
string User::getAddress() { return address; }
void User::setPassword(string newpassword) { password = newpassword; }
void User::setAddress(string newaddress) { address = newaddress; }