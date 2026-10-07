#ifndef INPUTKEYREADER_H
#define INPUTKEYREADER_H
#include <QObject>
#include <QString>
#include <linux/input.h>

class QSocketNotifier;
class QTimer;

class InputKeyReader : public QObject
{
    Q_OBJECT
public:
    explicit InputKeyReader(QObject *parent = nullptr);
    ~InputKeyReader() override;
    void start();

signals:
    void cameraRequested();
    void connectionChanged(bool connected);

private:
    friend class IntegrationTests;
    void tryOpen();
    void readEvents();
    void handleEvent(const input_event &event);
    bool sampleKeyState();
    void disconnectDevice();
    int fd = -1;
    QSocketNotifier *notifier = nullptr;
    QTimer *retryTimer;
    bool pressed = false;
    bool syncing = false;
};
#endif
