#include "src/vision/OcrEngine.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QProcessEnvironment>

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

QJsonObject runRapidOCR(const QString& imagePath, int padding)
{
    QJsonObject result;

    QString DX11_CAPTURE_PATH = imagePath.isEmpty() ? AppPaths::instance().dx11CapturePath() : imagePath;
    QString rapidOCRExe = AppPaths::instance().rapidOCRExePath();
    QString rapidOCRModelsPath = AppPaths::instance().rapidOCRModelsPath();
    QString rapidOCRDetPath = AppPaths::instance().rapidOCRDetPathV4();
    QString rapidOCRClsPath = AppPaths::instance().rapidOCRClsPathV4();
    QString rapidOCRRecPath = AppPaths::instance().rapidOCRRecPathV4();
    QString rapidOCRKeysPath = AppPaths::instance().rapidOCRKeysPath();

    QDir captureDir = QFileInfo(DX11_CAPTURE_PATH).absoluteDir();
    if (!captureDir.exists()) {
        captureDir.mkpath(".");
    }

    if (!QFile::exists(rapidOCRExe)) {
        qWarning() << "rapidOCR-json.exe 不存在:" << rapidOCRExe;
        return result;
    }

    QProcess process;
    process.setWorkingDirectory(QFileInfo(rapidOCRExe).absolutePath());

    // 让 RapidOCR-json.exe 能找到主程序目录下的 onnxruntime.dll 等依赖
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString currentPath = env.value("PATH");
    env.insert("PATH", QCoreApplication::applicationDirPath() + ";" + currentPath);
    process.setProcessEnvironment(env);

    QStringList arguments;
    arguments << ("--image_path=" + DX11_CAPTURE_PATH);
    arguments << ("--models=" + rapidOCRModelsPath);
    arguments << ("--det=" + rapidOCRDetPath);
    arguments << ("--cls=" + rapidOCRClsPath);
    arguments << ("--rec=" + rapidOCRRecPath);
    arguments << ("--keys=" + rapidOCRKeysPath);
    // 启用 ASCII 转义输出，避免中文在进程输出中因编码问题损坏
    arguments << "--ensureAscii=1";
    // 预处理白边，可优化窄边/裁剪图边缘文字的识别率
    arguments << ("--padding=" + QString::number(padding));
    // 0 表示不限制长边缩小；裁剪图本身不大，避免被错误缩小导致文字像素丢失
    arguments << "--maxSideLen=0";
    // 文字框置信度门限，适当默认值兼顾召回与精度
    arguments << "--boxScoreThresh=0.5";
    arguments << "--boxThresh=0.3";
    // 单个文字框大小倍率，略放大以包容文字笔画
    arguments << "--unClipRatio=1.6";
    // 启用方向检测与角度投票，适应倾斜文字
    arguments << "--doAngle=1";
    arguments << "--mostAngle=1";

    qDebug() << "执行ocr识别命令:" << rapidOCRExe << arguments;
    qDebug() << "ocr工作目录:" << QFileInfo(rapidOCRExe).absolutePath();

    process.start(rapidOCRExe, arguments);

    if (!process.waitForFinished(5000)) {
        qWarning() << "ocr识别命令执行超时";
        process.kill();
        return result;
    }

    int exitCode = process.exitCode();
    QByteArray output = process.readAllStandardOutput();
    QByteArray errorOutput = process.readAllStandardError();

    qDebug() << "ocr退出码:" << exitCode;
    qDebug() << "ocr标准输出:" << output;
    if (!errorOutput.isEmpty()) {
        qDebug() << "ocr错误输出:" << errorOutput;
    }

    if (exitCode != 0) {
        qWarning() << "ocr识别命令执行失败，退出码:" << exitCode;
        qWarning() << "错误输出:" << errorOutput;
        return result;
    }

    return parseOcrOutput(output);
}

} // namespace vision
