//
// Created by CZY on 2025/9/25.
//

#include "ConfigTypeEnum.h"

#include <qstring.h>

QString getTemplatePath(ConfigTypeEnum type) {
    switch (type) {
    case ConfigTypeEnum::OPENCV: return "识图";
    default: return "未知";
    }
}

// 字符串转枚举
ConfigTypeEnum stringToConfigType(const QString& typeStr) {
    if (typeStr.compare("OPENCV", Qt::CaseInsensitive) == 0) {
        return ConfigTypeEnum::OPENCV;
    }
    if (typeStr.compare("WAIT", Qt::CaseInsensitive) == 0) {
        return ConfigTypeEnum::WAIT;
    }
    return ConfigTypeEnum::UNKNOWN;
}

// 枚举转字符串（可选，方便调试或保存）
QString configTypeToString(ConfigTypeEnum type) {
    switch (type) {
    case ConfigTypeEnum::OPENCV: return "OPENCV";
    default: return "UNKNOWN";
    }
}