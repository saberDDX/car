#include "v4l2thread.h"
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <opencv2/opencv.hpp>

V4L2Thread::V4L2Thread(QObject *parent) //在 C++ 面向对象的语法里，只要一个函数的名字和类名完全一模一样，而且没有返回值（连 void 都不写），它就不是普通函数，而是构造函数。
    : QThread(parent), fd(-1), isRunning(false) {
}

V4L2Thread::~V4L2Thread() {//析构函数，会被自动执行
    stopCapture();
}

void V4L2Thread::startCapture() {
    if (!isRunning) {
        isRunning = true;
        start(); // 这会触发 Qt 底层开启新线程，并自动执行 run()
    }
}

void V4L2Thread::stopCapture() {
    if (isRunning) {
        isRunning = false;
        wait(); // 阻塞等待后台线程安全退出，防止内存崩溃
    }
}

void V4L2Thread::run() {
    if (!initCamera()) {
        qDebug() << "摄像头初始化失败！";
        return;
    }

    qDebug() << "进入摄像头后台抓图死循环...";
    while (isRunning) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        // 1. 取出一帧 (DQBUF)
        if (ioctl(fd, VIDIOC_DQBUF, &buf) >= 0) {
            
            // --- 开始跨界转换 ---
    // A. 将内核映射的内存直接包装成 OpenCV 的 YUYV 矩阵 (零拷贝)
    cv::Mat yuvMat(480, 640, CV_8UC2, buffers[buf.index].start);
    cv::Mat rgbMat;
    
    // B. 极速转换为 RGB 格式
    cv::cvtColor(yuvMat, rgbMat, cv::COLOR_YUV2RGB_YUYV);
    
    // C. 将 OpenCV 矩阵包装成 Qt 认识的 QImage
    QImage img((const unsigned char*)(rgbMat.data), 
               rgbMat.cols, rgbMat.rows, rgbMat.step, 
               QImage::Format_RGB888);
               
    // D. 跨线程发送给前端 UI（注意：必须 copy 深拷贝，否则内存会被后续帧覆盖）
    emit frameReady(img.copy());
    // --- 转换结束 ---
            
            // 2. 将空盘子还给内核 (QBUF)
            ioctl(fd, VIDIOC_QBUF, &buf);
        }
    }

    releaseCamera();
    qDebug() << "安全退出摄像头抓图线程";
}

bool V4L2Thread::initCamera() {
    // 1. 打开设备节点
    fd = open("/dev/video0", O_RDWR);
    if (fd < 0) {
        qDebug() << "无法打开摄像头设备";
        return false;
    }

    // 2. 设置格式 (640x480 YUYV)
    struct v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));//初始化
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    ioctl(fd, VIDIOC_S_FMT, &fmt);//正式下单 VIDIOC_S_FMT：set fmt设置视频格式

    // 3. 申请 4 个内核缓冲区
    struct v4l2_requestbuffers req;
    std::memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ioctl(fd, VIDIOC_REQBUFS, &req);//正式下单 VIDIOC_REQBUFS：request buffers申请缓冲区

    // 4. 内存映射 (mmap) 与入队
    buffers.resize(req.count);//buffers是std::vector创建的动态数组，所以可以这么写  在数组buffers里面创建四个Buffer
    for (size_t i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        ioctl(fd, VIDIOC_QUERYBUF, &buf);
        
        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);//内存映射
        
        ioctl(fd, VIDIOC_QBUF, &buf);
    }

    // 5. 开启视频流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &type);
    
    qDebug() << "摄像头初始化完成，显存通道已打通！";

    return true;
}

void V4L2Thread::releaseCamera() {
    if (fd >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);

        for (size_t i = 0; i < buffers.size(); ++i) {
            munmap(buffers[i].start, buffers[i].length);
        }
        close(fd);
        fd = -1;
        qDebug() << "摄像头资源已安全释放";
    }
}