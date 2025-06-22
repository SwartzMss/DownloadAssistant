#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include "logger.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 初始化日志系统
    Logger::instance()->info("应用程序启动");
    
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "DownloadAssistant_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            QApplication::installTranslator(&translator);
            break;
        }
    }
    
    // 设置应用程序信息
    QApplication::setApplicationName("DownloadAssistant");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("DownloadAssistant");
    
    // 设置应用程序图标和样式
    QApplication::setStyle("Fusion");
    
    MainWindow w;
    w.show();
    
    Logger::instance()->info("主窗口已显示");
    
    return a.exec();
} 