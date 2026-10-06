#include <iostream>
#include <fcntl.h>      // 提供 open()
#include <unistd.h>     // 提供 close()
#include <sys/ioctl.h>  // 提供 ioctl()
#include <linux/videodev2.h> // V4L2 核心头文件
#include <cstring>

int main(){
    const char* dev_name = "/dev/video0";

    // 1. 打开设备节点 (O_RDWR 表示可读可写)
    int fd = open(dev_name, O_RDWR);
    if (fd < 0) {
        std::cerr << "Failed to open device: " << dev_name << std::endl;//cout是常规打印，cerr是错误打印
        return -1;
}
std::cout << "device opened successfully: " <<fd << std::endl;

// 2. 查询设备能力 (Capability)
struct v4l2_capability cap;
if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {//向底层摄像头驱动发送一个“查户口”的指令，把摄像头的身份档案（能力信息）填进你准备好的 cap 箱子里
    std::cerr <<"faild to query device capabilities" << std:: endl;
    close(fd);
    return -1;
}

// 打印内核驱动返回的信息
    std::cout << "=== 摄像头底层信息 ===" << std::endl;
    std::cout << "驱动名称 (Driver): " << cap.driver << std::endl;
    std::cout << "设备名称 (Card): " << cap.card << std::endl;
    std::cout << "总线信息 (Bus info): " << cap.bus_info << std::endl;

    // 完事后记得关闭节点
    close(fd);
    return 0;
}