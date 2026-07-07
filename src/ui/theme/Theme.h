#ifndef APP_THEME_H
#define APP_THEME_H

class QApplication;

// 全局界面主题：Fusion 风格 + QSS 皮肤，统一配色与控件样式
namespace theme {
    void apply(QApplication& app);
}

#endif //APP_THEME_H
