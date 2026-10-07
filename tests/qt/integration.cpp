#include <QtTest>
#include <QDir>
#include <QPushButton>
#include <QSignalSpy>
#include "camera.h"
#include "mainwindow.h"
#include "inputkeyreader.h"
#include "v4l2thread.h"

class IntegrationTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        // No camera or network service is needed. An explicit invalid video node
        // exercises real open/ioctl failure and cleanup without touching hardware.
        qputenv("CAR_CAMERA_DEVICE", "/dev/null");
    }

    void onlyNewPressOpensCamera() {
        InputKeyReader reader;
        QSignalSpy requests(&reader, &InputKeyReader::cameraRequested);
        input_event event = {};
        event.type = EV_KEY;
        event.code = KEY_CAMERA;
        event.value = 1;
        reader.handleEvent(event);
        reader.handleEvent(event); // Duplicated press must not open a second window.
        event.value = 2;
        reader.handleEvent(event); // Long-press repeat must not open a window.
        event.code = KEY_ENTER;
        event.value = 1;
        reader.handleEvent(event);
        QCOMPARE(requests.count(), 1);
        event.code = KEY_CAMERA;
        event.value = 0;
        reader.handleEvent(event);
        QCOMPARE(requests.count(), 1);
        event.value = 1;
        reader.handleEvent(event);
        QCOMPARE(requests.count(), 2);
    }

    void queueLossSuppressesActions() {
        InputKeyReader reader;
        QSignalSpy requests(&reader, &InputKeyReader::cameraRequested);
        input_event event = {};
        event.type = EV_SYN;
        event.code = SYN_DROPPED;
        reader.handleEvent(event);
        event.type = EV_KEY;
        event.code = KEY_CAMERA;
        event.value = 1;
        reader.handleEvent(event);
        QCOMPARE(requests.count(), 0);
        QVERIFY(reader.syncing);
    }

    void cameraIsSingleAndBackRestoresMainWindow() {
        MainWindow window;
        window.show();
        InputKeyReader *reader = window.findChild<InputKeyReader *>();
        QVERIFY(reader);
        for (int cycle = 0; cycle < 3; ++cycle) {
            QVERIFY(QMetaObject::invokeMethod(reader, "cameraRequested", Qt::DirectConnection));
            QCOMPARE(window.findChildren<Camera *>().size(), 1);
            Camera *camera = window.findChild<Camera *>();
            QVERIFY(camera);
            QVERIFY(!window.isVisible());
            QVERIFY(QMetaObject::invokeMethod(reader, "cameraRequested", Qt::DirectConnection));
            QCOMPARE(window.findChildren<Camera *>().size(), 1);
            QPushButton *back = camera->findChild<QPushButton *>("cameraBackButton");
            QVERIFY(back);
            QTest::mouseClick(back, Qt::LeftButton);
            QTRY_COMPARE(window.findChildren<Camera *>().size(), 0);
            QVERIFY(window.isVisible());
        }
    }

    void failedInitializationClosesDeviceAndCanRestart() {
        V4L2Thread worker;
        QSignalSpy errors(&worker, &V4L2Thread::captureError);
        const int before = QDir("/proc/self/fd").entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size();
        for (int cycle = 0; cycle < 5; ++cycle) {
            worker.startCapture();
            QTRY_COMPARE(errors.count(), cycle + 1);
            worker.stopCapture();
            QVERIFY(!worker.isRunning());
        }
        QCOMPARE(QDir("/proc/self/fd").entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size(), before);
    }
};

QTEST_MAIN(IntegrationTests)
#include "integration.moc"
