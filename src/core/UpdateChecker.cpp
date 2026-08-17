#include "src/core/UpdateChecker.h"

#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

#include "src/core/AppVersion.h"
#include "src/core/Logger.h"

namespace {

// Gitee API v5：获取仓库最新发行版（公开仓库无需鉴权，受 IP 级速率限制）。
// 响应字段（节选）：
//   {
//     "tag_name": "yys-script-v1.5.4-release",
//     "name": "...",
//     "body": "<发行说明 markdown>",
//     "assets": [
//       { "browser_download_url": "https://gitee.com/.../<file>" },
//       ...
//     ]
//   }
constexpr const char* kGiteeApiUrl =
    "https://gitee.com/api/v5/repos/clannad_cai/onmyoji-script/releases/latest";

// release 详情页（无附件时回退使用），形如 .../releases/<raw_tag>
constexpr const char* kGiteeReleasesBase =
    "https://gitee.com/clannad_cai/onmyoji-script/releases";

// HTTP 请求超时：覆盖低帧率/弱网场景，避免卡死 UI 线程的等待
constexpr int kRequestTimeoutMs = 15000;

// 从任意 tag 字符串中提取纯版本号（major.minor[.patch[...]]）。
// 支持 "v1.5.5"、"yys-script-v1.5.4-release"、"1.5.4-beta" 等格式。
// 无法匹配时返回原字符串（fallback，保证不漏报新版本）。
QString extractCleanVersion(const QString& tag)
{
    static const QRegularExpression reVer(QStringLiteral("(\\d+(?:\\.\\d+)+)"));
    const auto m = reVer.match(tag);
    return m.hasMatch() ? m.captured(1) : tag;
}

} // namespace

UpdateChecker::UpdateChecker() : QObject(nullptr)
{
    // QNetworkAccessManager 必须在主线程（有事件循环的线程）构造。
    // 通过 this 作为 parent，跟随单例生命周期自动释放。
    m_nam = new QNetworkAccessManager(this);
}

UpdateChecker::~UpdateChecker() = default;

UpdateChecker& UpdateChecker::instance()
{
    // 局部静态变量在首次调用时构造，线程安全的初始化（C++11 magic statics）。
    static UpdateChecker inst;
    return inst;
}

void UpdateChecker::checkAsync(bool notifyOnNoUpdate)
{
    Logger::log(QString("[更新检查] 正在向 Gitee 查询最新发行版..."));

    QNetworkRequest req((QUrl(QString::fromLatin1(kGiteeApiUrl))));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("yys-script-update-checker"));
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(kRequestTimeoutMs);

    QNetworkReply* reply = m_nam->get(req);
    // reply 由 m_nam 父子关系管理，无需手动 delete；这里只需在 finished 时处理并 deleteLater。
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, notifyOnNoUpdate]() {
                onReplyFinished(reply, notifyOnNoUpdate);
            });
}

void UpdateChecker::onReplyFinished(QNetworkReply* reply, bool notifyOnNoUpdate)
{
    reply->deleteLater();

    // 网络错误：超时、DNS 解析失败、HTTP 非 2xx 等
    if (reply->error() != QNetworkReply::NoError) {
        // 404 通常意味着该仓库尚未发布任何 release
        const QString err = (reply->error() == QNetworkReply::ContentNotFoundError)
            ? QStringLiteral("Gitee 上尚未发布任何发行版")
            : reply->errorString();
        Logger::log(QString("[更新检查] 网络请求失败: %1").arg(err));
        emit checkFailed(err);
        if (notifyOnNoUpdate) {
            QMessageBox::warning(nullptr, QStringLiteral("检查更新"),
                QStringLiteral("检查更新失败：\n%1").arg(err));
        }
        return;
    }

    const QByteArray data = reply->readAll();
    QString tag, downloadUrl, notes;
    if (!parseLatestRelease(data, tag, downloadUrl, notes)) {
        Logger::log(QString("[更新检查] 解析 Gitee 响应失败"));
        emit checkFailed(QStringLiteral("解析 Gitee 响应失败"));
        if (notifyOnNoUpdate) {
            QMessageBox::warning(nullptr, QStringLiteral("检查更新"),
                QStringLiteral("解析 Gitee 响应失败，请稍后重试。"));
        }
        return;
    }

    const int cmp = compareVersions(tag, QString::fromLatin1(APP_VERSION));
    if (cmp <= 0) {
        // 远端 tag <= 本地版本：已是最新
        Logger::log(QString("[更新检查] 当前版本 v%1 已是最新（远端最新: %2）")
                    .arg(QString::fromLatin1(APP_VERSION)).arg(tag));
        emit noUpdateAvailable();
        if (notifyOnNoUpdate) {
            QMessageBox::information(nullptr, QStringLiteral("检查更新"),
                QStringLiteral("当前已是最新版本 (v%1)。").arg(QString::fromLatin1(APP_VERSION)));
        }
        return;
    }

    // 发现新版本
    Logger::log(QString("[更新检查] 发现新版本 %1（当前 v%2）")
                .arg(tag).arg(QString::fromLatin1(APP_VERSION)));
    emit updateAvailable(tag, downloadUrl, notes);

    // 默认弹窗询问是否前往下载；Detailed Text 中显示 release notes。
    QMessageBox box(QMessageBox::Information,
        QStringLiteral("发现新版本"),
        QStringLiteral("发现新版本 <b>%1</b>（当前 v%2）<br><br>是否立即前往 Gitee 下载？")
            .arg(tag).arg(QString::fromLatin1(APP_VERSION)),
        QMessageBox::Yes | QMessageBox::No);
    box.setDetailedText(notes);
    if (box.exec() == QMessageBox::Yes) {
        QDesktopServices::openUrl(QUrl(downloadUrl));
    }
}

bool UpdateChecker::parseLatestRelease(const QByteArray& data,
                                       QString& outTag,
                                       QString& outUrl,
                                       QString& outNotes)
{
    QJsonParseError parseErr{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        Logger::log(QString("[更新检查] JSON 解析失败: %1")
                    .arg(parseErr.error != QJsonParseError::NoError
                         ? parseErr.errorString()
                         : QStringLiteral("响应非 JSON 对象")));
        return false;
    }
    const QJsonObject obj = doc.object();

    const QString rawTag = obj.value("tag_name").toString().trimmed();
    if (rawTag.isEmpty()) {
        Logger::log(QString("[更新检查] 响应缺少 tag_name 字段"));
        return false;
    }
    outNotes = obj.value("body").toString().trimmed();

    // outTag 为清洗后的纯版本号（"yys-script-v1.5.4-release" -> "1.5.4"），
    // 用于显示与比较；URL fallback 必须使用 rawTag 才能 Gitee 路由正确。
    outTag = extractCleanVersion(rawTag);

    // 优先取第一个 asset 的浏览器下载地址；无附件时回退到 release 详情页 URL
    const QJsonArray assets = obj.value("assets").toArray();
    if (!assets.isEmpty()) {
        outUrl = assets.at(0).toObject().value("browser_download_url").toString();
    }
    if (outUrl.isEmpty()) {
        outUrl = QString::fromLatin1(kGiteeReleasesBase) + QStringLiteral("/") + rawTag;
    }
    return true;
}

int UpdateChecker::compareVersions(QString a, QString b)
{
    // 输入已通过 extractCleanVersion 清洗过（"1.5.4" 形式），
    // 这里再做一次清洗以防调用方直接传入原始 tag。
    a = extractCleanVersion(a.trimmed());
    b = extractCleanVersion(b.trimmed());

    const QStringList pa = a.split('.', Qt::SkipEmptyParts);
    const QStringList pb = b.split('.', Qt::SkipEmptyParts);
    const int n = qMax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        // 输入已清洗为纯数字段，toInt 失败时按 0 处理（防御性，避免抛异常）
        const int na = i < pa.size() ? pa[i].toInt() : 0;
        const int nb = i < pb.size() ? pb[i].toInt() : 0;
        if (na != nb) {
            return na > nb ? 1 : -1;
        }
    }
    return 0;
}
