#pragma once
#include <string>
#include <memory>
#include <algorithm>
#ifdef max
#undef max
#endif
#include <nlohmann/json.hpp>

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
    DiscountStrategy(double d);
    double apply(double price, int quantity = 1) const override;
    std::string name() const override;
};

// 阶梯折扣
class StepDiscountStrategy : public PromotionStrategy {
public:
    double apply(double price, int quantity = 1) const override;
    std::string name() const override;
};

// 降价
class PriceReductionStrategy : public PromotionStrategy {
    double reduction;
public:
    PriceReductionStrategy(double r);
    double apply(double price, int quantity = 1) const override;
    std::string name() const override;
};

// 券（装饰器）
class CouponStrategy : public PromotionStrategy {
    std::shared_ptr<PromotionStrategy> base;
    double couponValue;
public:
    CouponStrategy(std::shared_ptr<PromotionStrategy> b, double v);
    double apply(double price, int quantity = 1) const override;
    std::string name() const override;
};