#-------------------------------------------------
#
# Project created by QtCreator 2021-07-16T17:13:44
#
#-------------------------------------------------
CONFIG += c++11
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
    camera.cpp \
    inputkeyreader.cpp

HEADERS  += mainwindow.h \
    weather.h \
    map.h \
    music.h \
    video.h \
    v4l2thread.h \
    camera.h \
    inputkeyreader.h

FORMS    += mainwindow.ui \
    weather.ui \
    map.ui \
    music.ui \
    video.ui

RESOURCES += \
    img.qrc

# 引入 OpenCV 库 
isEmpty(OPENCV_INCLUDE_DIR): OPENCV_INCLUDE_DIR = /usr/include/opencv4
INCLUDEPATH += $$OPENCV_INCLUDE_DIR
LIBS += -lopencv_core -lopencv_imgproc
