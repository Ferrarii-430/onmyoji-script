//
// Created by CZY on 2025/9/25.
//

#ifndef CONFIGTYPEENUM_H
#define CONFIGTYPEENUM_H
#include <qstring.h>

enum class ConfigTypeEnum {
    OPENCV,
    WAIT,
    UNKNOWN
};

// 声明函数
QString getTemplatePath(ConfigTypeEnum type);
ConfigTypeEnum stringToConfigType(const QString& typeStr);
QString configTypeToString(ConfigTypeEnum type);

#endif //CONFIGTYPEENUM_H
