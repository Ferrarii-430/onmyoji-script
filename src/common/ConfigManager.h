//
// Created by CZY on 2025/10/15.
//

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H
#include <qcoreapplication.h>
#include <QString>


class ConfigManager {
public:
    static ConfigManager& instance() {
        static ConfigManager instance;
        return instance;
    }

    // 删除拷贝构造函数和赋值运算符
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // 路径获取方法
    QString dx11CapturePath() const {
        return QCoreApplication::applicationDirPath() + "/src/resource/thumbnail/debug_capture_result.png";
    }

    QString dx11LogPath() const {
        return QCoreApplication::applicationDirPath() + "/src/resource/log/dx11_log.txt";
    }

    QString dx11HookDllPath() const {
        return QCoreApplication::applicationDirPath() + "/src/resource/hook/libdx11_hook.dll";
    }

    QString dx11HookDllName() const {
        return "libdx11_hook.dll";
    }

    QString remoteCaptureExePath() const {
        return QCoreApplication::applicationDirPath() + "/remote_capture_call.exe";
    }

    QString screenshotPath() const {
        return QCoreApplication::applicationDirPath() + "/src/resource/screenshot/";
    }

    QString thumbnailPath() const {
        return QCoreApplication::applicationDirPath() + "/src/resource/thumbnail/";
    }

    QString configPath() const {
        return QCoreApplication::applicationDirPath() + "/src/resource/config.json";
    }

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
};



#endif //CONFIGMANAGER_H
