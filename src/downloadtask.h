#ifndef DOWNLOADTASK_H
#define DOWNLOADTASK_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QDateTime>

class DownloadTask : public QObject
{
    Q_OBJECT

public:
    enum Status {
        Pending,        // 等待中
        Queued,         // 排队中
        Downloading,    // 下载中
        Paused,         // 暂停
        Completed,      // 完成
        Failed,         // 失败
        Cancelled       // 取消
    };
    Q_ENUM(Status)

    explicit DownloadTask(QObject *parent = nullptr);
    explicit DownloadTask(const QString &url, const QString &savePath, QObject *parent = nullptr);
    ~DownloadTask();
    
    // 基本属性
    QString id() const { return m_id; }
    void setId(const QString &id) { m_id = id; }
    
    QString url() const { return m_url; }
    void setUrl(const QString &url);
    
    QString savePath() const { return m_savePath; }
    void setSavePath(const QString &path);
    
    QString fileName() const { return m_fileName; }
    void setFileName(const QString &name);
    
    // 状态和进度
    Status status() const { return m_status; }
    void setStatus(Status status);
    
    double progress() const { return m_progress; }
    void setProgress(double progress);
    
    qint64 downloadedSize() const { return m_downloadedSize; }
    void setDownloadedSize(qint64 size);
    
    qint64 totalSize() const { return m_totalSize; }
    void setTotalSize(qint64 size);
    
    qint64 speed() const { return m_speed; }
    void setSpeed(qint64 speed);
    
    QString errorMessage() const { return m_errorMessage; }
    void setErrorMessage(const QString &message);
    
    bool supportsResume() const { return m_supportsResume; }
    void setSupportsResume(bool supports) { m_supportsResume = supports; }
    
    // 时间信息
    QDateTime endTime() const { return m_endTime; }
    void setEndTime(const QDateTime &time) { m_endTime = time; }
    
    // 文本表示
    QString statusText() const;
    QString speedText() const;
    QString timeRemainingText() const;
    qint64 fileSize() const { return m_totalSize; }

signals:
    void statusChanged(Status status);
    void progressChanged(double progress);
    void speedChanged(qint64 speed);
    void errorOccurred(const QString &error);

private:
    void generateId();
    
    QString m_id;
    QString m_url;
    QString m_savePath;
    QString m_fileName;
    Status m_status;
    double m_progress;
    qint64 m_downloadedSize;
    qint64 m_totalSize;
    qint64 m_speed;
    QString m_errorMessage;
    bool m_supportsResume;
    QDateTime m_endTime;
};

#endif // DOWNLOADTASK_H 