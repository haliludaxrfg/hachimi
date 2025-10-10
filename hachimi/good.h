#pragma once
#include <string>
#include <iostream>


class Good {
	friend class DatabaseManager;
    int id; //prikey
    std::string name;
    double price; //保留两位小数
    int stock;
    std::string category;
public:
    Good();
    Good(int id, std::string name, double price, int stock, std::string category);
    Good(const Good& other);
    ~Good();
    Good& operator=(const Good& other);

    int getId() const;
    std::string getName() const;
    double getPrice() const;
    int getStock() const;
    std::string getCategory() const;

};