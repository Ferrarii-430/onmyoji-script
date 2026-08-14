#include "src/engine/TaskRunner.h"

#include <functional>

#include <QJsonObject>
#include <QMap>
#include <QRandomGenerator>

#include "src/core/AppPaths.h"
#include "src/core/ConfigTypeEnum.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/engine/ScriptActions.h"
#include "src/engine/scenarios/Arena.h"
#include "src/engine/scenarios/BorderBreakthrough.h"
#include "src/engine/scenarios/Budokai.h"
#include "src/engine/scenarios/Mitama.h"
#include "src/game/GameWindow.h"
#include "src/game/capture/CaptureService.h"

TaskRunner& TaskRunner::instance()
{
    static TaskRunner instance;
    return instance;
}

void TaskRunner::stop()
{
    m_isRunning = false;
    Logger::log(QString("正在停止任务..."));
}

QString TaskRunner::executeStep(const QJsonObject& step)
{
    ScriptActions& actions = ScriptActions::instance();
    QString typeStr = step["type"].toString();
    ConfigTypeEnum type = stringToConfigType(typeStr);
    QString savePath;

    // 识别类步骤：直接执行一次，不做重试。
    auto recognizeWithRetry = [this](const std::function<QString()>& recognize) -> QString {
        QString path = recognize();
        if (!path.isNull()) {
            emit showImage(path);
            return path;
        }
        return QString();
    };

    switch (type) {
        case ConfigTypeEnum::OPENCV: {
                Logger::log(QString("开始进行OpenCV识图"));
                QString imagePath = step["imagePath"].toString();
                const double score = step["score"].toDouble();
                const bool randomClick = step["randomClick"].toBool();
                const bool colorCheck = step["colorCheck"].toBool(false);
                savePath = recognizeWithRetry([&]() {
                    return actions.opencvRecognizesAndClick(imagePath, score, randomClick, colorCheck);
                });
                break;
        }

        case ConfigTypeEnum::OCR: {
                Logger::log(QString("开始进行OCR识图"));
                QString ocrText = step["ocrText"].toString();
                const double score = step["score"].toDouble();
                const bool randomClick = step["randomClick"].toBool();
                const QRectF roiPercent(step["ocrRoiX"].toDouble(), step["ocrRoiY"].toDouble(),
                                        step["ocrRoiW"].toDouble(), step["ocrRoiH"].toDouble());
                // 识别增强开关位掩码；旧配置无该字段时按「仅放大」处理，保持原有行为
                const auto enhance = static_cast<ocr::Enhance>(
                    step["ocrEnhance"].toInt(static_cast<int>(ocr::Enhance::Upscale)));
                savePath = recognizeWithRetry([&]() {
                    return actions.ocrRecognizesAndClick(ocrText, score, randomClick, roiPercent, enhance);
                });
                break;
        }

        case ConfigTypeEnum::YOLO: {
                Logger::log(QString("开始进行YOLO识图"));
                const double score = step["score"].toDouble();
                const bool randomClick = step["randomClick"].toBool();
                // 目标标签来自步骤配置；旧配置没有 yoloLabel 时沿用原先的默认标签
                QString yoloLabel = step["yoloLabel"].toString();
                if (yoloLabel.isEmpty()) {
                    yoloLabel = "common-btn-yellow_confirm";
                }
                Logger::log(QString("YOLO目标标签: %1").arg(yoloLabel));
                savePath = recognizeWithRetry([&]() {
                    return actions.yoloRecognizesAndClick(score, randomClick, yoloLabel, 0.0, 0.0);
                });
                break;
        }

        case ConfigTypeEnum::WAIT: {
                int waitTime = step["time"].toInt();
                bool randomWait = step["randomWait"].toBool();
                int offsetTime = step["offsetTime"].toInt();

                int actualWaitTime = waitTime;
                if (randomWait && offsetTime > 0) {
                    // 生成在 [-offsetTime, offsetTime] 范围内的随机偏移量
                    int randomOffset = QRandomGenerator::global()->bounded(-offsetTime, offsetTime + 1);
                    actualWaitTime = qMax(0, waitTime + randomOffset); // 确保等待时间非负
                }

                if (randomWait) {
                    Logger::log(QString("等待%1毫秒（随机偏移，基础时间%2毫秒）...").arg(actualWaitTime).arg(waitTime));
                } else {
                    Logger::log(QString("等待%1毫秒...").arg(actualWaitTime));
                }

                core::waitWithEventProcessing(actualWaitTime, [this]() { return m_isRunning; });
                break;
        }

        case ConfigTypeEnum::SYSTEM_BORDER_BREAKTHROUGH: {
                scenarios::executeBorderBreakthrough();
                break;
        }

        case ConfigTypeEnum::SYSTEM_ARENA: {
                scenarios::executeArena();
                break;
        }

        case ConfigTypeEnum::SYSTEM_MITAMA: {
                scenarios::executeMitama();
                break;
        }

        case ConfigTypeEnum::SYSTEM_BUDOKAI: {
                scenarios::executeBudokai();
                break;
        }

        default: {
                Logger::log(QString("未知的命令：%1").arg(typeStr));
                break;
        }
    }

    return savePath;
}

void TaskRunner::run(const QJsonArray& steps, int cycleCount)
{
    if (m_isRunning) {
        Logger::log(QString("任务已在运行中"));
        return;
    }

    if (!GameWindow::instance().locate())
    {
        Logger::log(QString("窗口未找到"));
        return;
    }

    if (steps.isEmpty())
    {
        Logger::log(QString("当前方案的内容为空，任务停止"));
        return;
    }

    if (!m_isInitLogPath)
    {
        if (capture::setDllLogPath())
        {
            Logger::log(QString("已修改dll日志路径: ") + AppPaths::instance().dx11LogPath());
        }else
        {
            Logger::log(QString("dll日志路径修改失败: ") + AppPaths::instance().dx11LogPath());
        }
        m_isInitLogPath = true;
    }

    m_isRunning = true;
    emit started();

    int number = cycleCount;
    int total = number;
    const bool infiniteLoop = (number <= 0);
    Logger::log(QString("任务循环次数: %1").arg(infiniteLoop ? "无限" : QString::number(number)));

    do {
        // 用于跟踪每个步骤的错误重试次数
        QMap<int, int> errorRetryMap;
        bool stopDoLoop = false; // 控制是否停止外部循环

        for (int i = 0; i < steps.size() && m_isRunning && !stopDoLoop; ++i)
        {
            QJsonObject step = steps[i].toObject();
            QString typeStr = step["type"].toString();
            ConfigTypeEnum type = stringToConfigType(typeStr);

            QString savePath = executeStep(step);

            //识别错误处理
            if (savePath.isEmpty() && type != ConfigTypeEnum::WAIT)
            {
                QString identifyErrorHandle = step["identifyErrorHandle"].toString();
                if (identifyErrorHandle == "next") {
                    //什么都不用做继续执行
                    Logger::log(QString("识别失败，继续执行下一个步骤"));
                } else if (identifyErrorHandle == "jump") {
                    //跳转到指定stepsId对应的步骤
                    if (step.contains("jumpStepsId") && !step["jumpStepsId"].toString().isEmpty()) {
                        QString jumpStepsId = step["jumpStepsId"].toString();
                        int targetIndex = -1;

                        // 遍历所有步骤，查找匹配的stepsId
                        for (int j = 0; j < steps.size(); ++j) {
                            QJsonObject currentStep = steps[j].toObject();
                            if (currentStep["stepsId"].toString() == jumpStepsId) {
                                targetIndex = j;
                                break;
                            }
                        }

                        if (targetIndex != -1) {
                            i = targetIndex - 1; // 设置i为targetIndex-1，因为循环会i++
                            Logger::log(QString("识别失败，跳转到步骤ID '%1' (索引 %2)").arg(jumpStepsId).arg(targetIndex));
                        } else {
                            Logger::log(QString("未找到步骤ID '%1'，使用默认next处理").arg(jumpStepsId));
                            // 默认为next
                        }
                    } else {
                        Logger::log(QString("跳转步骤ID未设置，使用默认next处理"));
                        // 默认为next
                    }
                } else if (identifyErrorHandle == "continue") {
                    //跳过for循环
                    Logger::log(QString("识别失败，跳过当前任务迭代的剩余步骤"));
                    break; // 跳出内部for循环，继续外部循环的下一个迭代
                } else if (identifyErrorHandle == "break") {
                    //直接停止do循环
                    Logger::log(QString("识别失败，停止整个任务循环"));
                    stopDoLoop = true;
                    break; // 跳出内部for循环
                } else if (identifyErrorHandle == "retry") {
                    //重新执行一次当前步骤
                    int &retryCount = errorRetryMap[i]; // 获取当前步骤的错误重试次数
                    if (retryCount < 999) { // 最大重试999次
                        retryCount++;
                        i = i - 1; // 重试当前步骤
                        Logger::log(QString("识别失败，第(%1/999)次重试当前步骤").arg(retryCount));
                        continue; // 跳过剩余代码，直接下一次迭代（重试）
                    } else {
                        Logger::log(QString("识别失败，重试次数用尽，继续下一个步骤"));
                        // 默认为next
                    }
                } else if (identifyErrorHandle == "end") {
                    //直接停止do循环
                    Logger::log(QString("结束任务"));
                    stopDoLoop = true;
                    break; // 跳出内部for循环
                } else {
                    Logger::log(QString("未知的错误处理选项: %1，使用默认next处理").arg(identifyErrorHandle));
                    // 默认为next
                }
            }
        }

        if (stopDoLoop) {
            break;
        }

        if (!infiniteLoop) {
            number--;
            Logger::log(QString("当前任务执行次数:(%1/%2)").arg(QString::number(total-number), QString::number(total)));
        }

        //每次任务结束都固定休眠1秒，防止无限循环一直执行
        core::waitWithEventProcessing(1000, [this]() { return m_isRunning; });

    } while (m_isRunning && (infiniteLoop || number > 0));

    // 若运行的是武道大会方案，循环结束后重置状态
    for (const QJsonValue& stepVal : steps) {
        if (stringToConfigType(stepVal.toObject().value("type").toString()) == ConfigTypeEnum::SYSTEM_BUDOKAI) {
            scenarios::resetBudokaiStatus();
            break;
        }
    }

    m_isRunning = false;
    emit finished();

    Logger::log(QString("任务循环结束"));
}
