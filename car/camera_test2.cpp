#include <iostream>
#include <vector>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

struct Buffer {
    void* start;
    size_t length;
};//起点 + 长度

int main(){
    const char* device = "/dev/video0";
    int fd = open(device, O_RDWR);
    if (fd < 0) {
        std ::cerr << "Failed to open device: " << device << std::endl;
        return -1;
    }

    // 1. 设置视频采集格式 (YUYV 640x480)
    struct v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));// 初始化 fmt 结构体
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        std::cerr << "设置格式失败" << std::endl;
        close(fd);
        return -1;
    }
    std::cout << "1. 格式配置成功: " << fmt.fmt.pix.width << "x" 
              << fmt.fmt.pix.height << " (YUYV)" << std::endl;

    // 2. 向驱动申请缓冲区 (申请 4 个 Buffer)
    struct v4l2_requestbuffers req;
    std::memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        std::cerr << "申请缓冲区失败" << std::endl;
        close(fd);
        return -1;
    }
    std::cout << "2. 成功申请缓冲区数量: " << req.count << std::endl;

    // 3. 将内核缓冲区通过 mmap 映射到用户空间，并压入采集队列
    std::vector<Buffer> buffers(req.count);//std::vector是一个支持动态扩容的连续数组容器  <Buffer>：模板类型参数  buffers：变量名  (req.count)：构造函数实参（带参初始化）
    for (size_t i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {//查询信息填入了&buf
            std::cerr << "查询缓冲区信息失败" << std::endl;
            close(fd);
            return -1;
        }

        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, buf.m.offset);

        if (buffers[i].start == MAP_FAILED) {
            std::cerr << "mmap 内存映射失败" << std::endl;
            close(fd);
            return -1;
        }

        // 把缓冲区挂载到驱动的空闲队列中等待装填
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            std::cerr << "入队失败" << std::endl;
            close(fd);
            return -1;
        }
    }
    std::cout << "3. 内存映射 (mmap) 与入队成功" << std::endl;

    // 4. 开启视频流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        std::cerr << "启动视频流失败" << std::endl;
        close(fd);
        return -1;
    }
    std::cout << "4. 视频流开启成功，等待捕获第一帧..." << std::endl;

    // 5. 从队列取出填充好的帧 (出队)
    struct v4l2_buffer buf;
    std::memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        std::cerr << "捕获视频帧失败" << std::endl;
    } else {
        std::cout << "=== 成功捕获第 1 帧画面 ===" << std::endl;
        std::cout << "使用缓冲区编号: " << buf.index << std::endl;
        std::cout << "有效字节大小: " << buf.bytesused << " 字节 (预期 640*480*2 = 614400 字节)" << std::endl;

        // 重新将 buffer 放回队列
        ioctl(fd, VIDIOC_QBUF, &buf);
    }

    // 6. 关闭流并释放映射
    ioctl(fd, VIDIOC_STREAMOFF, &type);
    for (size_t i = 0; i < buffers.size(); ++i) {
        munmap(buffers[i].start, buffers[i].length);
    }
    close(fd);
    std::cout << "5. 资源释放完毕，测试通过！" << std::endl;

    return 0;
}