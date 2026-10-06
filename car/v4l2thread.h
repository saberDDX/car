#ifndef V4L2THREAD_H
#define V4L2THREAD_H

#include <QThread>
#include <QImage>
#include <vector>
#include <linux/videodev2.h>

// 之前定义的缓冲区结构体
struct Buffer {
    void* start;
    size_t length;
};//只是定义了一个数据类型，并没有分配任何实际内存来装数据

class V4L2Thread : public QThread {//我要造一个叫 V4L2Thread 的新零件。这个零件本质上是一个 QThread（线程底盘），我要让它在后台默默运行；但它不仅仅是一个空转的线程，我要在它的内部重写一段专属逻辑，让这个线程一启动，就死死盯住 /dev/video0 给我抓取摄像头数据！
    Q_OBJECT
public:
    explicit V4L2Thread(QObject *parent = nullptr);
    ~V4L2Thread() override;

    // 留给主界面调用的业务接口
    void startCapture(); // 挂入倒挡时调用
    void stopCapture();  // 退出倒挡时调用

signals:
    // 核心信号：抓到一帧画面并转码后，通过它把数据抛给 UI 层
    void frameReady(const QImage &image); 

protected:
    // QThread 的核心，子线程真正在跑的死循环代码
    void run() override; 

private:
    int fd;
    bool isRunning;               // 控制线程循环的开关
    std::vector<Buffer> buffers;  // 显存映射池 创建一个名为buffers的数组，专门用来存放Buffer

    // 封装底层 ioctl 调用的私有方法
    bool initCamera();
    void releaseCamera();
};

#endif // V4L2THREAD_H