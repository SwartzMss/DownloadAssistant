#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QThread>
#include <QDateTime>

class Logger : public QObject
{
    Q_OBJECT

public:
    enum LogLevel {
        Debug,
        Info,
        Warning,
        Error
    };
    Q_ENUM(LogLevel)

    static Logger* instance();
    
    void setLogFile(const QString &filePath);
    void setLogLevel(LogLevel level);
    
    void debug(const QString &message, const QString &file = "", int line = 0);
    void info(const QString &message, const QString &file = "", int line = 0);
    void warning(const QString &message, const QString &file = "", int line = 0);
    void error(const QString &message, const QString &file = "", int line = 0);
    
    void log(LogLevel level, const QString &message, const QString &file = "", int line = 0);

private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();
    
    static Logger* m_instance;
    static QMutex m_mutex;
    
    QFile m_logFile;
    QTextStream m_textStream;
    LogLevel m_logLevel;
    QMutex m_writeMutex;
    
    QString levelToString(LogLevel level) const;
    QString getCurrentThreadId() const;
    QString formatMessage(LogLevel level, const QString &message, const QString &file, int line) const;
    void writeToFile(const QString &message);
};

// 便捷宏定义
#define LOG_DEBUG(msg) Logger::instance()->debug(msg, __FILE__, __LINE__)
#define LOG_INFO(msg) Logger::instance()->info(msg, __FILE__, __LINE__)
#define LOG_WARNING(msg) Logger::instance()->warning(msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::instance()->error(msg, __FILE__, __LINE__)

#endif // LOGGER_H 