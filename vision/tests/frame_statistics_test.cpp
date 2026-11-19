#include "frame_statistics.hpp"
#include <iostream>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    auto white=carvision::image_statistics(cv::Mat(4,4,CV_8UC3,cv::Scalar(255,255,255)));
    require(white.white_percent==100 && white.black_percent==0 && white.mean[0]==255 && white.stddev[0]==0,"uniform white image");
    auto black=carvision::image_statistics(cv::Mat(4,4,CV_8UC3,cv::Scalar(0,0,0)));
    require(black.black_percent==100 && black.white_percent==0,"uniform black image");
    carvision::CameraFrame frame;
    frame.width=2;frame.height=2;frame.stride=8;
    frame.raw_yuyv={16,128,235,128,255,255,255,255,235,128,16,128,255,255,255,255};
    const auto raw=carvision::raw_statistics(frame);
    require(raw.y.count==4 && raw.y.min==16 && raw.y.max==235 && raw.y.mean()==125.5,"luma statistics ignore padding");
    require(raw.u.mean()==128 && raw.v.mean()==128 && raw.u.stddev()==0,"neutral chroma statistics");
    auto image=carvision::copy_yuyv_to_bgr(frame.raw_yuyv.data(),frame.raw_yuyv.size(),12,2,2,8);
    auto mixed=carvision::image_statistics(image);
    require(mixed.white_percent==50 && mixed.black_percent==50,"raw and converted grayscale levels");
    frame.raw_yuyv.resize(11);
    bool rejected=false;
    try{carvision::raw_statistics(frame);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"diagnostic rejects truncated raw frame");
    std::cout<<"PASS: white/black image statistics and padded YUYV statistics; synthetic fixtures only\n";
}
