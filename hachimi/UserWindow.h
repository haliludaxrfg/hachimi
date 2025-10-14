#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QLineEdit>
#include "Client.h"
#include <QDoubleSpinBox>

class Client;

class UserWindow : public QWidget {
    Q_OBJECT
public:
    // 支持按手机号与 Client 操作
    UserWindow(const std::string& phone = "", Client* client = nullptr, QWidget* parent = nullptr);

signals:
    void backRequested(); // 返回上一级信号
    void accountDeleted();

private slots:
    void refreshGoods();
    void onAddToCart();
    void refreshCart();
    void onCheckout();

    // 新增：用户信息相关槽
    void onSaveUserInfo();
    void onDeleteAccount();

    // 新增：返回身份选择界面槽
    void onReturnToIdentitySelection();
    // 购物车编辑槽
    void onModifyCartItem();
    void onRemoveCartItem();

    // ------- 新增：订单相关槽 -------
    void refreshOrders();
    void onViewOrderDetail();
    void onReturnOrder();
    void onRepairOrder();
    void onDeleteOrder();

    // Promotions 操作槽
    void refreshPromotions();
    void onApplyPromotion();
    void onClearPromotion();
    void onShowOriginalTotal();
    void onShowDiscountedTotal();

    // 新增：商品搜索/筛选槽
    void onApplyGoodsFilter();
    void onClearGoodsFilter();

private:
    QTabWidget* tabWidget;
    QWidget* userTab;   // 个人信息管理
    QWidget* goodsTab;  // 商品浏览与购物车
    QWidget* orderTab;  // 订单查看
    QPushButton* backBtn;

    // 个人信息 UI
    QLineEdit* phoneEdit;
    QLineEdit* passwordEdit;
    QLineEdit* addressEdit;
    QPushButton* saveInfoBtn;
    QPushButton* deleteAccountBtn;

    // 新增：返回身份选择按钮
    QPushButton* returnIdentityBtn;

    // 内部状态
    std::string phone_;
    std::string currentPassword_;
    Client* client_;
    QTableWidget* goodsTable;
    QPushButton* refreshGoodsBtn;
    QPushButton* addToCartBtn;

    // ------- 新增：商品搜索/筛选控件 -------
    QLineEdit* goodsNameFilterEdit;      // 名称搜索
    QDoubleSpinBox* priceMinSpin;        // 价格下限
    QDoubleSpinBox* priceMaxSpin;        // 价格上限
    QLineEdit* categoryFilterEdit;       // 分类搜索
    QPushButton* applyGoodsFilterBtn;    // 应用筛选
    QPushButton* clearGoodsFilterBtn;    // 清除筛选

    QTableWidget* cartTable;
    QPushButton* refreshCartBtn;
    QPushButton* modifyCartBtn;
    QPushButton* removeCartBtn;
    QPushButton* checkoutBtn;
    QPushButton* showOriginalBtn;
    QPushButton* showDiscountedBtn;

    // Promotions UI（用户可见：只读列表 + 应用按钮）
    QComboBox* promoCombo;
    QPushButton* applyPromoBtn;
    QPushButton* clearPromoBtn;

    // 订单 UI
    QTableWidget* orderTable;
    QPushButton* refreshOrdersBtn;
    QPushButton* viewOrderBtn;
    QPushButton* returnOrderBtn;
    QPushButton* repairOrderBtn;
    QPushButton* deleteOrderBtn;
};