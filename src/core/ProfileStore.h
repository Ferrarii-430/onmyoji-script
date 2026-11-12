//
// Created by CZY on 2025/9/30.
//

#ifndef COMMON_H
#define COMMON_H
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

struct ItemInfo {
    QString taskName;
    QString id;
};

// 按线程隔离：多标签页并发运行时，每个会话各自选中不同的方案。
// 若共用一份全局变量，B 标签页切换方案会把 A 正在执行的任务的配置改掉，
// 表现为「跑着跑着换成了别人的方案参数」。
// 任务线程由 TaskRunner 在启动时写入自己的副本（见 TaskRunner::start）。
//
// 用 extern 声明 + 单点定义，而不是 inline 变量：
// MinGW 下 inline thread_local 的 TLS 初始化函数会在每个引用它的编译单元
// 各生成一份，链接期直接报 multiple definition。
extern thread_local QJsonArray m_configArray;
extern thread_local ItemInfo currentItem;
QString getPathByRecognitionImg(const std::string &configId, const std::string &fileName);
void addConfigToJsonFile(const QString &filePath, const QString &name);
bool removeConfigById(const QString &filePath, const QString &idToRemove);
void addConfigToJsonFile(const QString &filePath, const QString &configId, const QJsonObject& json);
void saveBase64ImageToFile(QJsonObject &data);
QString cleanString(const QString &s);
QJsonValue safeValue(const QJsonObject &obj, const QString &key);
bool removeConfigById(const QString &filePath, const QString &configId, const QString &stepsId);
void updateConfigInJsonFile(const QString &filePath, const QString &configId, const QJsonObject& json);
void updateProgrammeContent(const QString &filePath, const QString &configId, const QString &name);
bool moveProgramme(const QString &filePath, const QString &configId, int stepsIndex, bool status);
void refreshConfig();
QJsonArray getConfigJSON();
QJsonArray getLastConfigJSON();
void commonSetCurrentItem(const QString &id, const QString &taskName);
QMap<QString,QString> getStepsSelect(const QString &configId, const QString &currentStepsId);

// ------------------------------
// 系统方案自定义配置 systemConfig 的读写
// 每个系统方案(config.json 里 type=="system")可带一个 systemConfig 数组，
// 数组每项既描述控件(control/label/min/max/...)又存当前值(value)，key 为唯一标识。
// 每个方案对象还可带顶层 tip 字段，用于在 UI 上显示该方案的功能使用说明；
// systemConfig 数组每项也可带 tip 字段，用于解释单个配置项。
// ------------------------------
// 读取某方案完整的 systemConfig 数组(含控件描述)，无则返回空数组，用于 UI 渲染表单
QJsonArray getSystemConfig(const QString &configId);
// 读取某方案顶层 tip 字段(方案功能使用说明)，无则返回空字符串
QString getSystemTip(const QString &configId);
// 按 key 读取某方案的配置值(只取 value)，找不到时返回 defaultValue，用于场景运行时取值
QJsonValue getSystemConfigValue(const QString &configId, const QString &key,
                                const QJsonValue &defaultValue = QJsonValue());
// 更新某方案某 key 的配置值并写回文件(只改 value，不动控件描述)
void updateSystemConfigValue(const QString &filePath, const QString &configId,
                             const QString &key, const QJsonValue &value);

// ------------------------------
// 超鬼王专属：鬼王列表配置(bossConfig)
// 只属于「系统方案-超鬼王」，数组顺序即界面顺序，每项一条记录：
//   { "bossName": "鬼王名称", "lineupName": "阵容名称",
//     "greenMark": 0, "markBoss": false }
// greenMark 为绿标位置 0-5（0 表示不标记），markBoss 为是否红标 BOSS。
// 存在方案对象顶层，随 config.json 一起持久化，也会被配置导出/导入带上；
// 方案对象上的 showBossConfig 控制主界面是否显示「自定义配置」入口。
// ------------------------------
QJsonArray getBossConfig(const QString &configId);
void updateBossConfig(const QString &filePath, const QString &configId, const QJsonArray &list);
// 按鬼王名称取一条记录，找不到返回空对象（供场景运行时查表）
QJsonObject findBossConfig(const QString &configId, const QString &bossName);

// ------------------------------
// 配置导出 / 导入（带版本校验）
// ------------------------------
// 当前配置格式版本：config.json 本体保持裸数组格式不变，版本号仅写入导出文件。
// 新增配置字段时递增此值；导入低版本文件时缺失字段由各读取处的默认值兜底
// （如 TaskRunner 的 step["excludeLeft"].toDouble(0.0)），只降级不崩溃。
// v2：系统方案新增顶层 bossConfig（超鬼王鬼王列表）。
constexpr int kConfigVersion = 2;

// 导出当前全部方案到 targetFilePath（带 app/appVersion/configVersion/exportedAt/programs 包装）。
// 成功返回空字符串，失败返回错误信息（用于 UI 弹窗提示）
QString exportConfigToFile(const QString &targetFilePath);

// 从 sourceFilePath 导入方案并整体替换当前 config.json（导入前自动备份原文件为
// config.json.bak）。兼容导出包装格式与裸数组（旧版配置原文）两种文件。
// 版本校验：文件版本高于程序支持时拒绝导入；成功后 m_configArray 已刷新，
// 失败返回错误信息且原配置不受影响
QString importConfigFromFile(const QString &sourceFilePath);
#endif //COMMON_H
