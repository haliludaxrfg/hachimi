#include "good.h"


// 注意：类名应为 Good（首字母大写），与头文件保持一致
Good::Good(int id, std::string name, double price, int stock, std::string category)
    : id(id), name(name), price(price), stock(stock), category(category) {
}

int Good::getId() const { return id; }
std::string Good::getName() const { return name; }
double Good::getPrice() const { return price; }
int Good::getStock() const { return stock; }
std::string Good::getCategory() const { return category; }

void Good::setName(std::string newname) { name = newname; }
void Good::setPrice(double newprice) { price = newprice; }
void Good::setStock(int newstock) { stock = newstock; }
void Good::setCategory(std::string newcategory) { category = newcategory; }

Good::Good() : id(0), name(""), price(0.0), stock(0), category("") {}
Good::~Good() {}

Good::Good(const Good& other)
    : id(other.id), name(other.name), price(other.price), stock(other.stock), category(other.category) {
}

Good& Good::operator=(const Good& other) {
    if (this != &other) {
        id = other.id;
        name = other.name;
        price = other.price;
        stock = other.stock;
        category = other.category;
    }
    return *this;
}