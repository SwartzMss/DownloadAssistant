QT += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++14

# MSVC specific settings
msvc {
    QMAKE_CXXFLAGS += /Zc:__cplusplus
    QMAKE_CXXFLAGS += /std:c++17
    QMAKE_CXXFLAGS += /permissive-
}

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/downloadmanager.cpp \
    src/downloadtask.cpp \
    src/smbdownloader.cpp \
    src/smbworker.cpp \
    src/logger.cpp \
    src/remote_smbfile_dialog.cpp \
    src/tasktablewidget.cpp \
    src/smbutils.cpp

HEADERS += \
    src/mainwindow.h \
    src/downloadmanager.h \
    src/downloadtask.h \
    src/smbdownloader.h \
    src/smbworker.h \
    src/logger.h \
    src/remote_smbfile_dialog.h \
    src/tasktablewidget.h \
    src/smbutils.h

FORMS += \
    src/mainwindow.ui

RESOURCES += DownloadAssistant.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Windows specific settings
win32 {
    LIBS += -lws2_32 -liphlpapi
    DEFINES += WIN32_LEAN_AND_MEAN
    # libsmb2 is bundled in the repository under depend/libsmb2
    INCLUDEPATH += $$PWD/depend/libsmb2/include
    LIBS += -L$$PWD/depend/libsmb2/lib -lsmb2
}

unix {
    INCLUDEPATH += $$PWD/depend/libsmb2/include
    LIBS += -L$$PWD/depend/libsmb2/lib -lsmb2
}

# Release configuration
CONFIG(release, debug|release) {
    DESTDIR = release
} else {
    DESTDIR = debug
} 