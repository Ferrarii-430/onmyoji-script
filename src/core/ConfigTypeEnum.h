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
    CLICK_ROI,
    // 注意：系统方案（SYSTEM_*）不在此枚举中，由场景注册表
    // src/engine/scenarios/ScenarioRegistry.h 按步骤 type 字符串直接调度
    UNKNOWN
};

// 声明函数
QString getConfigTypeEnumToQStringName(ConfigTypeEnum type);
QString getConfigTypeEnumToQStringName(QString type);
ConfigTypeEnum stringToConfigType(const QString& typeStr);
QString configTypeToQString(ConfigTypeEnum type);

#endif //CONFIGTYPEENUM_H
