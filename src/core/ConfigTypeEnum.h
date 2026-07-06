//
// Created by CZY on 2025/9/25.
//

#ifndef CONFIGTYPEENUM_H
#define CONFIGTYPEENUM_H
#include <qstring.h>

enum class ConfigTypeEnum {
    OPENCV,
    WAIT,
    OCR,
    YOLO,
    SYSTEM_BORDER_BREAKTHROUGH,
    UNKNOWN
};

// 声明函数
QString getConfigTypeEnumToQStringName(ConfigTypeEnum type);
QString getConfigTypeEnumToQStringName(QString type);
ConfigTypeEnum stringToConfigType(const QString& typeStr);
QString configTypeToQString(ConfigTypeEnum type);

#endif //CONFIGTYPEENUM_H
