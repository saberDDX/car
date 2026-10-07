#include "v4l2thread.h"
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace {
int cameraIoctl(int fd, unsigned long request, void *argument)
{
    int result;
    do { result = ioctl(fd, request, argument); } while (result < 0 && errno == EINTR);
    return result;
}
}

V4L2Thread::V4L2Thread(QObject *parent) : QThread(parent) {}
V4L2Thread::~V4L2Thread() { stopCapture(); }

void V4L2Thread::startCapture()
{
    if (isRunning())
        return;
    stopRequested.store(false);
    start();
}

void V4L2Thread::stopCapture()
{
    stopRequested.store(true);
    wait();
}

bool V4L2Thread::fail(const QString &operation)
{
    emit captureError(operation + ": " + QString::fromLocal8Bit(std::strerror(errno)));
    return false;
}

QString V4L2Thread::probeCameraDevice()
{
    if (!openCamera())
        return {};
    const QString path = selectedDevice;
    releaseCamera();
    return path;
}

bool V4L2Thread::openCamera()
{
    const QString explicitPath = qEnvironmentVariable("CAR_CAMERA_DEVICE");
    QStringList candidates;
    if (!explicitPath.isEmpty()) {
        candidates << explicitPath;
    } else {
        const QDir stable("/dev/v4l/by-id");
        for (const QString &entry : stable.entryList({"*-video-index0"}, QDir::Files | QDir::System))
            candidates << stable.absoluteFilePath(entry);
        for (const QString &entry : QDir("/dev").entryList({"video*"}, QDir::Files | QDir::System))
            candidates << "/dev/" + entry;
    }
    for (const QString &path : candidates) {
        const int candidate = ::open(QFile::encodeName(path).constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (candidate < 0) {
            if (!explicitPath.isEmpty())
                return fail("无法打开摄像头 " + path);
            continue;
        }
        v4l2_capability caps = {};
        if (cameraIoctl(candidate, VIDIOC_QUERYCAP, &caps) < 0) {
            const int saved = errno;
            ::close(candidate);
            if (!explicitPath.isEmpty()) { errno = saved; return fail("无法查询摄像头能力"); }
            continue;
        }
        const unsigned int capabilities = caps.capabilities & V4L2_CAP_DEVICE_CAPS ? caps.device_caps : caps.capabilities;
        const bool uvc = std::strncmp(reinterpret_cast<const char *>(caps.driver), "uvcvideo", sizeof(caps.driver)) == 0;
        bool yuyv = false;
        v4l2_fmtdesc format = {};
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        while (cameraIoctl(candidate, VIDIOC_ENUM_FMT, &format) == 0) {
            if (format.pixelformat == V4L2_PIX_FMT_YUYV)
                yuyv = true;
            ++format.index;
        }
        if ((explicitPath.isEmpty() && !uvc) || !(capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
            !(capabilities & V4L2_CAP_STREAMING) || !yuyv) {
            ::close(candidate);
            if (!explicitPath.isEmpty()) {
                emit captureError("此摄像头不支持当前 YUYV 流式采集，请检查设备和格式。");
                return false;
            }
            continue;
        }
        fd = candidate;
        selectedDevice = path;
        qInfo() << "V4L2 camera selected:" << path;
        return true;
    }
    emit captureError("未找到支持 YUYV 的 USB 摄像头，请检查连接、权限和设备格式。");
    return false;
}

bool V4L2Thread::initCamera()
{
    if (!openCamera())
        return false;
    v4l2_format format = {};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = 640;
    format.fmt.pix.height = 480;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.field = V4L2_FIELD_ANY;
    if (cameraIoctl(fd, VIDIOC_S_FMT, &format) < 0)
        return fail("设置摄像头格式失败");
    width = format.fmt.pix.width;
    height = format.fmt.pix.height;
    stride = format.fmt.pix.bytesperline;
    if (format.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV || !width || !height ||
        width > 4096 || height > 4096 || width % 2 || stride < width * 2 || stride > INT_MAX) {
        emit captureError("摄像头返回了不支持的图像格式或尺寸。");
        return false;
    }
    v4l2_requestbuffers request = {};
    request.count = 4;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;
    if (cameraIoctl(fd, VIDIOC_REQBUFS, &request) < 0)
        return fail("申请摄像头缓冲区失败");
    if (!request.count || request.count > 32) {
        emit captureError("摄像头返回的缓冲区数量无效。");
        return false;
    }
    buffers.resize(request.count);
    for (size_t i = 0; i < buffers.size(); ++i) {
        v4l2_buffer buffer = {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        if (cameraIoctl(fd, VIDIOC_QUERYBUF, &buffer) < 0)
            return fail("查询摄像头缓冲区失败");
        void *mapping = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer.m.offset);
        if (mapping == MAP_FAILED)
            return fail("映射摄像头缓冲区失败");
        buffers[i].start = mapping;
        buffers[i].length = buffer.length;
        if (cameraIoctl(fd, VIDIOC_QBUF, &buffer) < 0)
            return fail("摄像头缓冲区入队失败");
    }
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (cameraIoctl(fd, VIDIOC_STREAMON, &type) < 0)
        return fail("启动摄像头失败");
    streaming = true;
    qInfo() << "V4L2 streaming:" << width << "x" << height << "stride" << stride;
    return true;
}

void V4L2Thread::run()
{
    if (!initCamera()) {
        releaseCamera();
        return;
    }
    QElapsedTimer lastFrame;
    bool firstFrame = true;
    lastFrame.start();
    while (!stopRequested.load()) {
        pollfd descriptor = {fd, POLLIN, 0};
        const int ready = poll(&descriptor, 1, 100);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            fail("等待摄像头帧失败");
            break;
        }
        if (!ready) {
            if (lastFrame.elapsed() >= 5000) {
                emit captureError("摄像头连续 5 秒未返回画面，请检查连接后重新打开。");
                break;
            }
            continue;
        }
        if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            emit captureError("摄像头已断开或采集发生错误，请检查连接后重新打开。");
            break;
        }
        if (!(descriptor.revents & POLLIN))
            continue;
        v4l2_buffer buffer = {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        if (cameraIoctl(fd, VIDIOC_DQBUF, &buffer) < 0) {
            if (errno == EAGAIN)
                continue;
            fail("读取摄像头帧失败");
            break;
        }
        const size_t needed = static_cast<size_t>(height - 1) * stride + width * 2;
        if (buffer.index >= buffers.size() || buffer.bytesused > buffers[buffer.index].length ||
            needed > buffers[buffer.index].length || buffer.bytesused < needed) {
            emit captureError("摄像头帧长度或缓冲区索引无效。");
            break;
        }
        if (!(buffer.flags & V4L2_BUF_FLAG_ERROR)) {
            try {
                cv::Mat yuv(height, width, CV_8UC2, buffers[buffer.index].start, stride);
                cv::Mat rgb;
                cv::cvtColor(yuv, rgb, cv::COLOR_YUV2RGB_YUYV);
                QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
                emit frameReady(image.copy());
                if (firstFrame) {
                    qInfo() << "V4L2 first frame received:" << width << "x" << height;
                    firstFrame = false;
                }
                lastFrame.restart();
            } catch (const cv::Exception &error) {
                qWarning() << "Frame conversion failed:" << error.what();
                emit captureError("摄像头图像转换失败，请重新打开。");
                break;
            }
        }
        if (cameraIoctl(fd, VIDIOC_QBUF, &buffer) < 0) {
            fail("摄像头缓冲区重新入队失败");
            break;
        }
    }
    releaseCamera();
}

void V4L2Thread::releaseCamera()
{
    if (streaming) {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cameraIoctl(fd, VIDIOC_STREAMOFF, &type);
        streaming = false;
    }
    for (const Buffer &buffer : buffers)
        if (buffer.start)
            munmap(buffer.start, buffer.length);
    buffers.clear();
    if (fd >= 0) { ::close(fd); fd = -1; }
    qInfo() << "V4L2 camera resources released";
}
