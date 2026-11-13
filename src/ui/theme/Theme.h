#ifndef APP_THEME_H
#define APP_THEME_H

class QApplication;
class QIcon;

// 全局界面主题：Fusion 风格 + QSS 皮肤，统一配色与控件样式
namespace theme {
    void apply(QApplication& app);

    // 标签页「运行中」角标：绿色播放三角（16px，自带 2x 缩放）。
    // 用 tab icon 而不是文字「▶」——QTabBar 的文字颜色由 QSS 统一控制，
    // 想让某一个标签的文字单独上色只能自定义 delegate
    QIcon runningIndicator();
}

#endif //APP_THEME_H
