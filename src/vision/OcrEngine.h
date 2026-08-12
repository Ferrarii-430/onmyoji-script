#ifndef OCRENGINE_H
#define OCRENGINE_H

#include <QJsonObject>
#include <QString>

namespace vision {

// 对指定图片执行 RapidOCR，返回解析后的 JSON 结果；
// imagePath 为空时默认使用最近一次截图（dx11CapturePath）
// padding 为预处理白边宽度，裁剪图可增大以优化边缘文字识别率（默认 50）
QJsonObject runRapidOCR(const QString& imagePath = QString(), int padding = 50);

} // namespace vision

#endif // OCRENGINE_H
