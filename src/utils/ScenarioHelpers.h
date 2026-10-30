//
// Created by CZY on 2026/10/20.
//

#ifndef SCENARIO_HELPERS_H
#define SCENARIO_HELPERS_H

#include <QJsonArray>
#include <QStringList>
#include <opencv2/core/types.hpp>

namespace utils {

// 系统方案公共工具：OCR 识别结果 → 点击坐标。
// 供各 scenarios/*.cpp 共用（Budokai / SixGates 等），避免重复实现。

// 从 OCR box（4 个角点）计算外接矩形，坐标已在 DX11 捕获坐标系。
// box 不足 4 个点或数据异常时返回空矩形
cv::Rect boxToRect(const QJsonArray& box);

// 计算点击点：randomClick 为 true 时在矩形内随机取点（拟人），否则取矩形中心
cv::Point makeClickPoint(const cv::Rect& rect, bool randomClick);

// 提取 OCR 识别结果（[{box,score,text}]）中的文本，忽略 box/score 这类每次识别都会波动的字段；
// 去除首尾空白并丢弃空文本后按文本排序，便于做稳定比较
QStringList ocrTexts(const QJsonArray& ocrData);

// OCR 文本变化跟踪器：忽略 box/score 与识别顺序，仅依据 text 判断相邻两次识别结果是否一致。
// 用于「滑动后列表内容未变化 → 已到列表顶部/底部」的判断，每个滚动循环各持有一个实例
class OcrTextTracker
{
public:
    // 记录本次识别结果，并返回其文本内容是否与上一次一致；
    // 首次调用或本次未识别出文本时返回 false（识别失败不算“内容未变化”）
    bool unchangedSinceLast(const QJsonArray& ocrData);

private:
    QStringList lastTexts_;
};

} // namespace utils

#endif // SCENARIO_HELPERS_H
