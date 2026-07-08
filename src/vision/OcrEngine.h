#ifndef OCRENGINE_H
#define OCRENGINE_H

#include <QJsonObject>
#include <QString>

namespace vision {

// 对指定图片执行 RapidOCR，返回解析后的 JSON 结果；
// imagePath 为空时默认使用最近一次截图（dx11CapturePath）
QJsonObject runRapidOCR(const QString& imagePath = QString());

} // namespace vision

#endif // OCRENGINE_H
