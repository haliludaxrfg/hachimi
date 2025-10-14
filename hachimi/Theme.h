#pragma once
#include <QObject>
#include <QColor>
#include <QString>

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

    // 背景图片相关
    bool showBackgroundImageSelector(QWidget* parent = nullptr, bool save = true);
    void applyBackgroundImage(bool save = true);
    void clearBackgroundImage(bool save = true);

    // 获取当前深色样式（供调试/扩展）
    QString darkStyleSheet() const noexcept;

    // ========== 新增：主题预设（5 槽）==========
    struct Preset {
        QString name;                // 预设名称
        bool dark = false;           // 深色/浅色
        bool customEnabled = false;  // 是否启用自定义调色板
        QColor customBg;
        QColor customText;
        QColor customAccent;
        bool backgroundEnabled = false;
        QString backgroundImagePath;
    };

    // 打开预设管理器（统一给 Admin/User 调用）
    bool showPresetManager(QWidget* parent = nullptr);

    // 保存当前主题到指定槽位(1..5)
    bool savePreset(int slotIndex, const QString& customName = QString());

    // 应用指定槽位(1..5)并保存为当前主题
    bool applyPreset(int slotIndex);

    // 读取槽位配置（不应用）
    bool readPreset(int slotIndex, Preset& outPreset) const;

signals:
    void themeChanged(bool dark);
    void backgroundChanged(const QString& backgroundPath); // 发出新背景路径（空 => 已清除）

private:
    explicit Theme(QObject* parent = nullptr);
    QString buildStyleSheetFromColors(const QColor& bg, const QColor& text, const QColor& accent) const;

    // 预设读写辅助
    QString presetsFilePath() const; // 本地 INI 文件路径
    bool writePreset(int slotIndex, const Preset& preset);
    bool loadPresetFromStorage(int slotIndex, Preset& out);

    // 当前状态
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