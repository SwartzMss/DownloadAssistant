#include "logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

Logger* Logger::m_instance = nullptr;
QMutex Logger::m_mutex;

Logger::Logger(QObject *parent)
    : QObject(parent)
    , m_logLevel(Info)
{
    // 设置默认日志文件路径
    QString logPath = QCoreApplication::applicationDirPath() + "/logs";
    QDir().mkpath(logPath);
    
    QString logFile = logPath + "/downloadassistant.log";
    setLogFile(logFile);
}

Logger::~Logger()
{
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

Logger* Logger::instance()
{
    if (m_instance == nullptr) {
        QMutexLocker locker(&m_mutex);
        if (m_instance == nullptr) {
            m_instance = new Logger();
        }
    }
    return m_instance;
}

void Logger::setLogFile(const QString &filePath)
{
    QMutexLocker locker(&m_writeMutex);
    
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    
    m_logFile.setFileName(filePath);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_textStream.setDevice(&m_logFile);
        // Qt6 中不再需要设置编码，默认使用 UTF-8
    } else {
        qWarning() << "Failed to open log file:" << filePath;
    }
}

void Logger::setLogLevel(LogLevel level)
{
    m_logLevel = level;
}

void Logger::debug(const QString &message, const QString &file, int line)
{
    log(Debug, message, file, line);
}

void Logger::info(const QString &message, const QString &file, int line)
{
    log(Info, message, file, line);
}

void Logger::warning(const QString &message, const QString &file, int line)
{
    log(Warning, message, file, line);
}

void Logger::error(const QString &message, const QString &file, int line)
{
    log(Error, message, file, line);
}

void Logger::log(LogLevel level, const QString &message, const QString &file, int line)
{
    if (level < m_logLevel) {
        return;
    }
    
    QString formattedMessage = formatMessage(level, message, file, line);
    writeToFile(formattedMessage);
    
    // 同时输出到控制台
    qDebug().noquote() << formattedMessage;
}

QString Logger::levelToString(LogLevel level) const
{
    switch (level) {
    case Debug:
        return "DEBUG";
    case Info:
        return "INFO";
    case Warning:
        return "WARN";
    case Error:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

QString Logger::getCurrentThreadId() const
{
    return QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

QString Logger::formatMessage(LogLevel level, const QString &message, const QString &file, int line) const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString threadId = getCurrentThreadId();
    QString levelStr = levelToString(level);
    
    QString fileInfo;
    if (!file.isEmpty() && line > 0) {
        QFileInfo fileInfoObj(file);
        fileInfo = QString("[%1:%2]").arg(fileInfoObj.fileName()).arg(line);
    }
    
    return QString("[%1] [%2] [Thread-%3] %4 %5")
           .arg(timestamp)
           .arg(levelStr)
           .arg(threadId)
           .arg(fileInfo)
           .arg(message);
}

void Logger::writeToFile(const QString &message)
{
    QMutexLocker locker(&m_writeMutex);
    
    if (m_logFile.isOpen()) {
        m_textStream << message << '\n';
    }
} 