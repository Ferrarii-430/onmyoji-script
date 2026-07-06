#include "src/vision/OcrEngine.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>

#include "src/core/AppPaths.h"

namespace vision {

namespace {

QJsonObject parseOcrOutput(const QString& ocrOutput)
{
    QString jsonStr = ocrOutput;

    // 查找JSON开始位置
    int jsonStart = jsonStr.indexOf('{');
    if (jsonStart == -1) {
        qWarning() << "未找到JSON数据";
        return QJsonObject();
    }

    jsonStr = jsonStr.mid(jsonStart);
    jsonStr = jsonStr.trimmed();

    // 将 \xE8\x8C\xB6 这种十六进制编码的中文字符转换为 Unicode
    QString processedStr;
    for (int i = 0; i < jsonStr.length(); ++i) {
        if (jsonStr[i] == '\\' && i + 3 < jsonStr.length() && jsonStr[i+1] == 'x') {
            QString hexStr = jsonStr.mid(i+2, 2);
            bool ok;
            ushort unicodeChar = hexStr.toUShort(&ok, 16);
            if (ok) {
                processedStr += QChar(unicodeChar);
                i += 3; // 跳过 \xXX
            } else {
                processedStr += jsonStr[i];
            }
        } else {
            processedStr += jsonStr[i];
        }
    }

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(processedStr.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON解析错误:" << parseError.errorString();
        qWarning() << "错误位置:" << parseError.offset;
        qWarning() << "处理后的字符串:" << processedStr;
        return QJsonObject();
    }

    if (!jsonDoc.isObject()) {
        qWarning() << "解析结果不是JSON对象";
        return QJsonObject();
    }

    return jsonDoc.object();
}

} // namespace

QJsonObject runRapidOCR()
{
    QJsonObject result;

    QString DX11_CAPTURE_PATH = AppPaths::instance().dx11CapturePath();
    QString rapidOCRExe = AppPaths::instance().rapidOCRExePath();
    QString rapidOCRModelsPath = AppPaths::instance().rapidOCRModelsPath();
    QString rapidOCRDetPath = AppPaths::instance().rapidOCRDetPathV4();
    QString rapidOCRClsPath = AppPaths::instance().rapidOCRClsPathV4();
    QString rapidOCRRecPath = AppPaths::instance().rapidOCRRecPathV4();

    QDir captureDir = QFileInfo(DX11_CAPTURE_PATH).absoluteDir();
    if (!captureDir.exists()) {
        captureDir.mkpath(".");
    }

    if (!QFile::exists(rapidOCRExe)) {
        qWarning() << "rapidOCR-json.exe 不存在:" << rapidOCRExe;
        return result;
    }

    QProcess process;
    QStringList arguments;
    arguments << ("--image_path=" + DX11_CAPTURE_PATH);
    arguments << ("--models=" + rapidOCRModelsPath);
    arguments << ("--det=" + rapidOCRDetPath);
    arguments << ("--cls=" + rapidOCRClsPath);
    arguments << ("--rec=" + rapidOCRRecPath);

    qDebug() << "执行ocr识别命令:" << rapidOCRExe << arguments;

    process.start(rapidOCRExe, arguments);

    if (!process.waitForFinished(5000)) {
        qWarning() << "ocr识别命令执行超时";
        process.kill();
        return result;
    }

    int exitCode = process.exitCode();
    QByteArray output = process.readAllStandardOutput();
    QByteArray errorOutput = process.readAllStandardError();

    if (exitCode != 0) {
        qWarning() << "ocr识别命令执行失败，退出码:" << exitCode;
        qWarning() << "错误输出:" << errorOutput;
        return result;
    }

    return parseOcrOutput(output);
}

} // namespace vision
