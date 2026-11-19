#include "v4l2_capture.hpp"
#include <iostream>
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::size_t fd_count(){std::size_t count=0;for(const auto& entry:std::filesystem::directory_iterator("/proc/self/fd")){(void)entry;++count;}return count;}
int main(){
    // Two 2-pixel rows, each padded to 8 bytes; neutral chroma gives grayscale.
    std::vector<unsigned char> raw{16,128,235,128,0,0,0,0,235,128,16,128,0,0,0,0};
    auto image=carvision::copy_yuyv_to_bgr(raw.data(),raw.size(),12,2,2,8);
    require(image.at<cv::Vec3b>(0,0)[0]<2 && image.at<cv::Vec3b>(0,1)[0]>253,"YUYV levels");
    require(image.at<cv::Vec3b>(1,0)[0]>253 && image.at<cv::Vec3b>(1,1)[0]<2,"padded stride");
    std::fill(raw.begin(),raw.end(),0);
    require(image.at<cv::Vec3b>(1,0)[0]>253,"converted frame must own pixels after MMAP reuse");
    bool rejected=false;
    try{carvision::copy_yuyv_to_bgr(raw.data(),raw.size(),11,2,2,8);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"short frame rejection");
    rejected=false;
    try{carvision::copy_yuyv_to_bgr(raw.data(),raw.size(),17,2,2,8);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"bytesused beyond mapping rejection");
    rejected=false;
    try{carvision::copy_yuyv_to_bgr(raw.data(),raw.size(),12,3,2,8);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"odd YUYV width rejection");
    cv::Mat source(8,16,CV_8UC3,cv::Scalar(40,100,180));
    std::vector<unsigned char> encoded;
    require(cv::imencode(".jpg",source,encoded),"JPEG fixture encode");
    auto jpeg=carvision::copy_mjpeg_to_bgr(encoded.data(),encoded.size(),encoded.size(),16,8);
    const auto color=jpeg.at<cv::Vec3b>(3,3);
    require(std::abs(color[0]-40)<=3 && std::abs(color[1]-100)<=3 && std::abs(color[2]-180)<=3,"MJPG color decode");
    rejected=false;
    try{carvision::copy_mjpeg_to_bgr(encoded.data(),encoded.size(),encoded.size()+1,16,8);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"MJPG bytesused bounds");
    rejected=false;
    try{carvision::copy_mjpeg_to_bgr(encoded.data(),encoded.size(),encoded.size(),8,8);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"MJPG negotiated dimensions check");
    std::fill(encoded.begin(),encoded.end(),0);
    require(jpeg.at<cv::Vec3b>(3,3)==color,"decoded JPEG owns memory independently of queued buffer");
    const auto before=fd_count();
    for(int i=0;i<50;++i){
        rejected=false;
        try{carvision::V4L2Capture camera("/dev/null");}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"reject non-camera device");
    }
    require(fd_count()==before,"failed camera construction must close descriptor");
    std::cout<<"PASS: YUYV/MJPG conversion, bounds, ownership and failed-open descriptor cleanup; no real camera exercised\n";
}
