#pragma once
#include <string>
using namespace std;

class Good {
    int id; //prikey
    string name;
    double price; //保留两位小数
    int stock;
    string category;
public:
    Good();
    Good(int id, string name, double price, int stock, string category);
    Good(const Good& other);
    ~Good();
    Good& operator=(const Good& other);

    int getId();
    string getName();
    double getPrice();
    int getStock();
    string getCategory();

    void setName(string newname);
    void setPrice(double newprice);
    void setStock(int newstock);
    void setCategory(string newcategory);
};