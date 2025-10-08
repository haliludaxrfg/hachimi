#include "good.h"
using namespace std;

// 注意：类名应为 Good（首字母大写），与头文件保持一致
Good::Good(int id, string name, double price, int stock, string category)
    : id(id), name(name), price(price), stock(stock), category(category) {
}

int Good::getId() { return id; }
string Good::getName() { return name; }
double Good::getPrice() { return price; }
int Good::getStock() { return stock; }
string Good::getCategory() { return category; }

void Good::setName(string newname) { name = newname; }
void Good::setPrice(double newprice) { price = newprice; }
void Good::setStock(int newstock) { stock = newstock; }
void Good::setCategory(string newcategory) { category = newcategory; }

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