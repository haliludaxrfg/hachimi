#include "promotion.h"

std::shared_ptr<PromotionStrategy> PromotionStrategy::fromJson(const nlohmann::json& js) {
    // 支持混合策略
    if (js.contains("strategies")) {
        std::shared_ptr<PromotionStrategy> base;
        for (const auto& strat : js["strategies"]) {
            std::string type = strat["type"];
            if (type == "discount") {
                base = std::make_shared<DiscountStrategy>(strat["value"]);
            } else if (type == "step_discount") {
                base = std::make_shared<StepDiscountStrategy>();
            } else if (type == "reduction") {
                base = std::make_shared<PriceReductionStrategy>(strat["value"]);
            }
            // 装饰器策略
            if (type == "coupon" && base) {
                base = std::make_shared<CouponStrategy>(base, strat["value"]);
            }
        }
        return base;
    } else {
        // 单一策略
        std::string type = js["type"];
        if (type == "discount") {
            return std::make_shared<DiscountStrategy>(js["value"]);
        } else if (type == "step_discount") {
            return std::make_shared<StepDiscountStrategy>();
        } else if (type == "reduction") {
            return std::make_shared<PriceReductionStrategy>(js["value"]);
        }
    }
    return nullptr;
}