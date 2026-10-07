#include "mainwindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <QTextStream>
#include <cstring>
#include "v4l2thread.h"

int main(int argc, char *argv[])
{
    if (argc == 2 && std::strcmp(argv[1], "--find-camera") == 0) {
        QCoreApplication application(argc, argv);
        V4L2Thread probe;
        QObject::connect(&probe, &V4L2Thread::captureError, [](const QString &message) {
            qWarning().noquote() << message;
        });
        const QString path = probe.probeCameraDevice();
        if (path.isEmpty())
            return 1;
        QTextStream(stdout) << path << '\n';
        return 0;
    }
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
