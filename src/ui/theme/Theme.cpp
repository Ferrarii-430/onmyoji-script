#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QStyleFactory>

namespace theme {

namespace {
// 配色：石板底 + 靛蓝主色，启动/停止按钮用语义色
const char* kStyleSheet = R"QSS(
/* ---------- 基础 ---------- */
QWidget {
    background-color: #f4f5f7;
    color: #2b2d33;
    font-size: 12px;
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
    padding: 4px 12px;
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
    background: transparent;
    padding: 2px;
}
QToolButton#settingButton:hover { background-color: #e3e5ec; }

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
QDialogButtonBox QPushButton {
    min-width: 64px;
}
)QSS";
} // namespace

void apply(QApplication& app)
{
    // Fusion 在各 Windows 版本上渲染一致，QSS 覆盖更可控
    app.setStyle(QStyleFactory::create("Fusion"));

    QFont font("Microsoft YaHei UI");
    font.setPointSize(9);
    app.setFont(font);

    app.setStyleSheet(kStyleSheet);
}

} // namespace theme
