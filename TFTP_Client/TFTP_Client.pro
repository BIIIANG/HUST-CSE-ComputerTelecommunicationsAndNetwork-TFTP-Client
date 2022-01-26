QT       += core gui
RC_ICONS = icon.ico

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets \
    network \
    core5compat

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    util.cpp \
    workthread.cpp

HEADERS += \
    def.h \
    mainwindow.h \
    util.h \
    workthread.h

FORMS += \
    mainwindow.ui

# 添加库文件
LIBS += -L"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\um\x64" \
        -lws2_32

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    images/images.qrc

DISTFILES += \
    images/icon.png
