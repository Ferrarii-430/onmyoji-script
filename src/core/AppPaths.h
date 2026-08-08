//
// Created by CZY on 2025/10/15.
//

#ifndef APPPATHS_H
#define APPPATHS_H
#include <qcoreapplication.h>
#include <QString>
#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <vector>
#endif


class AppPaths {
public:
    static AppPaths& instance() {
        static AppPaths instance;
        return instance;
    }

    // 删除拷贝构造函数和赋值运算符
    AppPaths(const AppPaths&) = delete;
    AppPaths& operator=(const AppPaths&) = delete;

    // 获取程序目录的短路径（8.3 格式），避免中文路径在 OpenCV / 外部注入器 / OCR 工具中
    // 因 ANSI 编码导致无法读写文件的问题。若短路径获取失败则回退到原始路径。
    static QString applicationDirPath() {
        QString path = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
        return toShortPath(path);
#else
        return path;
#endif
    }

    // 路径获取方法
    QString dx11CapturePath() const {
        return applicationDirPath() + "/src/resource/thumbnail/debug_capture_result.png";
    }

    QString matchResultPath() const {
        return applicationDirPath() + "/src/resource/thumbnail/debug_match_result.png";
    }

    QString dx11LogPath() const {
        return applicationDirPath() + "/src/resource/log/dx11_log.txt";
    }

    QString dx11HookDllPath() const {
        return applicationDirPath() + "/src/resource/hook/libdx11_hook.dll";
    }

    QString dx11HookDllName() const {
        return "libdx11_hook.dll";
    }

    QString remoteCaptureExePath() const {
        return applicationDirPath() + "/remote_capture_call.exe";
    }

    QString screenshotPath() const {
        return applicationDirPath() + "/src/resource/screenshot/";
    }

    QString thumbnailPath() const {
        return applicationDirPath() + "/src/resource/thumbnail/";
    }

    QString configPath() const {
        return applicationDirPath() + "/src/resource/config.json";
    }

    QString rapidOCRExePath() const {
        return applicationDirPath() + "/src/resource/RapidOCR/RapidOCR-json.exe";
    }

    QString classesNamePath() const {
        return applicationDirPath() + "/src/resource/classes.txt";
    }

    QString labelCatalogPath() const {
        return applicationDirPath() + "/src/resource/yolo_label_catalog.json";
    }

    QString onmyojiYoloOnnxPath() const {
        return applicationDirPath() + "/src/resource/onmyoji-yolo-v5.onnx";
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
        return applicationDirPath() + "/src/resource/RapidOCR/models/";
    }

private:
    AppPaths() = default;
    ~AppPaths() = default;

#ifdef Q_OS_WIN
    static QString toShortPath(const QString& path) {
        const wchar_t* wPath = reinterpret_cast<const wchar_t*>(path.utf16());
        const DWORD len = GetShortPathNameW(wPath, nullptr, 0);
        if (len == 0) {
            return path;
        }
        std::vector<wchar_t> buffer(len);
        if (GetShortPathNameW(wPath, buffer.data(), len) == 0) {
            return path;
        }
        return QString::fromWCharArray(buffer.data());
    }
#endif
};



#endif //APPPATHS_H
