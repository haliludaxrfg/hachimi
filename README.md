2024级吉林大学软件工程卓越工程师班面向对象课程设计项目。要求设计一个微商系统。

# Hachimi 电商重制（Qt/MSVC）

一个基于 Qt + C++ 的本地电商演示项目，内置客户端（用户端/管理员端）、轻量 TCP 服务器与 MySQL 数据存储，支持商品、用户、购物车、订单与促销策略等功能。

## 主要功能
- 用户端
  - 商品浏览与筛选（名称/分类/价格）
  - 购物车增删改与结算（库存校验、原价/折后价查看、应用/清除促销）
  - 订单列表与时间区间筛选（兼容 c/o 前缀订单号）、查看详情、退货/维修/删除
  - 个人信息修改（地址/密码），主题切换
- 管理员端
  - 用户/商品/订单/购物车管理
  - 促销策略管理：新增/删除/更新与重命名（支持 discount/tiered/full_reduction）
- 其他
  - 本地 TCP 服务端（QtTcpServer）监听 127.0.0.1:8888
  - JSON 协议（nlohmann/json），日志输出到 log.txt
  - 订单号生成：o + yyyyMMddHHmmsszzz + "_" + 随机16进制（长度超出截断）

## 目录结构（节选）
- `hachimi/Client.h|cpp`：客户端请求封装（QTcpSocket）
- `hachimi/Server.h|cpp`：服务器与业务分发（QTcpServer）
- `hachimi/databaseManager.h|cpp`：MySQL 访问
- `hachimi/AdminWindow.*`：管理员界面
- `hachimi/UserWindow.*`：用户界面
- `hachimi/LoginWindow.*`：登录/身份选择
- `hachimi/Theme.*`：主题管理
- `hachimi/good.h|user.h|order.*|TemporaryCart.*`：核心模型

## 环境与依赖
- Windows + Visual Studio 2022
- Qt 5/6（MSVC 工具链）
- MySQL Server 8.x（本地或远程）
- nlohmann/json（头文件库）

## 构建与运行
1. 准备依赖
   - 安装 Qt（配置 VS2022 的 Qt MSVC 工具集）。
   - 安装 MySQL 并创建数据库（示例名：`remake`），确保已创建所需表结构。
2. 打开项目
   - 使用 VS2022 打开 `hachimi.vcxproj`（或打开文件夹构建）。
3. 配置服务端与数据库
   - 服务器监听：`Server.cpp` 默认绑定 `127.0.0.1:8888`。
   - 客户端连接：`Client.cpp` 默认连接 `127.0.0.1:8888`。
   - 数据库连接：`Server.cpp` 中 `DatabaseManager(host,user,pwd,db,port)`（请按实际环境修改凭据）。
4. 编译运行
   - 选择 x64 Debug/Release，构建并运行。
   - 启动后通过登录/身份选择进入“用户端”或“管理员端”。

## 使用提示
- 管理员端包含：用户/商品/订单/购物车/促销标签页；促销支持重命名（带 `new_name` 字段）。
- 用户端订单筛选支持 c/o 前缀订单号时间解析；结算会校验库存并清空购物车。
- 日志在 `hachimi/log.txt`，商品过多时日志仅输出摘要以提升可读性。

## 配置项速览
- 端口/IP：`Server.cpp::SERstart()`、`Client.cpp::Impl(ip,port)`
- 数据库：`Server.cpp` 创建 `DatabaseManager(...)`
- 订单号规则：`UserWindow.cpp::generateOrderId_User()`

## 许可
本项目用于学习与演示，涉及的账号/数据库配置请勿在生产环境使用。
