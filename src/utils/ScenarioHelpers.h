//
// Created by CZY on 2026/10/20.
//

#ifndef SCENARIO_HELPERS_H
#define SCENARIO_HELPERS_H

#include <QJsonArray>
#include <opencv2/core/types.hpp>

namespace utils {

// 系统方案公共工具：OCR 识别结果 → 点击坐标。
// 供各 scenarios/*.cpp 共用（Budokai / SixGates 等），避免重复实现。

// 从 OCR box（4 个角点）计算外接矩形，坐标已在 DX11 捕获坐标系。
// box 不足 4 个点或数据异常时返回空矩形
cv::Rect boxToRect(const QJsonArray& box);

// 计算点击点：randomClick 为 true 时在矩形内随机取点（拟人），否则取矩形中心
cv::Point makeClickPoint(const cv::Rect& rect, bool randomClick);

} // namespace utils

#endif // SCENARIO_HELPERS_H
