#ifndef V4L2THREAD_H
#define V4L2THREAD_H
#include <QThread>
#include <QImage>
#include <QString>
#include <atomic>
#include <vector>

struct Buffer {
    void *start = nullptr;
    size_t length = 0;
};

class V4L2Thread : public QThread
{
    Q_OBJECT
public:
    explicit V4L2Thread(QObject *parent = nullptr);
    ~V4L2Thread() override;
    void startCapture();
    void stopCapture();
    QString probeCameraDevice(); // Read-only CLI discovery before granting device access.

signals:
    void frameReady(const QImage &image);
    void captureError(const QString &message);

protected:
    void run() override;

private:
    bool initCamera();
    bool openCamera();
    bool fail(const QString &operation);
    void releaseCamera();
    std::atomic_bool stopRequested{false};
    int fd = -1;
    bool streaming = false;
    unsigned int width = 0, height = 0, stride = 0;
    QString selectedDevice;
    std::vector<Buffer> buffers;
};
#endif
