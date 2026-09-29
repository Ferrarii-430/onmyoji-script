#ifndef SCENARIO_REGISTRY_H
#define SCENARIO_REGISTRY_H

#include <QString>
#include <functional>

namespace scenarios {

// 系统方案注册项。新增系统方案只需三步：
//   1. 编写方案的 executeXxx()（可选 resetXxx()），参考现有 scenarios/*.cpp
//   2. CMakeLists.txt 加入新文件
//   3. 方案 .cpp 末尾（namespace scenarios 内）调用 REGISTER_SCENARIO 宏注册
// TaskRunner 通过注册表按步骤 type 字符串调度，ConfigTypeEnum 不再包含系统方案，
// 新增/删除系统方案无需改动 TaskRunner / ConfigTypeEnum。
struct ScenarioEntry {
    QString type;                           // 步骤 type 字符串（如 "SYSTEM_MITAMA"），与 config.json 中一致
    std::function<bool()> execute;          // 执行一轮，true=正常完成继续循环
    std::function<void()> reset;            // 任务结束后的跨轮次状态重置，可为空
    bool enableCollaborationGuard = false;  // 执行期间开启协作弹窗守卫（识别失败自动扫协作弹窗并重试一次）
};

// 注册一个系统方案（由 REGISTER_SCENARIO 在静态初始化期自动调用）
void registerScenario(const ScenarioEntry& entry);

// 按步骤 type 查找注册的系统方案，未注册返回 nullptr
const ScenarioEntry* findScenario(const QString& type);

} // namespace scenarios

// 在方案 .cpp 末尾、namespace scenarios 内调用，例如：
//   REGISTER_SCENARIO("SYSTEM_MITAMA", executeMitama, resetMitamaStatus, true)
// 无跨轮次状态需要重置的方案第三参传 nullptr；第四参为是否开启协作弹窗守卫。
#define REGISTER_SCENARIO(TYPE, EXECUTE, RESET, GUARD)                               \
    namespace {                                                                      \
        [[maybe_unused]] const bool s_scenarioRegistered_##EXECUTE = [] {            \
            scenarios::registerScenario(                                             \
                scenarios::ScenarioEntry{QStringLiteral(TYPE), (EXECUTE), (RESET), (GUARD)}); \
            return true;                                                             \
        }();                                                                         \
    }

#endif // SCENARIO_REGISTRY_H
