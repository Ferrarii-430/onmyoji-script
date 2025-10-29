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

    QString matchResultPath() const {
        return QCoreApplication::applicationDirPath() + "/src/resource/thumbnail/debug_match_result.png";
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

    QString rapidOCRExePath() const {
        return QCoreApplication::applicationDirPath() + "/src/resource/RapidOCR/RapidOCR-json.exe";
    }

    QString classesNamePath() const {
        return QCoreApplication::applicationDirPath() + "/src/resource/classes.txt";
    }

    QString onmyojiYoloOnnxPath() const {
        return QCoreApplication::applicationDirPath() + "/src/resource/onmyoji-yolo-v5.onnx";
    }

    QString rapidOCRDetPathV4() const {
        return "ch_PP-OCRv4_det_infer.onnx";
    }

    QString rapidOCRClsPathV4() const {
        return "ch_ppocr_mobile_v2.0_cls_infer.onnx";
    }

    QString rapidOCRRecPathV4() const {
        return "rec_ch_PP-OCRv4_infer.onnx";
    }

    QString rapidOCRKeysPath() const {
        return "dict_chinese.txt";
    }

    QString rapidOCRModelsPath() const {
        return QCoreApplication::applicationDirPath() + "/src/resource/RapidOCR/models/";
    }

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
};



#endif //CONFIGMANAGER_H
