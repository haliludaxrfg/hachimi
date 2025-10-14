#pragma once

#ifndef HACHIMI_THEME_H
#define HACHIMI_THEME_H

#include <QObject>
#include <QString>
#include <QColor>

class QWidget;

class Theme : public QObject {
    Q_OBJECT
public:
    // 单例访问
    static Theme& instance();

    // 初始化（从系统或已保存设置）
    void initFromSystem();

    // 立即应用深色或浅色主题（若启用了自定义配色则优先使用）
    void apply(bool dark);

    // 切换当前主题并保存
    void toggle();

    bool isDark() const noexcept { return dark_; }

    // 持久化：保存/加载用户偏好（包含自定义颜色与背景图）
    void saveSettings();
    void loadSettings();

    // 自定义颜色（已有）
    void applyPaletteColors(const QColor& background, const QColor& text, const QColor& accent, bool save = true);
    bool showPaletteEditor(QWidget* parent = nullptr);
    void clearCustomColors(bool save = true);
    bool isUsingCustomColors() const noexcept { return customEnabled_; }

    // 新增：背景图片相关
    // 弹出文件选择对话，选择后应用并可保存（返回 true 表示已应用）
    bool showBackgroundImageSelector(QWidget* parent = nullptr, bool save = true);

    // 直接应用已设置的背景图片（如果存在）
    void applyBackgroundImage(bool save = true);

    // 清除背景图片（并可保存设置）
    void clearBackgroundImage(bool save = true);

    // 获取当前深色样式（供调试/扩展）
    QString darkStyleSheet() const noexcept;

signals:
    void themeChanged(bool dark);
    void backgroundChanged(const QString& backgroundPath); // 发出新背景路径（空 => 已清除）

private:
    explicit Theme(QObject* parent = nullptr);
    QString buildStyleSheetFromColors(const QColor& bg, const QColor& text, const QColor& accent) const;

    bool dark_;
    bool customEnabled_;
    QColor customBg_;
    QColor customText_;
    QColor customAccent_;
    QString cachedDarkSheet_;

    // 背景图片持久化字段
    bool backgroundEnabled_;
    QString backgroundImagePath_;
};

#endif // HACHIMI_THEME_H
