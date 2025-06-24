#include "mainwindow.h"

#include <QApplication>
#include "logger.h"
#include <QSharedMemory>
#include <QMessageBox>
#include <winsock2.h>

int main(int argc, char *argv[])
{
    WSADATA wsaData;
    int wsaInit = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaInit != 0) {
        QMessageBox::critical(nullptr, "Error",
                              QString("WSAStartup failed: %1").arg(wsaInit));
        return 1;
    }
    QApplication a(argc, argv);
    
    QSharedMemory sharedMemory("DownloadAssistantSingleInstanceKey");
    if (!sharedMemory.create(1)) {
        QMessageBox::warning(nullptr, "提示", "程序已在运行，不允许多开！");
        return 0;
    }
    
    // 初始化日志系统
    Logger::instance()->info("应用程序启动");
    
    
    // 设置应用程序信息
    QApplication::setApplicationName("DownloadAssistant");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("DownloadAssistant");
    
    // 设置应用程序图标和样式
    QApplication::setStyle("Fusion");
    
    MainWindow w;
    w.show();
    
    Logger::instance()->info("主窗口已显示");

    int ret = a.exec();

    WSACleanup();

    return ret;
}
