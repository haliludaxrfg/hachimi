#include "userManager.h"

bool UserManager::registerUser(std::vector<User>& users, const std::string& phone, const std::string& password) {
	if (!User::isValidPhoneNumber(phone)) {
		std::cout << "Invalid phone number. It should be 11 digits." << std::endl;
		return false;
	}
	if (password.length() < 5 || password.length() > 25) {
		std::cout << "Password length should be between 5 and 25 characters." << std::endl;
		return false;
	}
	auto it = std::find_if(users.begin(), users.end(), [&](const User& u) { return u.getPhone() == phone; });
	if (it != users.end()) {
		std::cout << "User with this phone number already exists." << std::endl;
		return false;
	}
	users.emplace_back(phone, password, "");
	std::cout << "User registered successfully." << std::endl;
	return true;
}

int UserManager::loginUser(const std::vector<User>& users, const std::string& phone, const std::string& password) {
	for (size_t i = 0; i < users.size(); ++i) {
		if (users[i].getPhone() == phone && users[i].getPassword() == password) {
			std::cout << "Login successful." << std::endl;
			return static_cast<int>(i);
		}
	}
	std::cout << "Invalid phone number or password." << std::endl;
	return -1;
}

int UserManager::switchUser(const std::vector<User>& users, int currentUser) {
	if (users.empty()) {
		std::cout << "No users available. Please register first." << std::endl;
		return -1;
	}
	std::cout << "Available users:" << std::endl;
	for (size_t i = 0; i < users.size(); ++i) {
		std::cout << i << ": " << users[i].getPhone() << std::endl;
	}
	std::cout << "Enter user index to switch to: ";
	int index;
	std::cin >> index;
	if (index < 0 || index >= static_cast<int>(users.size())) {
		std::cout << "Invalid user index." << std::endl;
		return currentUser;
	}
	std::cout << "Switched to user: " << users[index].getPhone() << std::endl;
	return index;
}

void UserManager::displayUserMenu() {
	std::cout << "User Management Menu:" << std::endl;
	std::cout << "1. Register User" << std::endl;
	std::cout << "2. Login User" << std::endl;
	std::cout << "3. Switch User" << std::endl;
	std::cout << "4. Exit" << std::endl;
	std::cout << "Select an option: ";
}

bool UserManager::handleUserOption(char option, std::vector<User>& users, int& currentUser) {
	switch (option) {
	case '1': {
		std::string phone, password;
		std::cout << "Enter phone number: ";
		std::cin >> phone;
		std::cout << "Enter password: ";
		std::cin >> password;
		return registerUser(users, phone, password);
	}
	case '2': {
		std::string phone, password;
		std::cout << "Enter phone number: ";
		std::cin >> phone;
		std::cout << "Enter password: ";
		std::cin >> password;
		int userIndex = loginUser(users, phone, password);
		if (userIndex != -1) {
			currentUser = userIndex;
			return true;
		}
		return false;
	}
	case '3':
		currentUser = switchUser(users, currentUser);
		return true;
	case '4':
		std::cout << "Exiting user management." << std::endl;
		return false;
	default:
		std::cout << "Invalid option. Please try again." << std::endl;
		return true;
	}
}