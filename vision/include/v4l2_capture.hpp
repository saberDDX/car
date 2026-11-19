#pragma once
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <time.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace carvision {
inline const char* capture_format_name(uint32_t format) {
    return format==V4L2_PIX_FMT_YUYV?"YUYV":format==V4L2_PIX_FMT_MJPEG?"MJPG":"UNKNOWN";
}
inline long long monotonic_ns() {
    timespec value{};
    if(clock_gettime(CLOCK_MONOTONIC,&value)!=0) throw std::runtime_error("clock_gettime failed");
    return static_cast<long long>(value.tv_sec)*1000000000LL+value.tv_nsec;
}
inline cv::Mat copy_yuyv_to_bgr(const void* memory,std::size_t mapped,std::size_t used,
                               unsigned width,unsigned height,unsigned stride) {
    if(!memory || !width || !height || width>4096 || height>4096 || width%2 || stride<width*2 ||
       stride>static_cast<unsigned>(std::numeric_limits<int>::max())) throw std::runtime_error("invalid YUYV format");
    const std::size_t needed=static_cast<std::size_t>(height-1)*stride+width*2;
    if(used>mapped || needed>used) throw std::runtime_error("short or invalid V4L2 frame");
    cv::Mat yuyv(static_cast<int>(height),static_cast<int>(width),CV_8UC2,const_cast<void*>(memory),stride),bgr;
    cv::cvtColor(yuyv,bgr,cv::COLOR_YUV2BGR_YUYV);
    return bgr; // Owns converted pixels; no reference to the MMAP region remains.
}
inline cv::Mat copy_mjpeg_to_bgr(const void* memory,std::size_t mapped,std::size_t used,unsigned width,unsigned height) {
    if(!memory || !used || used>mapped || used>static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
       !width || !height || width>4096 || height>4096) throw std::runtime_error("invalid MJPG payload");
    cv::Mat bytes(1,static_cast<int>(used),CV_8UC1,const_cast<void*>(memory));
    cv::Mat image=cv::imdecode(bytes,cv::IMREAD_COLOR);
    if(image.empty() || image.type()!=CV_8UC3 || image.cols!=static_cast<int>(width) || image.rows!=static_cast<int>(height))
        throw std::runtime_error("MJPG decode failed or dimensions do not match negotiated format");
    return image;
}
struct CameraFrame {
    cv::Mat bgr;
    std::vector<unsigned char> raw_data; // Diagnostic-only source payload, disabled during normal inference.
    uint32_t pixel_format=V4L2_PIX_FMT_YUYV;
    unsigned width=0,height=0,stride=0;
    uint32_t sequence=0;
    long long timestamp_ns=0;
    bool monotonic=false;
    bool start_of_exposure=false;
};
class V4L2Capture {
    struct Buffer {void* data=MAP_FAILED;std::size_t size=0;};
    int fd_=-1;
    bool streaming_=false;
    uint32_t pixel_format_=V4L2_PIX_FMT_YUYV;
    unsigned width_=0,height_=0,stride_=0;
    std::vector<Buffer> buffers_;
    static int call(int fd,unsigned long request,void* argument) {
        int result;
        do {result=ioctl(fd,request,argument);} while(result<0 && errno==EINTR);
        return result;
    }
    static std::runtime_error failure(const std::string& operation) {
        const int error=errno;
        return std::runtime_error(operation+": "+std::strerror(error));
    }
    static bool suitable(int fd,bool require_uvc,const std::string& path,uint32_t required_format=V4L2_PIX_FMT_YUYV) {
        v4l2_capability caps{};
        if(call(fd,VIDIOC_QUERYCAP,&caps)<0) {
            const int error=errno;
            std::cerr<<"camera_probe device="<<path<<" querycap_errno="<<error<<" error="<<std::strerror(error)<<'\n';
            return false;
        }
        const unsigned features=(caps.capabilities&V4L2_CAP_DEVICE_CAPS)?caps.device_caps:caps.capabilities;
        const std::string driver(reinterpret_cast<const char*>(caps.driver),strnlen(reinterpret_cast<const char*>(caps.driver),sizeof(caps.driver)));
        const std::string card(reinterpret_cast<const char*>(caps.card),strnlen(reinterpret_cast<const char*>(caps.card),sizeof(caps.card)));
        std::cerr<<"camera_probe device="<<path<<" driver="<<driver<<" card="<<card;
        if(!(features&V4L2_CAP_VIDEO_CAPTURE) || !(features&V4L2_CAP_STREAMING) ||
           (require_uvc && driver!="uvcvideo")) {
            std::cerr<<" rejected=not_required_capture_streaming_uvc\n";
            return false;
        }
        v4l2_fmtdesc format{};format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bool supported=false;
        std::cerr<<" formats=";
        while(call(fd,VIDIOC_ENUM_FMT,&format)==0) {
            for(int byte=0;byte<4;++byte) {
                const char c=static_cast<char>((format.pixelformat>>(8*byte))&0xff);
                std::cerr<<(c>=32 && c<127?c:'?');
            }
            std::cerr<<',';
            if(format.pixelformat==required_format) supported=true;
            ++format.index;
        }
        std::cerr<<(supported?" accepted=":" rejected=missing_")<<capture_format_name(required_format)<<'\n';
        return supported;
    }
    static bool probe(const std::string& path,bool require_uvc,uint32_t required_format=V4L2_PIX_FMT_YUYV) {
        const int fd=open(path.c_str(),O_RDWR|O_NONBLOCK|O_CLOEXEC);
        if(fd<0) {
            const int error=errno;
            std::cerr<<"camera_probe device="<<path<<" open_errno="<<error<<" error="<<std::strerror(error)<<'\n';
            return false;
        }
        struct Guard{int fd;~Guard(){close(fd);}} guard{fd};
        return suitable(fd,require_uvc,path,required_format);
    }
    static std::string discover(uint32_t required_format=V4L2_PIX_FMT_YUYV) {
        std::set<std::string> matches;
        std::error_code error;
        for(const auto& entry:std::filesystem::directory_iterator("/dev",error)) {
            if(entry.path().filename().string().rfind("video",0)!=0) continue;
            if(probe(entry.path().string(),true,required_format)) matches.insert(entry.path().string());
        }
        if(matches.size()!=1) {
            std::string message="auto camera selection requires one accessible "+std::string(capture_format_name(required_format))+" UVC device; found "+std::to_string(matches.size());
            for(const auto& path:matches) message+=" "+path;
            throw std::runtime_error(message+"; close other camera users or specify --camera /dev/videoN");
        }
        return *matches.begin();
    }
    void release() noexcept {
        if(streaming_) {v4l2_buf_type type=V4L2_BUF_TYPE_VIDEO_CAPTURE;call(fd_,VIDIOC_STREAMOFF,&type);streaming_=false;}
        for(auto& buffer:buffers_) if(buffer.data!=MAP_FAILED) munmap(buffer.data,buffer.size);
        buffers_.clear();
        if(fd_>=0) {close(fd_);fd_=-1;}
    }
public:
    // QUERYCAP/ENUM_FMT only: no S_FMT, buffer allocation, capture or NPU calls.
    static std::string find_device(const std::string& path="auto") {
        if(path=="auto") return discover();
        if(!probe(path,false)) throw std::runtime_error("camera probe rejected "+path+"; see diagnostics above");
        return path;
    }
    explicit V4L2Capture(std::string path,uint32_t pixel_format=V4L2_PIX_FMT_YUYV):pixel_format_(pixel_format) {
        if(pixel_format_!=V4L2_PIX_FMT_YUYV && pixel_format_!=V4L2_PIX_FMT_MJPEG) throw std::runtime_error("unsupported capture format");
        if(path=="auto") path=discover(pixel_format_);
        try {
            fd_=open(path.c_str(),O_RDWR|O_NONBLOCK|O_CLOEXEC);
            if(fd_<0) throw failure("open camera "+path);
            if(!suitable(fd_,false,path,pixel_format_)) throw std::runtime_error("device does not provide requested single-plane streaming capture: "+path);
            v4l2_format format{};format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
            format.fmt.pix.width=640;format.fmt.pix.height=480;
            format.fmt.pix.pixelformat=pixel_format_;format.fmt.pix.field=V4L2_FIELD_ANY;
            if(call(fd_,VIDIOC_S_FMT,&format)<0) throw failure("VIDIOC_S_FMT");
            width_=format.fmt.pix.width;height_=format.fmt.pix.height;stride_=format.fmt.pix.bytesperline;
            if(format.fmt.pix.pixelformat!=pixel_format_ || !width_ || !height_ || width_>4096 || height_>4096 ||
               (pixel_format_==V4L2_PIX_FMT_YUYV && (width_%2 || stride_<width_*2)))
                throw std::runtime_error("camera negotiated unsupported format/dimensions/stride");
            v4l2_requestbuffers request{};request.count=4;request.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;request.memory=V4L2_MEMORY_MMAP;
            if(call(fd_,VIDIOC_REQBUFS,&request)<0) throw failure("VIDIOC_REQBUFS");
            if(!request.count || request.count>32) throw std::runtime_error("invalid MMAP buffer count");
            buffers_.resize(request.count);
            for(unsigned i=0;i<request.count;++i) {
                v4l2_buffer buffer{};buffer.type=request.type;buffer.memory=request.memory;buffer.index=i;
                if(call(fd_,VIDIOC_QUERYBUF,&buffer)<0) throw failure("VIDIOC_QUERYBUF");
                const std::size_t minimum=pixel_format_==V4L2_PIX_FMT_YUYV?static_cast<std::size_t>(height_-1)*stride_+width_*2:1;
                if(buffer.length<minimum) throw std::runtime_error("short MMAP buffer");
                void* address=mmap(nullptr,buffer.length,PROT_READ|PROT_WRITE,MAP_SHARED,fd_,buffer.m.offset);
                if(address==MAP_FAILED) throw failure("mmap");
                buffers_[i]={address,buffer.length};
                if(call(fd_,VIDIOC_QBUF,&buffer)<0) throw failure("initial VIDIOC_QBUF");
            }
            v4l2_buf_type type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if(call(fd_,VIDIOC_STREAMON,&type)<0) throw failure("VIDIOC_STREAMON");
            streaming_=true;
            std::cout<<"camera="<<path<<" format="<<capture_format_name(pixel_format_)<<" width="<<width_<<" height="<<height_<<" stride="<<stride_<<" buffers="<<buffers_.size()<<'\n';
        } catch(...) {release();throw;}
    }
    ~V4L2Capture(){release();}
    V4L2Capture(const V4L2Capture&)=delete;
    V4L2Capture& operator=(const V4L2Capture&)=delete;
    void print_controls(std::ostream& output) const {
        for(const uint32_t id:{V4L2_CID_EXPOSURE_AUTO,V4L2_CID_EXPOSURE_ABSOLUTE,V4L2_CID_EXPOSURE_AUTO_PRIORITY,
                              V4L2_CID_GAIN,V4L2_CID_BRIGHTNESS,V4L2_CID_CONTRAST,V4L2_CID_GAMMA,
                              V4L2_CID_AUTO_WHITE_BALANCE,V4L2_CID_WHITE_BALANCE_TEMPERATURE}) {
            v4l2_queryctrl query{};query.id=id;
            if(call(fd_,VIDIOC_QUERYCTRL,&query)<0 || query.flags&V4L2_CTRL_FLAG_DISABLED) continue;
            v4l2_control control{};control.id=id;
            if(call(fd_,VIDIOC_G_CTRL,&control)<0) continue;
            output<<"control="<<reinterpret_cast<const char*>(query.name)<<" value="<<control.value
                  <<" min="<<query.minimum<<" max="<<query.maximum<<" default="<<query.default_value;
            if(query.type==V4L2_CTRL_TYPE_MENU) {
                v4l2_querymenu menu{};menu.id=id;menu.index=control.value;
                if(call(fd_,VIDIOC_QUERYMENU,&menu)==0) output<<" menu="<<reinterpret_cast<const char*>(menu.name);
            }
            output<<'\n';
        }
    }
    bool read(CameraFrame& frame,const volatile std::sig_atomic_t& stop,bool copy_raw=false) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(!stop) {
            if(std::chrono::steady_clock::now()>=deadline) throw std::runtime_error("camera returned no valid frame for five seconds");
            pollfd descriptor{fd_,POLLIN,0};
            const int ready=poll(&descriptor,1,100);
            if(ready<0) {if(errno==EINTR) continue;throw failure("poll camera");}
            if(!ready) continue;
            if(descriptor.revents&(POLLERR|POLLHUP|POLLNVAL)) throw std::runtime_error("camera disconnected or poll error");
            if(!(descriptor.revents&POLLIN)) continue;
            v4l2_buffer buffer{};buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;buffer.memory=V4L2_MEMORY_MMAP;
            if(call(fd_,VIDIOC_DQBUF,&buffer)<0) {if(errno==EAGAIN) continue;throw failure("VIDIOC_DQBUF");}
            if(buffer.index>=buffers_.size()) throw std::runtime_error("invalid dequeued buffer index");
            const bool valid=(buffer.flags&V4L2_BUF_FLAG_ERROR)==0;
            if(valid) {
                if(pixel_format_==V4L2_PIX_FMT_YUYV)
                    frame.bgr=copy_yuyv_to_bgr(buffers_[buffer.index].data,buffers_[buffer.index].size,buffer.bytesused,width_,height_,stride_);
                else
                    frame.bgr=copy_mjpeg_to_bgr(buffers_[buffer.index].data,buffers_[buffer.index].size,buffer.bytesused,width_,height_);
                frame.pixel_format=pixel_format_;
                frame.width=width_;frame.height=height_;frame.stride=stride_;
                if(copy_raw) {
                    const auto* data=static_cast<const unsigned char*>(buffers_[buffer.index].data);
                    frame.raw_data.assign(data,data+buffer.bytesused);
                } else frame.raw_data.clear();
                frame.sequence=buffer.sequence;
                frame.monotonic=(buffer.flags&V4L2_BUF_FLAG_TIMESTAMP_MASK)==V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
                frame.start_of_exposure=(buffer.flags&V4L2_BUF_FLAG_TSTAMP_SRC_MASK)==V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
                frame.timestamp_ns=static_cast<long long>(buffer.timestamp.tv_sec)*1000000000LL+buffer.timestamp.tv_usec*1000LL;
            }
            if(call(fd_,VIDIOC_QBUF,&buffer)<0) throw failure("VIDIOC_QBUF");
            if(valid) return true;
        }
        return false;
    }
};
} // namespace carvision
