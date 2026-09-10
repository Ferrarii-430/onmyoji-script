#ifndef OCRENGINE_H
#define OCRENGINE_H

#include <QJsonObject>
#include <QString>
#include <opencv2/core/mat.hpp>

namespace vision {

// 进程内 RapidOCR 推理（det/cls/rec 模型常驻单例，替代外部 RapidOCR-json.exe 进程调用）。
// image 为空 Mat 时返回空对象。
// 输出与原 exe 的 stdout JSON 同构：{"data":[{"box":[[x,y]x4],"score":float,"text":string}]}，
// box 坐标为输入图坐标系，消费方（ScriptActions）无需任何改动。
QJsonObject runRapidOCR(const cv::Mat& image, int padding = 50);

// 预加载 OCR 模型（幂等）。建议启动时调用避免首次识别卡顿；
// 未调用时 runRapidOCR 会自行惰性加载。
bool initInProcessOcr();

} // namespace vision

#endif //OCRENGINE_H
