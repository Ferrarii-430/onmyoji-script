//
// Created by CZY on 2025/9/25.
//

#include "src/core/ConfigTypeEnum.h"

#include <qstring.h>

QString getConfigTypeEnumToQStringName(ConfigTypeEnum type) {
    switch (type) {
        case ConfigTypeEnum::OPENCV: return "OpenCV识图";
        case ConfigTypeEnum::WAIT: return "等待";
        case ConfigTypeEnum::OCR: return "OCR识别";
        case ConfigTypeEnum::YOLO: return "YOLO";
        case ConfigTypeEnum::SYSTEM_BORDER_BREAKTHROUGH: return "SYSTEM_BORDER_BREAKTHROUGH";
        case ConfigTypeEnum::SYSTEM_ARENA: return "SYSTEM_ARENA";
        case ConfigTypeEnum::SYSTEM_MITAMA: return "SYSTEM_MITAMA";
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
    if (typeStr.compare("YOLO", Qt::CaseInsensitive) == 0) {
        return ConfigTypeEnum::YOLO;
    }
    if (typeStr.compare("SYSTEM_BORDER_BREAKTHROUGH", Qt::CaseInsensitive) == 0) {
        return ConfigTypeEnum::SYSTEM_BORDER_BREAKTHROUGH;
    }
    if (typeStr.compare("SYSTEM_ARENA", Qt::CaseInsensitive) == 0) {
        return ConfigTypeEnum::SYSTEM_ARENA;
    }
    if (typeStr.compare("SYSTEM_MITAMA", Qt::CaseInsensitive) == 0) {
        return ConfigTypeEnum::SYSTEM_MITAMA;
    }
    return ConfigTypeEnum::UNKNOWN;
}

// 枚举转字符串（可选，方便调试或保存）
QString configTypeToQString(ConfigTypeEnum type) {
    switch (type) {
        case ConfigTypeEnum::OPENCV: return "OPENCV";
        case ConfigTypeEnum::WAIT: return "WAIT";
        case ConfigTypeEnum::OCR: return "OCR";
        case ConfigTypeEnum::YOLO: return "YOLO";
        case ConfigTypeEnum::SYSTEM_BORDER_BREAKTHROUGH: return "SYSTEM_BORDER_BREAKTHROUGH";
        case ConfigTypeEnum::SYSTEM_ARENA: return "SYSTEM_ARENA";
        case ConfigTypeEnum::SYSTEM_MITAMA: return "SYSTEM_MITAMA";
        default: return "UNKNOWN";
    }
}