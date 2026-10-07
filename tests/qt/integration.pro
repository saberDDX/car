QT += widgets network testlib
CONFIG += c++11 testcase
TEMPLATE = app
TARGET = qt-integration-tests
INCLUDEPATH += ../../car
SOURCES += integration.cpp \
    ../../car/mainwindow.cpp ../../car/camera.cpp ../../car/inputkeyreader.cpp \
    ../../car/v4l2thread.cpp ../../car/weather.cpp ../../car/map.cpp \
    ../../car/music.cpp ../../car/video.cpp
HEADERS += ../../car/mainwindow.h ../../car/camera.h ../../car/inputkeyreader.h \
    ../../car/v4l2thread.h ../../car/weather.h ../../car/map.h \
    ../../car/music.h ../../car/video.h
FORMS += ../../car/mainwindow.ui ../../car/weather.ui ../../car/map.ui \
    ../../car/music.ui ../../car/video.ui
RESOURCES += ../../car/img.qrc
isEmpty(OPENCV_INCLUDE_DIR): OPENCV_INCLUDE_DIR = /usr/include/opencv4
INCLUDEPATH += $$OPENCV_INCLUDE_DIR
LIBS += -lopencv_core -lopencv_imgproc
