#include "PromotionStrategy.h"
#include <stdexcept>

// 工厂方法：通过JSON创建策略对象
std::shared_ptr<PromotionStrategy> PromotionStrategy::fromJson(const nlohmann::json& js) {
    std::string type = js.value("type", "");

    if (type == "discount") {
        double discount = js.value("discount", 1.0);
        return std::make_shared<DiscountStrategy>(discount);
    }
    else if (type == "step_discount") {
        return std::make_shared<StepDiscountStrategy>();
    }
    else if (type == "reduction") {
        double reduction = js.value("reduction", 0.0);
        return std::make_shared<PriceReductionStrategy>(reduction);
    }
    else if (type == "coupon") {
        double couponValue = js.value("coupon_value", 0.0);
        auto baseStrategy = fromJson(js["base"]);
        return std::make_shared<CouponStrategy>(baseStrategy, couponValue);
    }
    else {
        throw std::invalid_argument("Unknown promotion strategy type: " + type);
    }
}

// DiscountStrategy 成员函数实现
DiscountStrategy::DiscountStrategy(double d) : discount(d) {}

double DiscountStrategy::apply(double price, int quantity) const {
    return price * discount * quantity;
}

std::string DiscountStrategy::name() const {
    return "discount";
}

// StepDiscountStrategy 成员函数实现
double StepDiscountStrategy::apply(double price, int quantity) const {
    double total = 0;
    for (int i = 1; i <= quantity; ++i) {
        if (i == 2) total += price * 0.9;
        else if (i == 3) total += price * 0.8;
        else total += price;
    }
    return total;
}

std::string StepDiscountStrategy::name() const {
    return "step_discount";
}

// PriceReductionStrategy 成员函数实现
PriceReductionStrategy::PriceReductionStrategy(double r) : reduction(r) {}

double PriceReductionStrategy::apply(double price, int quantity) const {
    return std::max(0.0, (price - reduction) * quantity);
}

std::string PriceReductionStrategy::name() const {
    return "reduction";
}

// CouponStrategy 成员函数实现
CouponStrategy::CouponStrategy(std::shared_ptr<PromotionStrategy> b, double v)
    : base(b), couponValue(v) {
}

double CouponStrategy::apply(double price, int quantity) const {
    double afterBase = base->apply(price, quantity);
    return std::max(0.0, afterBase - couponValue);
}

std::string CouponStrategy::name() const {
    return "coupon";
}