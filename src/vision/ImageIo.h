//
// Created by TRAE on 2026/08/09.
//

#ifndef IMAGEIO_H
#define IMAGEIO_H

#include <vector>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <opencv2/imgcodecs.hpp>

namespace vision {

namespace {

inline bool isAsciiPath(const QString& path)
{
    for (const QChar& c : path) {
        if (c.unicode() > 127) {
            return false;
        }
    }
    return true;
}

} // namespace

// 读写图像，兼容中文路径。
// 1. 若路径全为 ASCII，优先使用 cv::imread/cv::imwrite（静态链接 OpenCV 下编码器最稳定）。
// 2. 若路径包含中文，回退到 Qt 文件 I/O + OpenCV 内存编解码，避免 MinGW/OpenCV
//    把 UTF-8 路径当作 ANSI 导致找不到文件的问题。

inline cv::Mat imreadQt(const QString& path, int flags = cv::IMREAD_COLOR)
{
    try {
        if (isAsciiPath(path)) {
            const cv::Mat img = cv::imread(path.toLocal8Bit().constData(), flags);
            if (!img.empty()) {
                return img;
            }
        }
    } catch (const cv::Exception&) {
        // 静态链接 OpenCV 下编码器初始化失败时回退到 Qt I/O
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return cv::Mat();
    }

    const QByteArray data = file.readAll();
    file.close();
    if (data.isEmpty()) {
        return cv::Mat();
    }

    try {
        std::vector<uchar> buffer(data.cbegin(), data.cend());
        return cv::imdecode(buffer, flags);
    } catch (const cv::Exception&) {
        return cv::Mat();
    }
}

inline bool imwriteQt(const QString& path, const cv::Mat& img,
                      const std::vector<int>& params = std::vector<int>())
{
    if (img.empty()) {
        return false;
    }

    try {
        if (isAsciiPath(path)) {
            const bool ok = params.empty()
                            ? cv::imwrite(path.toLocal8Bit().constData(), img)
                            : cv::imwrite(path.toLocal8Bit().constData(), img, params);
            if (ok) {
                return true;
            }
        }
    } catch (const cv::Exception&) {
        // 继续尝试 Qt I/O 回退
    }

    QString ext = QFileInfo(path).suffix().toLower();
    if (ext.isEmpty()) {
        ext = QStringLiteral("png");
    }
    const QByteArray format = (ext.startsWith('.') ? ext : ('.' + ext)).toUtf8();

    try {
        std::vector<uchar> buffer;
        const bool encoded = params.empty()
                             ? cv::imencode(format.constData(), img, buffer)
                             : cv::imencode(format.constData(), img, buffer, params);
        if (!encoded || buffer.empty()) {
            return false;
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }

        const qint64 written = file.write(reinterpret_cast<const char*>(buffer.data()),
                                          static_cast<qint64>(buffer.size()));
        file.close();
        return written == static_cast<qint64>(buffer.size());
    } catch (const cv::Exception&) {
        return false;
    }
}

} // namespace vision

#endif // IMAGEIO_H
