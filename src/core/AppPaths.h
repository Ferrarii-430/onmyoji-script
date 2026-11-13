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

    // 注入器 / hook DLL 属于独立子模块，中转文件路径按 ANSI 窄字符处理：
    // 应用目录含中文时 DLL 侧打不开（日志、持久化截图会静默失败），
    // 因此含非 ASCII 字符时统一回退到全 ASCII 的 ProgramData 目录。
    // 应用侧读写走同一方法，保证写入与回读的路径一致。
    QString dx11WorkDir() const {
        if (!isAsciiPath(applicationDirPath())) {
            return QStringLiteral("C:/ProgramData/yys-script/src/resource");
        }
        return applicationDirPath() + "/src/resource";
    }

    // 路径获取方法
    QString dx11CapturePath() const {
        return dx11WorkDir() + "/thumbnail/debug_capture_result.png";
    }

    // 按游戏实例区分的中转文件：多开时每个实例各写各的，避免互相覆写。
    // DLL 侧的截图名与日志名都由此传入，规则改这里即可。
    QString dx11CapturePathFor(DWORD pid) const {
        return dx11WorkDir()
               + QString("/thumbnail/debug_capture_%1.png").arg(pid);
    }

    QString dx11LogPathFor(DWORD pid) const {
        return dx11WorkDir()
               + QString("/log/dx11_log_%1.txt").arg(pid);
    }

    QString matchResultPath() const {
        return applicationDirPath() + "/src/resource/thumbnail/debug_match_result.png";
    }

    // 按游戏实例区分的识别结果图：多标签页并发运行时各写各的，
    // 否则 A 会话刚写完的调试图会被 B 会话覆盖，界面回显的可能是别人的画面。
    QString matchResultPathFor(DWORD pid) const {
        return applicationDirPath()
               + QString("/src/resource/thumbnail/debug_match_result_%1.png").arg(pid);
    }

    // 按游戏实例区分的原始截图落盘路径（persistScreenshot 开启时使用）
    QString debugCapturePathFor(DWORD pid) const {
        return applicationDirPath()
               + QString("/src/resource/thumbnail/debug_capture_%1.png").arg(pid);
    }

    QString dx11LogPath() const {
        return dx11WorkDir() + "/log/dx11_log.txt";
    }

    // 应用自身日志（Logger 输出），与 dx11 hook 日志同目录但独立轮转
    QString appLogPath() const {
        return applicationDirPath() + "/src/resource/log/app_log.txt";
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

    QString settingPath() const {
        return applicationDirPath() + "/src/resource/setting.json";
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

    QString rapidOCRDetPath() const {
        return "PP-OCRv6_small_det_onnx.onnx";
    }

    QString rapidOCRClsPath() const {
        return "ch_ppocr_mobile_v2.0_cls_infer.onnx";
    }

    QString rapidOCRRecPath() const {
        return "PP-OCRv6_small_rec_onnx.onnx";
    }

    QString rapidOCRKeysPath() const {
        return "ppocrv6_dict.txt";
    }

    QString rapidOCRModelsPath() const {
        return applicationDirPath() + "/src/resource/PaddleOCR/";
    }

private:
    AppPaths() = default;
    ~AppPaths() = default;

#ifdef Q_OS_WIN
    static bool isAsciiPath(const QString& path) {
        for (const QChar& c : path) {
            if (c.unicode() > 127) {
                return false;
            }
        }
        return true;
    }

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
