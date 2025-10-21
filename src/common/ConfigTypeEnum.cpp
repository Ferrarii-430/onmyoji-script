//
// Created by CZY on 2025/9/25.
//

#include "ConfigTypeEnum.h"

#include <qstring.h>

QString getConfigTypeEnumToQStringName(ConfigTypeEnum type) {
    switch (type) {
        case ConfigTypeEnum::OPENCV: return "OpenCV识图";
        case ConfigTypeEnum::WAIT: return "等待";
        case ConfigTypeEnum::OCR: return "OCR识别";
        default: return "未知";
    }
}

QString getConfigTypeEnumToQStringName(QString type) {
    ConfigTypeEnum config = stringToConfigType(type);
    return getConfigTypeEnumToQStringName(config);
}

// 字符串转枚举
ConfigTypeEnum stringToConfigType(const QString& typeStr) {
    if (typeStr.compare("OPENCV", Qt::CaseInsensitive) == 0) {
        return ConfigTypeEnum::OPENCV;
    }
    if (typeStr.compare("WAIT", Qt::CaseInsensitive) == 0) {
        return ConfigTypeEnum::WAIT;
    }
    if (typeStr.compare("OCR", Qt::CaseInsensitive) == 0) {
        return ConfigTypeEnum::OCR;
    }
    return ConfigTypeEnum::UNKNOWN;
}

// 枚举转字符串（可选，方便调试或保存）
QString configTypeToQString(ConfigTypeEnum type) {
    switch (type) {
        case ConfigTypeEnum::OPENCV: return "OPENCV";
        case ConfigTypeEnum::WAIT: return "WAIT";
        case ConfigTypeEnum::OCR: return "OCR";
        default: return "UNKNOWN";
    }
}