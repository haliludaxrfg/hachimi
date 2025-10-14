#include "Theme.h"
#include "logger.h"
#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QColorDialog>
#include <QFileDialog>
#include <QFile>
#include <QUrl>

Theme& Theme::instance() {
    static Theme* inst = new Theme(qApp);
    return *inst;
}

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
        QWidget { background-color: #0B1A1E; color: #E6F1F2; }
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
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #175656, stop:1 #0F3B3E);
            color: #F8FFFE;
            border: 1px solid #1E6E69;
            border-radius: 4px;
            padding: 6px 10px;
        }
        QPushButton:hover { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1f7f78, stop:1 #126c68); }
        QPushButton:pressed { background-color: #0b5b55; }
        QMessageBox { background-color: #071014; color: #E6F1F2; }
        QMenu { background-color: #071014; color: #E6F1F2; }
        QToolTip { background-color: #0f2a2e; color: #E6F1F2; border: 1px solid #1E3A3E; }
    )";
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
        // 若没有把图片加入资源，则尝试查找项目 assets 路径（开发阶段）
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
    } else {
        if (dark) qApp->setStyleSheet(cachedDarkSheet_);
        else qApp->setStyleSheet(QString());
        Logger::instance().info(std::string("Theme: applied ") + (dark ? "dark" : "light"));
    }
    dark_ = dark;
    emit themeChanged(dark_);
    // 如果已配置背景图，确保背景样式被叠加（apply 不会移除 backgroundEnabled_）
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

    QString ss = QString(R"(
        QWidget { background-color: %1; color: %2; }
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
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %9, stop:1 %10);
            color: %2;
            border: 1px solid %5;
            border-radius: 4px;
            padding: 6px 10px;
        }
        QPushButton:hover { background-color: %11; }
        QPushButton:pressed { background-color: %12; }
        QMessageBox { background-color: %7; color: %2; }
        QMenu { background-color: %7; color: %2; }
        QToolTip { background-color: %3; color: %2; border: 1px solid %5; }
    )")
        .arg(bgc.name(QColor::HexArgb))
        .arg(textc.name(QColor::HexArgb))
        .arg(inpBg.name(QColor::HexArgb))
        .arg(QString::number(QColor(accentStr).red()) + "," + QString::number(QColor(accentStr).green()) + "," + QString::number(QColor(accentStr).blue()))
        .arg(headerBg.darker(110).name(QColor::HexArgb))
        .arg(accentStr)
        .arg(bgc.darker(140).name(QColor::HexArgb))
        .arg(headerBg.name(QColor::HexArgb))
        .arg(accentStr)
        .arg(bgc.name(QColor::HexArgb))
        .arg(QColor(accentStr).name(QColor::HexArgb))
        .arg(accentStr);

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

bool Theme::showBackgroundImageSelector(QWidget* parent, bool save) {
    QString fn = QFileDialog::getOpenFileName(parent, "选择背景图片", QString(), "图片文件 (*.png *.jpg *.jpeg *.bmp)");
    if (fn.isEmpty()) return false;
    if (!QFile::exists(fn)) {
        Logger::instance().warn(std::string("Theme: selected background file does not exist: ") + fn.toStdString());
        return false;
    }
    backgroundImagePath_ = fn;
    backgroundEnabled_ = true;
    applyBackgroundImage(save);
    return true;
}

void Theme::applyBackgroundImage(bool save) {
    if (!backgroundEnabled_ || backgroundImagePath_.isEmpty()) {
        // 若没有背景，确保恢复纯样式（但保留颜色样式）
        if (customEnabled_) qApp->setStyleSheet(buildStyleSheetFromColors(customBg_, customText_, customAccent_));
        else qApp->setStyleSheet(dark_ ? cachedDarkSheet_ : QString());
        emit backgroundChanged(QString());
        if (save) saveSettings();
        return;
    }

    // 构造背景样式（使用 file:// URL）
    QUrl url = QUrl::fromLocalFile(backgroundImagePath_);
    QString bgRule = QString("QWidget { background-image: url(\"%1\"); background-repeat: no-repeat; background-position: center; background-attachment: fixed; background-size: cover; }")
                        .arg(url.toString());

    // 基本样式（颜色）与背景规则合并：把背景规则追加到基础样式
    QString base;
    if (customEnabled_) base = buildStyleSheetFromColors(customBg_, customText_, customAccent_);
    else base = (dark_ ? cachedDarkSheet_ : QString());

    QString finalSheet = base + "\n" + bgRule;
    qApp->setStyleSheet(finalSheet);
    emit backgroundChanged(backgroundImagePath_);
    Logger::instance().info(std::string("Theme: applied background image: ") + backgroundImagePath_.toStdString());
    if (save) saveSettings();
}

void Theme::clearBackgroundImage(bool save) {
    backgroundEnabled_ = false;
    backgroundImagePath_.clear();
    applyBackgroundImage(save); // will reapply base style and save if requested
    Logger::instance().info("Theme: cleared background image");
}