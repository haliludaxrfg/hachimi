#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include "Client.h"
#include "TemporaryCart.h"
#include <nlohmann/json.hpp>
#include <QDoubleSpinBox>
#include <QDateTimeEdit>
#include <QComboBox>
#include <QElapsedTimer>

 // 保留原有注释结构
class AdminWindow : public QWidget {
    Q_OBJECT
public:
    // 改为接收 Client* 以通过网络/API 操作数据
    AdminWindow(Client* client, QWidget* parent = nullptr);

signals:
    void backRequested(); // 返回信号

private slots:
    void onReturnToIdentitySelection();
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
    void onApplyGoodsFilter();
    void onClearGoodsFilter();

    // 购物车相关槽（已移除 Add/Save）
    void onLoadCartForUser();
    void onEditCartItem();
    void onRemoveCartItem();

    // 订单相关
    void refreshOrders();
    void onReturnOrder();
    void onRepairOrder();
    void onDeleteOrder();
    void onApplyOrdersFilter();
    void onClearOrdersFilter();
    void onViewOrderDetail();


    // Promotions
    void refreshPromotions();
    void onAddPromotion();
    void onEditPromotion();
    void onDeletePromotion();

    // 订单筛选（时间范围：开始/结束；管理员额外按手机号）    
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

    // ------- 额外：商品搜索/筛选控件 -------
    QLineEdit* goodsNameFilterEdit;    // 名称模糊搜索
    QDoubleSpinBox* priceMinSpin;      // 价格下限
    QDoubleSpinBox* priceMaxSpin;      // 价格上限
    QLineEdit* categoryFilterEdit;     // 分类模糊搜索
    QPushButton* applyGoodsFilterBtn;  // 应用筛选
    QPushButton* clearGoodsFilterBtn;  // 清除筛选

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
    QPushButton* deletePromoBtn;

    QComboBox* promosTypeFilter; // 新增：促销类型筛选下拉

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

    // 订单筛选控件（管理员视图）
    QDateTimeEdit* orderStartEdit;      // 起始时间
    QDateTimeEdit* orderEndEdit;        // 结束时间
    QLineEdit* ordersPhoneFilterEdit;   // 管理员按手机号筛选订单
    QComboBox* ordersStatusFilter;      // 新增：按状态筛选
    QPushButton* applyOrdersFilterBtn;
    QPushButton* clearOrdersFilterBtn;

    // 每秒操作限制相关
    QElapsedTimer actionTimer_;
    bool tryThrottle(QWidget* parent = nullptr);

    // INTERNAL helpers
    // 不带节流的内部刷新，用于在需要避免二次节流（如筛选按钮）时调用
    void refreshGoodsInternal();

    // 构造期间抑制节流弹窗（构造期连续刷新时使用）
    bool suppressThrottle_ = false;
};