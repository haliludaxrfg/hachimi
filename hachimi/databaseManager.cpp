#include "databaseManager.h"

bool DatabaseManager::DTBconnect() {
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
void DatabaseManager::DTBdisconnect() {
	std::cout << "DatabaseManager::disconnect() called." << std::endl;
	if (connection_) {
		mysql_close(connection_);
		connection_ = nullptr;
	}
}
bool DatabaseManager::DTBexecuteQuery(const std::string& query) {
	if (!connection_) return false;
	if (mysql_query(connection_, query.c_str()) == 0) {
		return true;
	}
	else {
		std::cerr << "MySQL query error: " << mysql_error(connection_) << std::endl;
		return false;
	}
}

MYSQL_RES* DatabaseManager::DTBexecuteSelect(const std::string& query) {
	if (!connection_) return nullptr;
	// 运行查询
	if (mysql_query(connection_, query.c_str()) != 0) {
		std::cerr << "MySQL select query error: " << mysql_error(connection_) << " | Query: " << query << std::endl;
		return nullptr;
	}
	// 获取结果集
	MYSQL_RES* result = mysql_store_result(connection_);
	if (!result) {
		// 如果没有字段，说明该语句没有返回结果集（如 INSERT/UPDATE），否则说明是错误
		if (mysql_field_count(connection_) == 0) {
			return nullptr;
		} else {
			std::cerr << "MySQL store result failed: " << mysql_error(connection_) << " (errno: " << mysql_errno(connection_) << ") | Query: " << query << std::endl;
			return nullptr;
		}
	}
	return result;
}

//-----
//basic

DatabaseManager::DatabaseManager(const std::string& host, const std::string& user,
	const std::string& password, const std::string& database, unsigned int port)
	: host_(host), user_(user), password_(password), database_(database), port_(port), connection_(nullptr) {
	// 注：原代码强制使用127.0.0.1和3306，这里改为使用传入参数以避免连接到错误实例
}
DatabaseManager::~DatabaseManager() {
	DTBdisconnect();
}

bool DatabaseManager::DTBinitialize() {
	return DTBconnect();
}

bool DatabaseManager::DTBisConnected() const {
	return connection_ != nullptr;
}

bool DatabaseManager::DTBaddUser(const User& u) {
	if (!connection_) return false;
	std::string checkQuery = "SELECT COUNT(*) FROM user WHERE phone='" + u.getPhone() + "'";
	MYSQL_RES* result = DTBexecuteSelect(checkQuery);
	if (!result) return false;
	MYSQL_ROW row = mysql_fetch_row(result);
	bool exists = (row && std::stoi(row[0]) > 0);
	mysql_free_result(result);
	if (exists) {
		std::cerr << "User with phone " << u.getPhone() << " already exists." << std::endl;
		return false;
	}
	return DTBsaveUser(u);
}

bool DatabaseManager::DTBsaveUser(const User& u) {
	if (!connection_) return false;
	std::string query = "INSERT INTO user (phone, password, address) VALUES ('" +
		u.getPhone() + "', '" + u.getPassword() + "', '" + u.getAddress() + "')";
	return DTBexecuteQuery(query);
}

bool DatabaseManager::DTBupdateUser(const User& u) {
	if (!connection_) return false;
	// 统一使用表名 user（project 中其他地方也使用 user）
	std::string query = "UPDATE user SET password='" + u.getPassword() +
		"', address='" + u.getAddress() + "' WHERE phone='" + u.getPhone() + "'";
	if (mysql_query(connection_, query.c_str()) == 0) {
		// 可选：检查受影响行数，0 表示未找到匹配行
		my_ulonglong affected = mysql_affected_rows(connection_);
		if (affected == 0) {
			std::cerr << "DTBupdateUser: 未找到匹配用户 phone=" << u.getPhone() << std::endl;
			return false;
		}
		return true;
	} else {
		std::cerr << "MySQL update error: " << mysql_error(connection_) << " | Query: " << query << std::endl;
		return false;
	}
}

bool DatabaseManager::DTBloadUser(const std::string& phone, User& u) {
	if (!connection_) return false;
	std::string query = "SELECT phone, password, address FROM user WHERE phone='" + phone + "'";
	MYSQL_RES* result = DTBexecuteSelect(query);
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

bool DatabaseManager::DTBdeleteUser(const std::string& phone) {
	if (!connection_) return false;
	// 统一使用表名 user
	std::string query = "DELETE FROM user WHERE phone='" + phone + "'";
	if (mysql_query(connection_, query.c_str()) == 0) {
		my_ulonglong affected = mysql_affected_rows(connection_);
		if (affected == 0) {
			std::cerr << "DTBdeleteUser: 未找到匹配用户 phone=" << phone << std::endl;
			return false;
		}
		return true;
	} else {
		std::cerr << "MySQL delete error: " << mysql_error(connection_) << " | Query: " << query << std::endl;
		return false;
	}
}

std::vector<User> DatabaseManager::DTBloadAllUsers() {
	std::vector<User> users;
	if (!connection_) return users;
	std::string query = "SELECT phone, password, address FROM user";
	MYSQL_RES* result = DTBexecuteSelect(query);
	if (!result) return users;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		users.emplace_back(row[0] ? row[0] : "", row[1] ? row[1] : "", row[2] ? row[2] : "");
	}
	mysql_free_result(result);
	return users;
}

bool DatabaseManager::DTBsaveGood(const Good& g) {
	if (!connection_) return false;
	std::string query = "INSERT INTO good (name, price, stock, category) VALUES ('" +
		g.name + "', " + std::to_string(g.price) + ", " + std::to_string(g.stock) + ", '" + g.category + "')";
	return DTBexecuteQuery(query);
}

bool DatabaseManager::DTBupdateGood(const Good& g) {
	if (!connection_) return false;
	std::string query = "UPDATE good SET name='" + g.name +
		"', price=" + std::to_string(g.price) +
		", stock=" + std::to_string(g.stock) +
		", category='" + g.category +
		"' WHERE id=" + std::to_string(g.id);
	return DTBexecuteQuery(query);
}

bool DatabaseManager::DTBloadGood(int id, Good& g) {
	if (!connection_) return false;
	std::string query = "SELECT id, name, price, stock, category FROM good WHERE id=" + std::to_string(id);
	MYSQL_RES* result = DTBexecuteSelect(query);
	if (!result) return false;
	MYSQL_ROW row = mysql_fetch_row(result);
	if (row) {
		g = Good(std::stoi(row[0] ? row[0] : "0"),
			row[1] ? row[1] : "",
			row[2] ? std::stod(row[2]) : 0.0,
			row[3] ? std::stoi(row[3]) : 0,
			row[4] ? row[4] : "");
		mysql_free_result(result);
		return true;
	}
	mysql_free_result(result);
	return false;
}

bool DatabaseManager::DTBdeleteGood(int id) {
	if (!connection_) return false;
	std::string query = "DELETE FROM good WHERE id=" + std::to_string(id);
	return DTBexecuteQuery(query);
}

std::vector<Good> DatabaseManager::DTBloadAllGoods() {
	std::vector<Good> goods;
	if (!connection_) return goods;

	std::string query = "SELECT id, name, price, stock, category FROM good";
	// 调试性日志：显示 connection 指针与将要执行的查询
	std::cout << "DTBloadAllGoods: this=" << static_cast<void*>(this)
	          << " connection_=" << static_cast<void*>(connection_)
	          << " Query=\"" << query << "\"" << std::endl;

	MYSQL_RES* result = DTBexecuteSelect(query);
	if (!result) {
		// 如果 DTBexecuteSelect 返回 nullptr，额外输出当前数据库名与错误信息以便排查
		std::cout << "DTBloadAllGoods: DTBexecuteSelect 返回 nullptr。";
		if (connection_) {
			// 获取当前默认数据库名，便于确认是否连接到期望的 schema
			if (mysql_query(connection_, "SELECT DATABASE()") == 0) {
				MYSQL_RES* dbRes = mysql_store_result(connection_);
				if (dbRes) {
					MYSQL_ROW dbRow = mysql_fetch_row(dbRes);
					if (dbRow && dbRow[0]) {
						std::cout << " Current DATABASE(): " << dbRow[0];
					}
					mysql_free_result(dbRes);
				}
			}
			std::cerr << " MySQL error: " << mysql_error(connection_) << std::endl;
		} else {
			std::cout << " connection_ is nullptr." << std::endl;
		}
		return goods;
	}

	// 输出结果行数，帮助判定表是否为空
	my_ulonglong rows = mysql_num_rows(result);
	std::cout << "DTBloadAllGoods: mysql_num_rows = " << rows << std::endl;

	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		goods.emplace_back(std::stoi(row[0] ? row[0] : "0"),
			row[1] ? row[1] : "",
			row[2] ? std::stod(row[2]) : 0.0,
			row[3] ? std::stoi(row[3]) : 0,
			row[4] ? row[4] : "");
	}
	mysql_free_result(result);
	return goods;
}

std::vector<Good> DatabaseManager::DTBloadGoodsByCategory(const std::string& category) {
	std::vector<Good> goods;
	if (!connection_) return goods;
	std::string query = "SELECT id, name, price, stock, category FROM good WHERE category='" + category + "'";
	MYSQL_RES* result = DTBexecuteSelect(query);
	if (!result) return goods;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		goods.emplace_back(std::stoi(row[0] ? row[0] : "0"),
			row[1] ? row[1] : "",
			row[2] ? std::stod(row[2]) : 0.0,
			row[3] ? std::stoi(row[3]) : 0,
			row[4] ? row[4] : "");
	}
	mysql_free_result(result);
	return goods;
}

bool DatabaseManager::DTBupdateGoodStock(int good_id, int new_stock) {
	if (!connection_) return false;
	std::string query = "UPDATE good SET stock=" + std::to_string(new_stock) +
		" WHERE id=" + std::to_string(good_id);
	return DTBexecuteQuery(query);
}

bool DatabaseManager::DTBsaveOrder(const Order& o) {
	if (!connection_) return false;
	std::string query = "INSERT INTO `order` (order_id, user_phone, total_amount, discount_amount, final_amount, status, shipping_address, discount_policy) VALUES ('" +
		o.getOrderId() + "', '" + o.getUserPhone() + "', " + std::to_string(o.getTotalAmount()) +
		", " + std::to_string(o.getDiscountAmount()) + ", " + std::to_string(o.getFinalAmount()) +
		", " + std::to_string(o.getStatus()) + ", '" + o.getShippingAddress() +
		"', '" + o.getDiscountPolicy() + "')";
	if (!DTBexecuteQuery(query)) return false;
	for (const auto& item : o.getItems()) {
		if (!DTBsaveOrderItem(item)) {
			std::cerr << "Failed to save order item for order " << o.getOrderId() << std::endl;
			return false;
		}
	}
	return true;
}

bool DatabaseManager::DTBupdateOrder(const Order& o) {
	if (!connection_) return false;
	std::string query = "UPDATE `order` SET "
		"user_phone='" + o.getUserPhone() +
		"', shipping_address='" + o.getShippingAddress() +
		"', status=" + std::to_string(o.getStatus()) +
		", discount_policy='" + o.getDiscountPolicy() +
		"', total_amount=" + std::to_string(o.getTotalAmount()) +
		", discount_amount=" + std::to_string(o.getDiscountAmount()) +
		", final_amount=" + std::to_string(o.getFinalAmount()) +
		" WHERE order_id='" + o.getOrderId() + "'";
	if (!DTBexecuteQuery(query)) return false;
	if (!DTBdeleteOrderItems(o.getOrderId())) return false;
	for (const auto& item : o.getItems()) {
		if (!DTBsaveOrderItem(item)) return false;
	}
	return true;
}

bool DatabaseManager::DTBupdateOrderStatus(const std::string& order_id, int status) {
	if (!connection_) return false;
	std::string query = "UPDATE `order` SET status=" + std::to_string(status) +
		" WHERE order_id='" + order_id + "'";
	return DTBexecuteQuery(query);
}

bool DatabaseManager::DTBloadOrder(const std::string& order_id, Order& o) {
	if (!connection_) return false;
	std::string query = "SELECT order_id, user_phone, total_amount, discount_amount, final_amount, status, shipping_address, discount_policy FROM `order` WHERE order_id='" + order_id + "'";
	MYSQL_RES* result = DTBexecuteSelect(query);
	if (!result) return false;
	MYSQL_ROW row = mysql_fetch_row(result);
	if (row) {
		o = Order(TemporaryCart(), row[0] ? row[0] : "");
		o.setStatus(row[5] ? std::stoi(row[5]) : 0);
		o.setShippingAddress(row[6] ? row[6] : "");
		o.setDiscountPolicy(row[7] ? row[7] : "");
		o.setItems(DTBloadOrderItems(o.getOrderId()));
		mysql_free_result(result);
		return true;
	}
	mysql_free_result(result);
	return false;
}

bool DatabaseManager::DTBdeleteOrder(const std::string& order_id) {
	if (!connection_) return false;
	if (!DTBdeleteOrderItems(order_id)) {
		std::cerr << "Failed to delete order items for order " << order_id << std::endl;
		return false;
	}
	std::string query = "DELETE FROM `order` WHERE order_id='" + order_id + "'";
	return DTBexecuteQuery(query);
}

std::vector<Order> DatabaseManager::DTBloadOrdersByUser(const std::string& user_phone) {
	std::vector<Order> orders;
	if (!connection_) return orders;
	std::string query = "SELECT order_id, user_phone, total_amount, discount_amount, final_amount, status, shipping_address, discount_policy FROM `order` WHERE user_phone='" + user_phone + "'";
	MYSQL_RES* result = DTBexecuteSelect(query);
	if (!result) return orders;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		Order o(TemporaryCart(), row[0] ? row[0] : "");
		o.setStatus(row[5] ? std::stoi(row[5]) : 0);
		o.setShippingAddress(row[6] ? row[6] : "");
		o.setDiscountPolicy(row[7] ? row[7] : "");
		o.setItems(DTBloadOrderItems(o.getOrderId()));
		orders.push_back(o);
	}
	mysql_free_result(result);
	return orders;
}

std::vector<Order> DatabaseManager::DTBloadOrdersByStatus(int status) {
	std::vector<Order> orders;
	if (!connection_) return orders;
	std::string query = "SELECT order_id, user_phone, total_amount, discount_amount, final_amount, status, shipping_address, discount_policy FROM `order` WHERE status=" + std::to_string(status);
	MYSQL_RES* result = DTBexecuteSelect(query);
	if (!result) return orders;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		Order o(TemporaryCart(), row[0] ? row[0] : "");
		o.setStatus(row[5] ? std::stoi(row[5]) : 0);
		o.setShippingAddress(row[6] ? row[6] : "");
		o.setDiscountPolicy(row[7] ? row[7] : "");
		o.setItems(DTBloadOrderItems(o.getOrderId()));
		orders.push_back(o);
	}
	mysql_free_result(result);
	return orders;
}

std::vector<Order> DatabaseManager::DTBloadRecentOrders(int limit) {
	std::vector<Order> orders;
	if (!connection_) return orders;
	std::string query = "SELECT order_id, user_phone, total_amount, discount_amount, final_amount, status, shipping_address, discount_policy FROM `order` ORDER BY order_id DESC LIMIT " + std::to_string(limit);
	MYSQL_RES* result = DTBexecuteSelect(query);
	if (!result) return orders;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		Order o(TemporaryCart(), row[0] ? row[0] : "");
		o.setStatus(row[5] ? std::stoi(row[5]) : 0);
		o.setShippingAddress(row[6] ? row[6] : "");
		o.setDiscountPolicy(row[7] ? row[7] : "");
		o.setItems(DTBloadOrderItems(o.getOrderId()));
		orders.push_back(o);
	}
	mysql_free_result(result);
	return orders;
}

bool DatabaseManager::DTBsaveOrderItem(const OrderItem& item) {
	if (!connection_) return false;
	std::string query = "INSERT INTO orderitem (order_id, good_id, good_name, price, quantity, subtotal) VALUES ('" +
		item.getOrderId() + "', " +
		std::to_string(item.getGoodId()) + ", '" +
		item.getGoodName() + "', " +
		std::to_string(item.getPrice()) + ", " +
		std::to_string(item.getQuantity()) + ", " +
		std::to_string(item.getSubtotal()) + ")";
	return DTBexecuteQuery(query);
}

bool DatabaseManager::DTBupdateOrderItem(const OrderItem& item) {
	if (!connection_) return false;
	std::string query = "UPDATE orderitem SET good_name='" + item.getGoodName() +
		"', price=" + std::to_string(item.getPrice()) +
		", quantity=" + std::to_string(item.getQuantity()) +
		", subtotal=" + std::to_string(item.getSubtotal()) +
		" WHERE order_id='" + item.getOrderId() + "' AND good_id=" + std::to_string(item.getGoodId());
	return DTBexecuteQuery(query);
}

std::vector<OrderItem> DatabaseManager::DTBloadOrderItems(const std::string& order_id) {
	std::vector<OrderItem> items;
	if (!connection_) return items;
	std::string query = "SELECT order_id, good_id, good_name, price, quantity, subtotal FROM orderitem WHERE order_id='" + order_id + "'";
	MYSQL_RES* result = DTBexecuteSelect(query);
	if (!result) return items;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		OrderItem item;
		item.setOrderId(row[0] ? row[0] : "");
		item.setGoodId(row[1] ? std::stoi(row[1]) : 0);
		item.setGoodName(row[2] ? row[2] : "");
		item.setPrice(row[3] ? std::stod(row[3]) : 0.0);
		item.setQuantity(row[4] ? std::stoi(row[4]) : 0);
		item.setSubtotal(row[5] ? std::stod(row[5]) : 0.0);
		items.push_back(item);
	}
	mysql_free_result(result);
	return items;
}

bool DatabaseManager::DTBdeleteOrderItems(const std::string& order_id) {
	if (!connection_) return false;
	std::string query = "DELETE FROM orderitem WHERE order_id='" + order_id + "'";
	return DTBexecuteQuery(query);
}

bool DatabaseManager::DTBsaveTemporaryCart(const TemporaryCart& cart) {
	if (!connection_) return false;
	std::string query = "INSERT INTO temporarycart (cart_id, user_phone, shipping_address, discount_policy, total_amount, discount_amount, final_amount, is_converted) VALUES ('" +
		cart.cart_id + "', '" + cart.user_phone + "', '" + cart.shipping_address + "', '" + cart.discount_policy + "', " +
		std::to_string(cart.total_amount) + ", " + std::to_string(cart.discount_amount) + ", " + std::to_string(cart.final_amount) +
		", " + std::to_string(cart.is_converted ? 1 : 0) + ")";
	if (!DTBexecuteQuery(query)) return false;
	for (const auto& item : cart.items) {
		if (!DTBsaveCartItem(item, cart.cart_id)) {
			std::cerr << "Failed to save cart item for cart " << cart.cart_id << std::endl;
			return false;
		}
	}
	return true;
}

bool DatabaseManager::DTBupdateTemporaryCart(const TemporaryCart& cart) {
	if (!connection_) return false;
	std::string query = "UPDATE temporarycart SET user_phone='" + cart.user_phone +
		"', shipping_address='" + cart.shipping_address +
		"', discount_policy='" + cart.discount_policy +
		"', total_amount=" + std::to_string(cart.total_amount) +
		", discount_amount=" + std::to_string(cart.discount_amount) +
		", final_amount=" + std::to_string(cart.final_amount) +
		", is_converted=" + std::to_string(cart.is_converted ? 1 : 0) +
		" WHERE cart_id='" + cart.cart_id + "'";
	if (!DTBexecuteQuery(query)) return false;
	if (!DTBdeleteAllCartItems(cart.cart_id)) return false;
	for (const auto& item : cart.items) {
		if (!DTBsaveCartItem(item, cart.cart_id)) {
			std::cerr << "Failed to save cart item for cart " << cart.cart_id << std::endl;
			return false;
		}
	}
	return true;
}

bool DatabaseManager::DTBloadTemporaryCart(const std::string& cart_id, TemporaryCart& cart) {
	if (!connection_) return false;
	std::string query = "SELECT cart_id, user_phone, shipping_address, discount_policy, total_amount, discount_amount, final_amount, is_converted FROM temporarycart WHERE cart_id='" + cart_id + "'";
	MYSQL_RES* result = DTBexecuteSelect(query);
	if (!result) return false;
	MYSQL_ROW row = mysql_fetch_row(result);
	if (row) {
		cart.cart_id = row[0] ? row[0] : "";
		cart.user_phone = row[1] ? row[1] : "";
		cart.shipping_address = row[2] ? row[2] : "";
		cart.discount_policy = row[3] ? row[3] : "";
		cart.total_amount = row[4] ? std::stod(row[4]) : 0.0;
		cart.discount_amount = row[5] ? std::stod(row[5]) : 0.0;
		cart.final_amount = row[6] ? std::stod(row[6]) : 0.0;
		cart.is_converted = row[7] ? (std::stoi(row[7]) != 0) : false;
		mysql_free_result(result);
		cart.items = DTBloadCartItems(cart.cart_id);
		return true;
	}
	mysql_free_result(result);
	return false;
}

bool DatabaseManager::DTBdeleteTemporaryCart(const std::string& cart_id) {
	if (!connection_) return false;
	if (!DTBdeleteAllCartItems(cart_id)) return false;
	std::string query = "DELETE FROM temporarycart WHERE cart_id='" + cart_id + "'";
	return DTBexecuteQuery(query);
}

std::vector<TemporaryCart> DatabaseManager::DTBloadExpiredCarts() {
	std::vector<TemporaryCart> carts;
	if (!connection_) return carts;
	std::string query = "SELECT cart_id, user_phone, shipping_address, discount_policy, total_amount, discount_amount, final_amount, is_converted FROM temporarycart WHERE is_converted=0 AND last_updated < NOW() - INTERVAL 1 DAY";
	MYSQL_RES* result = DTBexecuteSelect(query);
	if (!result) return carts;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		TemporaryCart cart;
		cart.cart_id = row[0] ? row[0] : "";
		cart.user_phone = row[1] ? row[1] : "";
		cart.shipping_address = row[2] ? row[2] : "";
		cart.discount_policy = row[3] ? row[3] : "";
		cart.total_amount = row[4] ? std::stod(row[4]) : 0.0;
		cart.discount_amount = row[5] ? std::stod(row[5]) : 0.0;
		cart.final_amount = row[6] ? std::stod(row[6]) : 0.0;
		cart.is_converted = row[7] ? (std::stoi(row[7]) != 0) : false;
		cart.items = DTBloadCartItems(cart.cart_id);
		carts.push_back(cart);
	}
	mysql_free_result(result);
	return carts;
}

bool DatabaseManager::DTBcleanupExpiredCarts() {
	if (!connection_) return false;
	std::string query = "DELETE FROM temporarycart WHERE is_converted=0 AND last_updated < NOW() - INTERVAL 1 DAY";
	return DTBexecuteQuery(query);
}

bool DatabaseManager::DTBsaveCartItem(const CartItem& item, const std::string& cart_id) {
	if (!connection_) return false;
	std::string query = "INSERT INTO cartitem (cart_id, good_id, good_name, price, quantity, subtotal) VALUES ('" +
		cart_id + "', " +
		std::to_string(item.good_id) + ", '" +
		item.good_name + "', " +
		std::to_string(item.price) + ", " +
		std::to_string(item.quantity) + ", " +
		std::to_string(item.subtotal) + ")";
	return DTBexecuteQuery(query);
}

bool DatabaseManager::DTBupdateCartItem(const CartItem& item, const std::string& cart_id) {
	if (!connection_) return false;
	std::string query = "UPDATE cartitem SET good_name='" + item.good_name +
		"', price=" + std::to_string(item.price) +
		", quantity=" + std::to_string(item.quantity) +
		", subtotal=" + std::to_string(item.subtotal) +
		" WHERE cart_id='" + cart_id + "' AND good_id=" + std::to_string(item.good_id);
	return DTBexecuteQuery(query);
}

bool DatabaseManager::DTBdeleteCartItem(int good_id, const std::string& cart_id) {
	if (!connection_) return false;
	std::string query = "DELETE FROM cartitem WHERE cart_id='" + cart_id + "' AND good_id=" + std::to_string(good_id);
	return DTBexecuteQuery(query);
}

bool DatabaseManager::DTBdeleteAllCartItems(const std::string& cart_id) {
	if (!connection_) return false;
	std::string query = "DELETE FROM cartitem WHERE cart_id='" + cart_id + "'";
	return DTBexecuteQuery(query);
}

std::vector<CartItem> DatabaseManager::DTBloadCartItems(const std::string& cart_id) {
	std::vector<CartItem> items;
	if (!connection_) return items;
	std::string query = "SELECT good_id, good_name, price, quantity, subtotal FROM cartitem WHERE cart_id='" + cart_id + "'";
	MYSQL_RES* result = DTBexecuteSelect(query);
	if (!result) return items;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		CartItem item{ Good(0, "", 0.0, 0, ""), 0 };
		item.good_id = row[0] ? std::stoi(row[0]) : 0;
		item.good_name = row[1] ? row[1] : "";
		item.price = row[2] ? std::stod(row[2]) : 0.0;
		item.quantity = row[3] ? std::stoi(row[3]) : 0;
		item.subtotal = row[4] ? std::stod(row[4]) : 0.0;
		items.push_back(item);
	}
	mysql_free_result(result);
	return items;
}