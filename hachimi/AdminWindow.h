#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include "Client.h"
#include "TemporaryCart.h"
#include <nlohmann/json.hpp>

// 保留原有注释结构
class AdminWindow : public QWidget {
    Q_OBJECT
public:
    // 改为接收 Client* 以通过网络/API 操作数据
    AdminWindow(Client* client, QWidget* parent = nullptr);

signals:
    void backRequested(); // 返回信号

private slots:
    // 用户相关
    void refreshUsers();
	void onAddUser();
	void onEditUser();
    void onDeleteUser();
    void onSearchUser(); // 新增：依据手机号查询用户

    // 商品相关
    void refreshGoods();
    void onAddGood();
    void onEditGood();
    void onDeleteGood();

    // 订单相关
    void refreshOrders();
    // 新增：订单操作槽
    void onReturnOrder();
    void onRepairOrder();
    void onDeleteOrder();
    // 新增：查看订单详情（管理员）
    void onViewOrderDetail();

    // 新增：返回身份选择界面槽
    void onReturnToIdentitySelection();

    // 购物车相关槽（已移除 Add/Save）
    void onLoadCartForUser();
    void onEditCartItem();
    void onRemoveCartItem();

    // Promotions
    void refreshPromotions();
    void onAddPromotion();
    void onEditPromotion();
    void onDeletePromotion();

private:
    // UI 主控件
    QTabWidget* tabWidget;
    QWidget* userTab;
    QWidget* goodTab;
    QWidget* orderTab;
    QPushButton* backBtn;

    // 新增 UI 成员
    Client* client_;
    QTableWidget* usersTable;
    QPushButton* refreshUsersBtn;
    QPushButton* addUserBtn;    // 新增：添加用户按钮
    QPushButton* editUserBtn;   // 新增：修改用户按钮
    QPushButton* deleteUserBtn;
    QPushButton* searchUserBtn; // 新增：查询用户按钮

    // 新增：返回身份选择按钮
    QPushButton* returnIdentityBtn;

    // 商品 UI
    QTableWidget* goodsTable;
    QPushButton* refreshGoodsBtn;
    QPushButton* addGoodBtn;
    QPushButton* editGoodBtn;
    QPushButton* deleteGoodBtn;

    // 订单 UI
    QTableWidget* ordersTable;
    QPushButton* refreshOrdersBtn;
    // 新增：订单操作按钮
    QPushButton* returnOrderBtn;
    QPushButton* repairOrderBtn;
    QPushButton* deleteOrderBtn;
    // 新增：查看订单详情按钮
    QPushButton* viewOrderBtn;

    // ------- 购物车管理 UI -------
    QWidget* cartTab;
    QLineEdit* cartPhoneEdit;
    QPushButton* loadCartBtn;
    QTableWidget* cartTable; // good_id, name, price, qty, subtotal
    QPushButton* editCartItemBtn;
    QPushButton* removeCartItemBtn;

    // 当前加载的购物车缓存
    TemporaryCart currentCart_;

    // Promotions UI
    QWidget* promoTab;
    QTableWidget* promoTable;
    QPushButton* refreshPromosBtn;
    QPushButton* addPromoBtn;
    QPushButton* editPromoBtn;
    QPushButton* deletePromoBtn;

    // helper: 在 cpp 中实现 createPromotionsTab()
    void createPromotionsTab();

    // Promotion 编辑辅助类型与方法
    enum class PromotionKind { Discount, Tiered, FullReduction, Unknown };
    // 显示选择促销类型对话框
    static bool showPromotionTypeSelector(QWidget* parent, PromotionKind& outKind);
    // 根据促销类型弹出专用编辑器，返回 true + policyJson 填充；若返回 false 则取消
    static bool showTypedPromotionEditor(QWidget* parent, PromotionKind kind,
                                         const std::string& inName, const nlohmann::json& inPolicyJson, const std::string& inPolicyDetail,
                                         std::string& outName, nlohmann::json& outPolicyJson, std::string& outPolicyDetail);
};