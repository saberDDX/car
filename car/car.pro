#-------------------------------------------------
#
# Project created by QtCreator 2021-07-16T17:13:44
#
#-------------------------------------------------
CONFIG += C++11
QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = car
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    weather.cpp \
    map.cpp \
    music.cpp \
    video.cpp \
    v4l2thread.cpp \
    camera.cpp 

HEADERS  += mainwindow.h \
    weather.h \
    map.h \
    music.h \
    video.h \
    v4l2thread.h \
    camera.h 

FORMS    += mainwindow.ui \
    weather.ui \
    map.ui \
    music.ui \
    video.ui

RESOURCES += \
    img.qrc

# 引入 OpenCV 库 
INCLUDEPATH += /usr/include/opencv4
LIBS += -lopencv_core -lopencv_imgproc

LIBS += -lzmq