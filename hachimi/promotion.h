#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <algorithm>
#ifdef max
#undef max
#endif
#include <nlohmann/json.hpp>
using namespace std;

// 促销策略基类
class PromotionStrategy {
public:
    virtual ~PromotionStrategy() = default;
    virtual double apply(double price, int quantity = 1) const = 0;
    virtual std::string name() const = 0;
    // 工厂方法：通过JSON创建策略对象
    static std::shared_ptr<PromotionStrategy> fromJson(const nlohmann::json& js);
};

// 统一折扣
class DiscountStrategy : public PromotionStrategy {
    double discount; // 0.9 表示9折
public:
    DiscountStrategy(double d) : discount(d) {}
    double apply(double price, int quantity = 1) const override {
        return price * discount * quantity;
    }
    std::string name() const override { return "discount"; }
};

// 阶梯折扣
class StepDiscountStrategy : public PromotionStrategy {
public:
    double apply(double price, int quantity = 1) const override {
        double total = 0;
        for (int i = 1; i <= quantity; ++i) {
            if (i == 2) total += price * 0.9;
            else if (i == 3) total += price * 0.8;
            else if (i == 4) continue; // 免单
            else total += price;
        }
        return total;
    }
    std::string name() const override { return "step_discount"; }
};

// 降价
class PriceReductionStrategy : public PromotionStrategy {
    double reduction;
public:
    PriceReductionStrategy(double r) : reduction(r) {}
    double apply(double price, int quantity = 1) const override {
        return std::max(0.0, (price - reduction) * quantity);
    }
    std::string name() const override { return "reduction"; }
};

// 券（装饰器）
class CouponStrategy : public PromotionStrategy {
    std::shared_ptr<PromotionStrategy> base;
    double couponValue;
public:
    CouponStrategy(std::shared_ptr<PromotionStrategy> b, double v)
        : base(b), couponValue(v) {}
    double apply(double price, int quantity = 1) const override {
        double afterBase = base->apply(price, quantity);
        return std::max(0.0, afterBase - couponValue);
    }
    std::string name() const override { return "coupon"; }
};
