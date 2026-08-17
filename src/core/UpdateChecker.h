//
// Gitee 发行版更新检查器。
//
// 通过 Gitee API v5 查询仓库最新 release，与本地 APP_VERSION 比对：
//   GET https://gitee.com/api/v5/repos/{owner}/{repo}/releases/latest
// 响应中 tag_name 形如 "v1.5.5"，本端去除前导 'v' 后按数字段逐段比较。
//
// 使用方式：
//   - 启动时静默检查：UpdateChecker::instance().checkAsync(false)
//     （仅在发现新版本时弹窗，失败/已是最新仅写日志）
//   - 设置页手动检查：UpdateChecker::instance().checkAsync(true)
//     （无论结果均弹窗提示用户）
//
// 注意：QNetworkAccessManager 是异步的，调用方线程必须有 Qt 事件循环。
// UpdateChecker 是单例，首次 instance() 时构造，其生命周期与进程相同。
//
#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    static UpdateChecker& instance();

    UpdateChecker(const UpdateChecker&) = delete;
    UpdateChecker& operator=(const UpdateChecker&) = delete;

    // 异步触发一次更新检查。
    // notifyOnNoUpdate=true 时（手动检查）：失败/无新版本也弹窗；
    // notifyOnNoUpdate=false 时（启动静默检查）：仅在发现新版本时弹窗，其它情况只写日志。
    void checkAsync(bool notifyOnNoUpdate = false);

signals:
    // 发现新版本：latestVersion 为已剥离前导 'v' 的版本号（如 "1.5.5"）；
    // downloadUrl 为 release 详情页 URL（无附件时回退到仓库 releases 列表页）；
    // releaseNotes 为 release body 内容（可能为空）。
    void updateAvailable(const QString& latestVersion,
                         const QString& downloadUrl,
                         const QString& releaseNotes);

    // 无新版本（仅在有响应且本地版本 >= 远端最新时发出）
    void noUpdateAvailable();

    // 检查失败（网络错误、API 限流、JSON 解析失败等）
    void checkFailed(const QString& errorMsg);

private:
    UpdateChecker();
    ~UpdateChecker() override;

    void onReplyFinished(QNetworkReply* reply, bool notifyOnNoUpdate);

    // 解析 Gitee API 响应，提取 tag_name / body / 浏览器下载地址。
    bool parseLatestRelease(const QByteArray& data,
                            QString& outTag,
                            QString& outUrl,
                            QString& outNotes);

    // 比较两个版本字符串（支持 "v1.5.5" / "1.5.5" / "1.5.5-beta" 等形式）。
    // 按数字段逐段比较，非数字段视为 0；忽略段后的非数字后缀。
    // 返回: 1 = a 更新, 0 = 相等, -1 = a 更旧。
    static int compareVersions(QString a, QString b);

    QNetworkAccessManager* m_nam = nullptr;
};

#endif // UPDATECHECKER_H
