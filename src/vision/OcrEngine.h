#ifndef OCRENGINE_H
#define OCRENGINE_H

#include <QJsonObject>

namespace vision {

// 对最近一次截图（dx11CapturePath）执行 RapidOCR，返回解析后的 JSON 结果
QJsonObject runRapidOCR();

} // namespace vision

#endif // OCRENGINE_H
