#include "databaseManager.h"

bool DatabaseManager::connect() {
	std::cout << "DatabaseManager::connect() called." << std::endl;
	// 初始化 MYSQL 结构体
	connection_ = mysql_init(nullptr);
	if (!connection_) {
		std::cerr << "mysql_init failed!" << std::endl;
		return false;
	}
	// 连接数据库
	if (!mysql_real_connect(connection_, host_.c_str(), user_.c_str(), password_.c_str(),
		database_.c_str(), port_, nullptr, 0)) {
		std::cerr << "mysql_real_connect failed: " << mysql_error(connection_) << std::endl;
		mysql_close(connection_);
		connection_ = nullptr;
		return false;
	}
	return true;
}
void DatabaseManager::disconnect() {
	std::cout << "DatabaseManager::disconnect() called." << std::endl;
	if (connection_) {
		mysql_close(connection_);
		connection_ = nullptr;
	}
}
bool DatabaseManager::executeQuery(const std::string& query) {
	if (!connection_) return false;
	if (mysql_query(connection_, query.c_str()) == 0) {
		return true;
	}
	else {
		std::cerr << "MySQL query error: " << mysql_error(connection_) << std::endl;
		return false;
	}
}

MYSQL_RES* DatabaseManager::executeSelect(const std::string& query) {
	if (!connection_) return nullptr;
	if (mysql_query(connection_, query.c_str()) != 0) {
		std::cerr << "MySQL select query error: " << mysql_error(connection_) << std::endl;
		return nullptr;
	}
	return mysql_store_result(connection_);
}

//-----

DatabaseManager::DatabaseManager(const std::string& host, const std::string& user,
	const std::string& password, const std::string& database, unsigned int port)
	: host_(host), user_(user), password_(password), database_(database), port_(port), connection_(nullptr) {
}
DatabaseManager::~DatabaseManager() {
	disconnect();
}

bool DatabaseManager::initialize() {
	disconnect();
	return connect();
}

bool DatabaseManager::isConnected() const {
	return connection_ != nullptr;
}

bool DatabaseManager::addUser(const User& u) {
	if (!connection_) return false;
	// 检查用户是否已存在
	std::string checkQuery = "SELECT COUNT(*) FROM user WHERE phone='" + u.getPhone() + "'";
	MYSQL_RES* result = executeSelect(checkQuery);
	if (!result) return false;
	MYSQL_ROW row = mysql_fetch_row(result);
	bool exists = (row && std::stoi(row[0]) > 0);
	mysql_free_result(result);
	if (exists) {
		std::cerr << "User with phone " << u.getPhone() << " already exists." << std::endl;
		return false;
	}
	// 插入新用户
	return saveUser(u);
}

bool DatabaseManager::saveUser(const User& u) {
	if (!connection_) return false;
	std::string query = "INSERT INTO user (phone, password, address) VALUES ('" +
		u.getPhone() + "', '" + u.getPassword() + "', '" + u.getAddress() + "')";
	return executeQuery(query);
}

bool DatabaseManager::updateUser(const User& u) {
	if (!connection_) return false;
	std::string query = "UPDATE users SET password='" + u.getPassword() +
		"', address='" + u.getAddress() + "' WHERE phone='" + u.getPhone() + "'";
	return executeQuery(query);
}

bool DatabaseManager::loadUser(const std::string& phone, User& u) {
	if (!connection_) return false;
	std::string query = "SELECT phone, password, address FROM user WHERE phone='" + phone + "'";
	MYSQL_RES* result = executeSelect(query);
	if (!result) return false;
	MYSQL_ROW row = mysql_fetch_row(result);
	if (row) {
		u = User(row[0] ? row[0] : "", row[1] ? row[1] : "", row[2] ? row[2] : "");
		mysql_free_result(result);
		return true;
	}
	mysql_free_result(result);
	return false;
}

bool DatabaseManager::deleteUser(const std::string& phone) {
	if (!connection_) return false;
	std::string query = "DELETE FROM users WHERE phone='" + phone + "'";
	return executeQuery(query);
}

std::vector<User> DatabaseManager::loadAllUsers() {
	std::vector<User> users;
	if (!connection_) return users;
	std::string query = "SELECT phone, password, address FROM users";
	MYSQL_RES* result = executeSelect(query);
	if (!result) return users;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		users.emplace_back(row[0] ? row[0] : "", row[1] ? row[1] : "", row[2] ? row[2] : "");
	}
	mysql_free_result(result);
	return users;
}