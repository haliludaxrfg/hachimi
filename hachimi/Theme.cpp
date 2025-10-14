#include "Theme.h"
#include "logger.h"
#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QColorDialog>
#include <QFileDialog>
#include <QFile>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QMainWindow>
#include <QScrollArea>
#include <QGroupBox>
#include <QFrame>
#include <QTabWidget>
#include <QGraphicsDropShadowEffect>
#include <QFont> // 新增
static void applyTextShadowToLabels(const QColor& textColor);
// 让背景图可见：为顶层窗口与常见容器开启样式背景并设为透明
static void markTopLevelAndContainersTransparent() {
    for (QWidget* w : qApp->topLevelWidgets()) {
        if (!w) continue;

        // 先标记属性，再启用样式背景
        w->setProperty("themeRoot", true);
        w->setAttribute(Qt::WA_StyledBackground, true);

        // 避免反复 += 导致样式不断膨胀；只设置一次透明背景
        if (!w->property("themeTransparentApplied").toBool()) {
            w->setStyleSheet("background: transparent;");
            w->setProperty("themeTransparentApplied", true);
        }

        if (auto mw = qobject_cast<QMainWindow*>(w)) {
            if (QWidget* cw = mw->centralWidget()) {
                cw->setAttribute(Qt::WA_StyledBackground, true);
                if (!cw->property("themeTransparentApplied").toBool()) {
                    cw->setStyleSheet("background: transparent;");
                    cw->setProperty("themeTransparentApplied", true);
                }
            }
        }

        // QTabWidget 及每个 tab page
        for (auto tw : w->findChildren<QTabWidget*>()) {
            tw->setAttribute(Qt::WA_StyledBackground, true);
            if (!tw->property("themeTransparentApplied").toBool()) {
                tw->setStyleSheet("QTabWidget::pane{background: transparent;}");
                tw->setProperty("themeTransparentApplied", true);
            }
            for (int i = 0; i < tw->count(); ++i) {
                if (QWidget* page = tw->widget(i)) {
                    page->setAttribute(Qt::WA_StyledBackground, true);
                    if (!page->property("themeTransparentApplied").toBool()) {
                        page->setStyleSheet("background: transparent;");
                        page->setProperty("themeTransparentApplied", true);
                    }
                }
            }
        }

        // QScrollArea 视口
        for (auto sa : w->findChildren<QScrollArea*>()) {
            if (QWidget* vp = sa->viewport()) {
                vp->setAttribute(Qt::WA_StyledBackground, true);
                if (!vp->property("themeTransparentApplied").toBool()) {
                    vp->setStyleSheet("background: transparent;");
                    vp->setProperty("themeTransparentApplied", true);
                }
            }
        }

        // 常见容器
        for (auto gb : w->findChildren<QGroupBox*>()) {
            gb->setAttribute(Qt::WA_StyledBackground, true);
            if (!gb->property("themeTransparentApplied").toBool()) {
                gb->setStyleSheet("background: transparent;");
                gb->setProperty("themeTransparentApplied", true);
            }
        }
        for (auto fr : w->findChildren<QFrame*>()) {
            fr->setAttribute(Qt::WA_StyledBackground, true);
            if (!fr->property("themeTransparentApplied").toBool()) {
                fr->setStyleSheet("background: transparent;");
                fr->setProperty("themeTransparentApplied", true);
            }
        }
    }
}

Theme& Theme::instance() {
    static Theme* inst = new Theme(qApp);
    return *inst;
}
static QString systemLightStyleSheet();

Theme::Theme(QObject* parent)
    : QObject(parent),
    dark_(false),
    customEnabled_(false),
    customBg_(QColor()),
    customText_(QColor()),
    customAccent_(QColor()),
    backgroundEnabled_(false),
    backgroundImagePath_()
{
    // 默认深色样式（若无自定义配色或浅色模式使用空样式）
    cachedDarkSheet_ = R"(
        QWidget { background-color: #0B1A1E; color: #E6F1F2; font-family: '方正FW珍珠体 简繁','Microsoft YaHei',sans-serif; }
        QTabWidget::pane { background: transparent; }
        QTabBar::tab { background: transparent; padding: 6px 12px; color: #CFEFEA; }
        QTabBar::tab:selected { background: rgba(45,212,191,0.10); border-radius:4px; }
        QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            background-color: #0F2A2E;
            color: #E6F1F2;
            border: 1px solid #1E3A3E;
            border-radius: 4px;
            padding: 4px;
        }
        QLineEdit:focus, QTextEdit:focus, QComboBox:focus { border: 1px solid #2DD4BF; }
        QTableWidget {
            background-color: #071014;
            color: #E6F1F2;
            gridline-color: #122B2E;
            selection-background-color: rgba(45,212,191,0.14);
            selection-color: #E6F1F2;
        }
        QHeaderView::section {
            background-color: #0F2A2E;
            color: #CFEFEA;
            padding: 4px;
            border: 1px solid #122B2E;
        }
        /* 去除按钮渐变，改为纯色 */
        QPushButton {
            background-color: #175656;
            color: #F8FFFE;
            border: 1px solid #1E6E69;
            border-radius: 4px;
            padding: 6px 10px;
        }
        QPushButton:hover { background-color: #1f7f78; }
        QPushButton:pressed { background-color: #0b5b55; }
        QMessageBox { background-color: #071014; color: #E6F1F2; }
        QMenu { background-color: #071014; color: #E6F1F2; }
        QToolTip { background-color: #0f2a2e; color: #E6F1F2; border: 1px solid #1E3A3E; }
    )";

    // 全局设置应用字体（若系统未安装将回退到样式表里的备选字体）
    QFont appFont(QStringLiteral("方正FW珍珠体 简繁"));
    appFont.setStyleStrategy(QFont::PreferAntialias);
    qApp->setFont(appFont);
}

void Theme::initFromSystem() {
    // 先尝试加载保存的偏好；若存在则以保存为准
    loadSettings();

    // 如果已存在用户自定义配色或背景，优先应用之
    if (customEnabled_) {
        apply(dark_);
        if (backgroundEnabled_) applyBackgroundImage(false);
        Logger::instance().info("Theme: loaded custom palette from settings");
        return;
    }
    if (backgroundEnabled_) {
        // 有保存的背景（但无自定义配色）则应用当前深浅模式并背景
        apply(dark_);
        applyBackgroundImage(false);
        Logger::instance().info("Theme: loaded saved background and applied");
        return;
    }

    // --- 新增：当没有任何已保存设置时，使用基于提供图片的默认浅粉紫主题 ---
    // 资源路径（确保已经把图片加入 resources.qrc 并编译进资源）
    const QString defaultBgRes = QStringLiteral(":/images/default_bg.png");

    // 默认配色（与图片色调匹配：浅粉紫背景 + 暗粉色/深灰文本 + 粉色强调）
    QColor defaultBgColor("#FBF5FF");      // 非纯白，偏粉紫的底色
    QColor defaultTextColor("#2E1A2B");    // 深色文本，保证可读性
    QColor defaultAccent("#FF79B4");       // 粉色强调（按钮/选中/焦点）

    // 应用默认配色（不立刻保存，用户可选择保存偏好）
    customEnabled_ = true;
    customBg_ = defaultBgColor;
    customText_ = defaultTextColor;
    customAccent_ = defaultAccent;

    // 设置默认背景（资源路径），启用但不自动保存
    if (QFile::exists(QStringLiteral("assets/default_bg.png")) || QUrl(defaultBgRes).isValid()) {
        // 优先使用资源路径（如果已加入资源）
        backgroundImagePath_ = defaultBgRes;
        backgroundEnabled_ = true;
    } else {
        // 若没有把图片加入资源，则尝试查找项目 assets 开发阶段）
        QString devPath = QStringLiteral("assets/default_bg.png");
        if (QFile::exists(devPath)) {
            backgroundImagePath_ = devPath;
            backgroundEnabled_ = true;
        } else {
            backgroundEnabled_ = false;
            backgroundImagePath_.clear();
            Logger::instance().warn("Theme:initFromSystem default background not found in resources or assets/");
        }
    }

    // 应用主题与背景（不保存），给用户可见的默认主题
    apply(/*dark=*/false);               // 选择浅色基线（图片为亮色系）
    if (backgroundEnabled_) applyBackgroundImage(false);

    Logger::instance().info("Theme: applied default palette and background based on provided image");
}

void Theme::apply(bool dark) {
    if (customEnabled_) {
        // 若启用了自定义配色，优先使用构建的样式表
        QString ss = buildStyleSheetFromColors(customBg_, customText_, customAccent_);
        qApp->setStyleSheet(ss);
        Logger::instance().info("Theme: applied custom palette stylesheet");
        // 为文字添加阴影（基于当前文本色）
        applyTextShadowToLabels(customText_.isValid() ? customText_ : qApp->palette().color(QPalette::WindowText));
    } else {
        // 修复：恢复为“系统默认设置”时，改用你提供的浅色样式，而不是清空样式表
        if (dark) {
            qApp->setStyleSheet(cachedDarkSheet_);
        } else {
            qApp->setStyleSheet(systemLightStyleSheet());
        }
        Logger::instance().info(std::string("Theme: applied ") + (dark ? "dark" : "system-light-css"));
        applyTextShadowToLabels(qApp->palette().color(QPalette::WindowText));
    }
    dark_ = dark;
    emit themeChanged(dark_);
    if (backgroundEnabled_) applyBackgroundImage(false);
}

void Theme::toggle() {
    apply(!dark_);
    saveSettings();
}

QString Theme::darkStyleSheet() const noexcept {
    return cachedDarkSheet_;
}

void Theme::saveSettings() {
    QSettings s("hachimi", "hachimi_app");
    s.beginGroup("Theme");
    s.setValue("dark", dark_);
    s.setValue("customEnabled", customEnabled_);
    if (customEnabled_) {
        s.setValue("bg", customBg_.name(QColor::HexArgb));
        s.setValue("text", customText_.name(QColor::HexArgb));
        s.setValue("accent", customAccent_.name(QColor::HexArgb));
    } else {
        s.remove("bg");
        s.remove("text");
        s.remove("accent");
    }
    // 背景图片保存
    s.setValue("backgroundEnabled", backgroundEnabled_);
    if (backgroundEnabled_) s.setValue("backgroundPath", backgroundImagePath_);
    else s.remove("backgroundPath");
    s.endGroup();
    s.sync();
    Logger::instance().info("Theme: settings saved");
}

void Theme::loadSettings() {
    QSettings s("hachimi", "hachimi_app");
    s.beginGroup("Theme");
    if (s.contains("dark")) {
        dark_ = s.value("dark").toBool();
    }
    customEnabled_ = s.value("customEnabled", false).toBool();
    if (customEnabled_) {
        QString bg = s.value("bg", QString()).toString();
        QString text = s.value("text", QString()).toString();
        QString accent = s.value("accent", QString()).toString();
        if (!bg.isEmpty()) customBg_ = QColor(bg);
        if (!text.isEmpty()) customText_ = QColor(text);
        if (!accent.isEmpty()) customAccent_ = QColor(accent);
        Logger::instance().info("Theme: loaded custom colors from settings");
    }
    // 加载背景设置
    backgroundEnabled_ = s.value("backgroundEnabled", false).toBool();
    if (backgroundEnabled_) {
        backgroundImagePath_ = s.value("backgroundPath", QString()).toString();
        Logger::instance().info(std::string("Theme: loaded background path from settings: ") + backgroundImagePath_.toStdString());
    }
    s.endGroup();
}
QString Theme::buildStyleSheetFromColors(const QColor& bg, const QColor& text, const QColor& accent) const {
    QColor bgc = bg.isValid() ? bg : QColor("#0B1A1E");
    QColor textc = text.isValid() ? text : QColor("#E6F1F2");
    QColor inpBg = bgc.darker(120);
    QColor headerBg = bgc.darker(110);
    QString accentStr = accent.isValid() ? accent.name(QColor::HexArgb) : QString("#2DD4BF");

    // 计算按钮纯色与悬停/按下色（去除渐变）
    QColor accentColor = QColor(accentStr);
    QString accNormal = accentColor.name(QColor::HexArgb);
    QString accHover = accentColor.lighter(115).name(QColor::HexArgb);
    QString accPressed = accentColor.darker(115).name(QColor::HexArgb);
    QString rgbAccent = QString("%1,%2,%3")
        .arg(accentColor.red())
        .arg(accentColor.green())
        .arg(accentColor.blue());

    QString ss = QString(R"(
        QWidget { background-color: %1; color: %2; font-family: '方正FW珍珠体 简繁','Microsoft YaHei',sans-serif; }
        QTabWidget::pane { background: transparent; }
        QTabBar::tab { background: transparent; padding: 6px 12px; color: %2; }
        QTabBar::tab:selected { background: rgba(%4,0.10); border-radius:4px; }
        QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            background-color: %3;
            color: %2;
            border: 1px solid %5;
            border-radius: 4px;
            padding: 4px;
        }
        QLineEdit:focus, QTextEdit:focus, QComboBox:focus { border: 1px solid %6; }
        QTableWidget {
            background-color: %7;
            color: %2;
            gridline-color: %5;
            selection-background-color: rgba(%4,0.14);
            selection-color: %2;
        }
        QHeaderView::section {
            background-color: %8;
            color: %2;
            padding: 4px;
            border: 1px solid %5;
        }
        /* 去除按钮渐变，改为纯色 */
        QPushButton {
            background-color: %9;
            color: %2;
            border: 1px solid %5;
            border-radius: 4px;
            padding: 6px 10px;
        }
        QPushButton:hover { background-color: %10; }
        QPushButton:pressed { background-color: %11; }
        QMessageBox { background-color: %7; color: %2; }
        QMenu { background-color: %7; color: %2; }
        QToolTip { background-color: %3; color: %2; border: 1px solid %5; }
    )")
        .arg(bgc.name(QColor::HexArgb))
        .arg(textc.name(QColor::HexArgb))
        .arg(inpBg.name(QColor::HexArgb))
        .arg(rgbAccent)
        .arg(headerBg.darker(110).name(QColor::HexArgb))
        .arg(accNormal)
        .arg(bgc.darker(140).name(QColor::HexArgb))
        .arg(headerBg.name(QColor::HexArgb))
        .arg(accNormal)
        .arg(accHover)
        .arg(accPressed);

    return ss;
}

void Theme::applyPaletteColors(const QColor& background, const QColor& text, const QColor& accent, bool save) {
    customEnabled_ = true;
    customBg_ = background;
    customText_ = text;
    customAccent_ = accent;
    QString ss = buildStyleSheetFromColors(customBg_, customText_, customAccent_);
    qApp->setStyleSheet(ss);
    Logger::instance().info("Theme: applied palette colors");
    // 同步应用文字阴影
    applyTextShadowToLabels(customText_.isValid() ? customText_ : qApp->palette().color(QPalette::WindowText));
    emit themeChanged(dark_);
    if (save) saveSettings();
}

bool Theme::showPaletteEditor(QWidget* parent) {
    QColor bg = QColorDialog::getColor(customBg_.isValid() ? customBg_ : qApp->palette().color(QPalette::Window),
                                       parent, "选择背景颜色");
    if (!bg.isValid()) return false;
    QColor txt = QColorDialog::getColor(customText_.isValid() ? customText_ : qApp->palette().color(QPalette::WindowText),
                                        parent, "选择文字颜色");
    if (!txt.isValid()) return false;
    QColor acc = QColorDialog::getColor(customAccent_.isValid() ? customAccent_ : QColor("#2DD4BF"),
                                        parent, "选择强调颜色（按钮/选中/焦点）");
    if (!acc.isValid()) return false;

    applyPaletteColors(bg, txt, acc, true);
    return true;
}

void Theme::clearCustomColors(bool save) {
    customEnabled_ = false;
    customBg_ = QColor();
    customText_ = QColor();
    customAccent_ = QColor();
    // 重新应用 current dark/light mode
    apply(dark_);
    if (save) saveSettings();
    Logger::instance().info("Theme: cleared custom colors");
}

// -------- 背景图片相关实现 --------

// 禁用：加载背景图片
bool Theme::showBackgroundImageSelector(QWidget* parent, bool save) {
    (void)parent; (void)save;
    Logger::instance().info("Theme: background image selection is disabled");
    return false;
}

// 禁用：应用背景图片（保持现有颜色样式，不叠加图片）
void Theme::applyBackgroundImage(bool save) {
    (void)save;
    // 不做任何处理，避免修改样式表
}

// 禁用：清除背景图片
void Theme::clearBackgroundImage(bool save) {
    (void)save;
    // 不做任何处理，避免修改样式表
}

// -------- 主题预设管理实现 --------

QString Theme::presetsFilePath() const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.hachimi";
    }
    QDir dir(base);
    if (!dir.exists()) dir.mkpath(".");
    QString file = dir.filePath("theme_presets.ini");
    return file;
}

bool Theme::writePreset(int slotIndex, const Preset& p) {
    if (slotIndex < 1 || slotIndex > 5) return false;
    QSettings ini(presetsFilePath(), QSettings::IniFormat);
    ini.beginGroup(QString("Preset%1").arg(slotIndex));
    ini.setValue("name", p.name);
    ini.setValue("dark", p.dark);
    ini.setValue("customEnabled", p.customEnabled);
    ini.setValue("customBg", p.customBg.name(QColor::HexArgb));
    ini.setValue("customText", p.customText.name(QColor::HexArgb));
    ini.setValue("customAccent", p.customAccent.name(QColor::HexArgb));
    ini.setValue("backgroundEnabled", p.backgroundEnabled);
    ini.setValue("backgroundImagePath", p.backgroundImagePath);
    ini.endGroup();
    ini.sync();
    return (ini.status() == QSettings::NoError);
}

static QColor colorFromHex(const QVariant& v, const QColor& fallback = QColor()) {
    QColor c(v.toString());
    return c.isValid() ? c : fallback;
}

bool Theme::loadPresetFromStorage(int slotIndex, Preset& out) {
    if (slotIndex < 1 || slotIndex > 5) return false;
    QSettings ini(presetsFilePath(), QSettings::IniFormat);
    ini.beginGroup(QString("Preset%1").arg(slotIndex));
    if (!ini.contains("dark") && !ini.contains("customEnabled") && !ini.contains("backgroundEnabled")) {
        ini.endGroup();
        return false; // 空槽
    }
    out.name = ini.value("name", QString("预设 %1").arg(slotIndex)).toString();
    out.dark = ini.value("dark", false).toBool();
    out.customEnabled = ini.value("customEnabled", false).toBool();
    out.customBg = colorFromHex(ini.value("customBg"), QColor("#FF0B1A1E"));
    out.customText = colorFromHex(ini.value("customText"), QColor("#FFE6F1F2"));
    out.customAccent = colorFromHex(ini.value("customAccent"), QColor("#FF2DD4BF"));
    out.backgroundEnabled = ini.value("backgroundEnabled", false).toBool();
    out.backgroundImagePath = ini.value("backgroundImagePath").toString();
    ini.endGroup();
    return true;
}

bool Theme::readPreset(int slotIndex, Preset& outPreset) const {
    return const_cast<Theme*>(this)->loadPresetFromStorage(slotIndex, outPreset);
}

bool Theme::savePreset(int slotIndex, const QString& customName) {
    Preset p;
    p.name = customName.isEmpty() ? QString("预设 %1").arg(slotIndex) : customName;
    p.dark = dark_;
    p.customEnabled = customEnabled_;
    p.customBg = customBg_;
    p.customText = customText_;
    p.customAccent = customAccent_;
    p.backgroundEnabled = backgroundEnabled_;
    p.backgroundImagePath = backgroundImagePath_;
    return writePreset(slotIndex, p);
}

bool Theme::applyPreset(int slotIndex) {
    Preset p;
    if (!loadPresetFromStorage(slotIndex, p)) {
        QMessageBox::warning(nullptr, "主题预设", QString("槽位 %1 为空，无法应用").arg(slotIndex));
        return false;
    }
    // 应用深浅色
    apply(p.dark);
    // 应用调色板（不重复保存）
    customEnabled_ = p.customEnabled;
    if (p.customEnabled) {
        applyPaletteColors(p.customBg, p.customText, p.customAccent, false);
    } else {
        clearCustomColors(false);
    }
    // 应用背景图
    backgroundEnabled_ = p.backgroundEnabled;
    backgroundImagePath_ = p.backgroundImagePath;
    if (backgroundEnabled_ && !backgroundImagePath_.isEmpty()) {
        applyBackgroundImage(false);
    } else {
        clearBackgroundImage(false);
    }
    // 保存为当前主题
    saveSettings();
    emit themeChanged(dark_);
    emit backgroundChanged(backgroundImagePath_);
    return true;
}

bool Theme::showPresetManager(QWidget* parent) {
    QDialog dlg(parent);
    dlg.setWindowTitle("主题预设管理");
    QVBoxLayout* v = new QVBoxLayout(&dlg);

    QHBoxLayout* row1 = new QHBoxLayout;
    row1->addWidget(new QLabel("预设槽位:"));
    QComboBox* slotCombo = new QComboBox(&dlg);
    for (int i = 1; i <= 5; ++i) slotCombo->addItem(QString("预设 %1").arg(i), i);
    row1->addWidget(slotCombo);
    row1->addWidget(new QLabel("名称:"));
    QLineEdit* nameEdit = new QLineEdit(&dlg);
    row1->addWidget(nameEdit);
    v->addLayout(row1);

    QHBoxLayout* row2 = new QHBoxLayout;
    QPushButton* editPaletteBtn = new QPushButton("编辑调色板", &dlg);
    row2->addWidget(editPaletteBtn);
    v->addLayout(row2);

    QHBoxLayout* row3 = new QHBoxLayout;
    QPushButton* saveBtn = new QPushButton("保存到该槽", &dlg);
    QPushButton* applyBtn = new QPushButton("应用该槽", &dlg);
    QPushButton* resetBtn = new QPushButton("重置为系统主题", &dlg);
    QPushButton* closeBtn = new QPushButton("关闭", &dlg);
    row3->addWidget(saveBtn);
    row3->addWidget(applyBtn);
    row3->addWidget(resetBtn);
    row3->addStretch();
    row3->addWidget(closeBtn);
    v->addLayout(row3);

    // 初始化名称显示
    auto refreshName = [&]() {
        int slotIndex = slotCombo->currentData().toInt();
        Preset p;
        if (loadPresetFromStorage(slotIndex, p)) {
            nameEdit->setText(p.name);
        } else {
            nameEdit->setText(QString("预设 %1").arg(slotIndex));
        }
    };
    refreshName();

    QObject::connect(slotCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                     &dlg, [&](int){ refreshName(); });

    QObject::connect(editPaletteBtn, &QPushButton::clicked, &dlg, [&]() {
        showPaletteEditor(&dlg);
    });

    QObject::connect(saveBtn, &QPushButton::clicked, &dlg, [&]() {
        int slotIndex = slotCombo->currentData().toInt();
        if (savePreset(slotIndex, nameEdit->text().trimmed()))
            QMessageBox::information(&dlg, "主题预设", "已保存到槽位");
        else
            QMessageBox::warning(&dlg, "主题预设", "保存失败");
    });
    QObject::connect(applyBtn, &QPushButton::clicked, &dlg, [&]() {
        int slotIndex = slotCombo->currentData().toInt();
        applyPreset(slotIndex);
    });
    // 调整“重置为系统主题”逻辑：真正回到系统默认（此处为自定义的系统浅色样式）
    QObject::connect(resetBtn, &QPushButton::clicked, &dlg, [&]() {
        // 清除自定义调色板与背景配置
        customEnabled_ = false;
        backgroundEnabled_ = false;
        backgroundImagePath_.clear();
        // 设为浅色并应用系统浅色样式
        dark_ = false;
        apply(false);
        saveSettings();
        QMessageBox::information(&dlg, "主题预设", "已恢复为系统默认样式（浅色 CSS）");
    });
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, [&]() { dlg.accept(); });

    dlg.setMinimumWidth(520);
    return dlg.exec() == QDialog::Accepted;
}

// 在 Theme.cpp 文件底部实现
static QString systemLightStyleSheet() {
    return QString::fromUtf8(R"(
QWidget { 
    background-color: #FFFFFF;  /* 白色主背景 */
    color: #333333;             /* 深灰色文字 */
    font-family: '方正FW珍珠体 简繁','Microsoft YaHei',sans-serif;
}

/* 标签页样式 */
QTabBar::tab { 
    background: transparent; 
    padding: 6px 12px; 
    color: #666666;             /* 中性灰色 */
}
QTabBar::tab:selected { 
    background: rgba(255, 105, 180, 0.1);  /* 粉红色透明背景 */
    border-radius: 4px; 
    color: #FF69B4;             /* 粉红色 */
}

/* 输入控件 */
QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background-color: #F8F9FA;   /* 浅灰背景 */
    color: #333333;
    border: 1px solid #DDDDDD;   /* 浅灰边框 */
    border-radius: 4px;
    padding: 4px;
}
QLineEdit:focus, QTextEdit:focus, QComboBox:focus { 
    border: 1px solid #87CEEB;   /* 天蓝色焦点边框 */
}

/* 表格样式 */
QTableWidget {
    background-color: #FFFFFF;
    color: #333333;
    gridline-color: #EEEEEE;
    selection-background-color: rgba(135, 206, 235, 0.2);  /* 天蓝色选中 */
    selection-color: #333333;
}

/* 按钮样式 */
QPushButton {
    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FF69B4, stop:1 #FF1493); /* 粉红渐变 */
    color: #FFFFFF;              /* 白色文字 */
    border: 1px solid #FF69B4;
    border-radius: 4px;
    padding: 6px 10px;
}
QPushButton:hover { 
    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FF85C1, stop:1 #FF43A4); 
}
QPushButton:pressed { 
    background-color: #E75480;   /* 深粉红 */
}

/* 其他元素 */
QHeaderView::section {
    background-color: #F0F0F0;   /* 浅灰背景 */
    color: #333333;              /* 黑色文字 */
    border: 1px solid #DDDDDD;
}
)");
}

// 为所有 QLabel 应用/更新文本阴影（尽量不影响已有自定义效果）
// 为所有 QLabel 应用/更新“黑色描边”效果
static void applyTextShadowToLabels(const QColor& textColor) {
    (void)textColor; // 描边固定为黑色，忽略传入颜色
    const QColor outline(0, 0, 0, 230); // 纯黑，略透明提升观感

    for (QWidget* w : qApp->topLevelWidgets()) {
        if (!w) continue;
        const auto labels = w->findChildren<QLabel*>();
        for (QLabel* lbl : labels) {
            if (!lbl) continue;
            if (lbl->text().trimmed().isEmpty()) continue;

            auto existing = qobject_cast<QGraphicsDropShadowEffect*>(lbl->graphicsEffect());
            if (existing) {
                if (lbl->property("themeTextShadowApplied").toBool()) {
                    existing->setColor(outline);
                    existing->setBlurRadius(0.8);              // 小模糊模拟描边
                    existing->setOffset(QPointF(0.0, 0.0));    // 无偏移 => 描边环绕
                }
                continue;
            }

            auto effect = new QGraphicsDropShadowEffect(lbl);
            effect->setBlurRadius(0.8);
            effect->setOffset(QPointF(0.0, 0.0));
            effect->setColor(outline);
            lbl->setGraphicsEffect(effect);
            lbl->setProperty("themeTextShadowApplied", true);
        }
    }
}