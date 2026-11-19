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
    const auto before=fd_count();
    for(int i=0;i<50;++i){
        rejected=false;
        try{carvision::V4L2Capture camera("/dev/null");}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"reject non-camera device");
    }
    require(fd_count()==before,"failed camera construction must close descriptor");
    std::cout<<"PASS: YUYV stride/ownership/bounds and failed-open descriptor cleanup; no real camera exercised\n";
}
