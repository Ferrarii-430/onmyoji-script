#include "Theme.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QPolygonF>
#include <QStyleFactory>

#include <functional>

namespace theme {

namespace {
// 配色：石板底 + 靛蓝主色，启动/停止按钮用语义色
const char* kStyleSheet = R"QSS(
/* ---------- 基础 ---------- */
QWidget {
    background-color: #f4f5f7;
    color: #2b2d33;
    font-size: 13px;
}
QDialog, QMessageBox {
    background-color: #f4f5f7;
}
QLabel {
    background: transparent;
}
QToolTip {
    background-color: #2b2d33;
    color: #ffffff;
    border: none;
    padding: 4px 8px;
    border-radius: 4px;
}

/* ---------- 按钮 ---------- */
QPushButton, QToolButton {
    background-color: #5b6cf0;
    color: #ffffff;
    border: none;
    border-radius: 5px;
    padding: 3px 8px;
    min-height: 20px;
}
QPushButton:hover, QToolButton:hover {
    background-color: #6f7ef2;
}
QPushButton:pressed, QToolButton:pressed {
    background-color: #4a59d6;
}
QPushButton:disabled, QToolButton:disabled {
    background-color: #c8ccd8;
    color: #f0f0f0;
}

/* 启动/停止/设置：语义配色 */
QToolButton#startTaskButton {
    background-color: #34a853;
    font-weight: bold;
}
QToolButton#startTaskButton:hover { background-color: #3fbb60; }
QToolButton#startTaskButton:pressed { background-color: #2c8f46; }
QToolButton#stopTaskButton {
    background-color: #ea4335;
    font-weight: bold;
}
QToolButton#stopTaskButton:hover { background-color: #f0564a; }
QToolButton#stopTaskButton:pressed { background-color: #c93a2e; }
QToolButton#settingButton {
    background-color: #64748b;
    font-weight: bold;
}
QToolButton#settingButton:hover { background-color: #7586a0; }
QToolButton#settingButton:pressed { background-color: #55637d; }

/* ---------- 输入控件 ---------- */
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit {
    background-color: #ffffff;
    border: 1px solid #d4d7e0;
    border-radius: 5px;
    padding: 3px 6px;
    selection-background-color: #5b6cf0;
    selection-color: #ffffff;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QTextEdit:focus {
    border: 1px solid #5b6cf0;
}
QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled {
    background-color: #ececf1;
    color: #9a9daa;
}
QComboBox::drop-down {
    border: none;
    width: 20px;
}
/* SpinBox/下拉箭头：QSS 自定义了按钮子控件后 Qt 不再画默认箭头，
   必须显式提供 image；图标由 theme::apply 启动时绘制生成 */
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
    background-color: #e6e8ef;
    border: none;
    width: 18px;
    border-radius: 2px;
    margin: 1px;
}
QSpinBox::up-button:hover, QSpinBox::down-button:hover,
QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {
    background-color: #d4d7e0;
}
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
    image: url(%ARROW_UP%);
    width: 10px;
    height: 6px;
}
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
    image: url(%ARROW_DOWN%);
    width: 10px;
    height: 6px;
}
QComboBox::down-arrow {
    image: url(%ARROW_DOWN%);
    width: 10px;
    height: 6px;
}
QComboBox QAbstractItemView {
    background-color: #ffffff;
    border: 1px solid #d4d7e0;
    selection-background-color: #5b6cf0;
    selection-color: #ffffff;
    outline: none;
}
QCheckBox {
    background: transparent;
    spacing: 6px;
}
QCheckBox::indicator {
    width: 15px;
    height: 15px;
    border: 1px solid #c0c4d0;
    border-radius: 3px;
    background-color: #ffffff;
}
QCheckBox::indicator:checked {
    background-color: #5b6cf0;
    border-color: #5b6cf0;
    image: url(%CHECK_MARK%);
}

/* ---------- 列表 / 表格 ---------- */
QListWidget, QTableWidget, QTableView {
    background-color: #ffffff;
    border: 1px solid #d4d7e0;
    border-radius: 6px;
    outline: none;
    alternate-background-color: #f7f8fb;
}
QListWidget::item {
    padding: 4px 6px;
    border-radius: 4px;
}
QListWidget::item:hover, QTableWidget::item:hover {
    background-color: #eef0fb;
}
QListWidget::item:selected, QTableWidget::item:selected {
    background-color: #5b6cf0;
    color: #ffffff;
}
QHeaderView {
    background: transparent;
}
QHeaderView::section {
    background-color: #eceef4;
    color: #4a4d57;
    border: none;
    border-bottom: 1px solid #d4d7e0;
    padding: 4px 6px;
    font-weight: bold;
}
QTableCornerButton::section {
    background-color: #eceef4;
    border: none;
}

/* ---------- 日志输出：控制台风格 ---------- */
QPlainTextEdit {
    background-color: #1f2430;
    color: #d8dee9;
    border: 1px solid #171b24;
    border-radius: 6px;
    padding: 4px;
    font-family: "Consolas", "Courier New", monospace;
    selection-background-color: #5b6cf0;
}

/* ---------- 滚动条 ---------- */
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #c3c7d4;
    border-radius: 5px;
    min-height: 24px;
}
QScrollBar::handle:vertical:hover { background: #a9aebf; }
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: #c3c7d4;
    border-radius: 5px;
    min-width: 24px;
}
QScrollBar::handle:horizontal:hover { background: #a9aebf; }
QScrollBar::add-line, QScrollBar::sub-line {
    width: 0;
    height: 0;
}
QScrollBar::add-page, QScrollBar::sub-page {
    background: transparent;
}

/* ---------- 其他 ---------- */
QStackedWidget {
    background: transparent;
}
QDialogButtonBox {
    /* 统一按 Windows 习惯排列，避免不同角色按钮分散两端 */
    button-layout: 0;
}
QDialogButtonBox QPushButton {
    min-width: 72px;
    padding: 4px 14px;
}
)QSS";

// 把矢量绘制的小图标落盘到临时目录，供 QSS 的 image: url() 引用
QString writeIcon(const QString& name, const QSize& size,
                  const std::function<void(QPainter&, const QSizeF&)>& draw)
{
    QPixmap pm(size * 2); // 2x 供高 DPI 缩放
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);
    draw(painter, QSizeF(size));
    painter.end();

    const QString path = QDir::temp().filePath("onmyoji_theme_" + name + ".png");
    pm.save(path, "PNG");
    return QDir::fromNativeSeparators(path);
}

QString makeArrowIcon(bool up, const QColor& color)
{
    return writeIcon(up ? "arrow_up" : "arrow_down", QSize(10, 6),
                     [up, color](QPainter& p, const QSizeF& s) {
        QPolygonF triangle;
        if (up) {
            triangle << QPointF(s.width() / 2, 0.5)
                     << QPointF(s.width() - 0.5, s.height() - 0.5)
                     << QPointF(0.5, s.height() - 0.5);
        } else {
            triangle << QPointF(0.5, 0.5)
                     << QPointF(s.width() - 0.5, 0.5)
                     << QPointF(s.width() / 2, s.height() - 0.5);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPolygon(triangle);
    });
}

QString makeCheckIcon(const QColor& color)
{
    return writeIcon("check", QSize(12, 12), [color](QPainter& p, const QSizeF& s) {
        QPen pen(color, 2.0);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        QPainterPath path;
        path.moveTo(s.width() * 0.22, s.height() * 0.55);
        path.lineTo(s.width() * 0.42, s.height() * 0.75);
        path.lineTo(s.width() * 0.80, s.height() * 0.28);
        p.drawPath(path);
    });
}
} // namespace

void apply(QApplication& app)
{
    // Fusion 在各 Windows 版本上渲染一致，QSS 覆盖更可控
    app.setStyle(QStyleFactory::create("Fusion"));

    // 锁定浅色调色板：系统深色模式下 Fusion 会用白色 ButtonText 画
    // SpinBox/下拉箭头，落在白底控件上就“消失”了
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0xf4, 0xf5, 0xf7));
    palette.setColor(QPalette::WindowText, QColor(0x2b, 0x2d, 0x33));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor(0xf7, 0xf8, 0xfb));
    palette.setColor(QPalette::Text, QColor(0x2b, 0x2d, 0x33));
    palette.setColor(QPalette::Button, QColor(0xe6, 0xe8, 0xef));
    palette.setColor(QPalette::ButtonText, QColor(0x2b, 0x2d, 0x33));
    palette.setColor(QPalette::Highlight, QColor(0x5b, 0x6c, 0xf0));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, QColor(0x2b, 0x2d, 0x33));
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(0x9a, 0x9d, 0xaa));
    app.setPalette(palette);

    QFont font("Microsoft YaHei UI");
    font.setPointSize(10);
    app.setFont(font);

    const QColor arrowColor(0x4a, 0x4d, 0x57);
    QString qss = QString::fromUtf8(kStyleSheet);
    qss.replace("%ARROW_UP%", makeArrowIcon(true, arrowColor));
    qss.replace("%ARROW_DOWN%", makeArrowIcon(false, arrowColor));
    qss.replace("%CHECK_MARK%", makeCheckIcon(Qt::white));
    app.setStyleSheet(qss);
}

} // namespace theme
