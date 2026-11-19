#pragma once
#include "v4l2_capture.hpp"
#include <cmath>
#include <iomanip>
#include <ostream>
#include <utility>

namespace carvision {
struct ImageStatistics {
    cv::Scalar mean,stddev;
    double white_percent=0,black_percent=0;
};
inline ImageStatistics image_statistics(const cv::Mat& image) {
    if(image.empty() || image.type()!=CV_8UC3) throw std::runtime_error("expected nonempty BGR8 image");
    ImageStatistics stats;
    cv::meanStdDev(image,stats.mean,stats.stddev);
    cv::Mat mask;
    cv::inRange(image,cv::Scalar(250,250,250),cv::Scalar(255,255,255),mask);
    stats.white_percent=100.*cv::countNonZero(mask)/image.total();
    cv::inRange(image,cv::Scalar(0,0,0),cv::Scalar(5,5,5),mask);
    stats.black_percent=100.*cv::countNonZero(mask)/image.total();
    return stats;
}
inline void print_image_statistics(std::ostream& out,const cv::Mat& image,const std::string& label) {
    const auto stats=image_statistics(image);
    out<<std::fixed<<std::setprecision(3)<<"image="<<label<<" size="<<image.cols<<'x'<<image.rows
       <<" mean_BGR="<<stats.mean[0]<<','<<stats.mean[1]<<','<<stats.mean[2]
       <<" std_BGR="<<stats.stddev[0]<<','<<stats.stddev[1]<<','<<stats.stddev[2]
       <<" near_white_percent="<<stats.white_percent<<" near_black_percent="<<stats.black_percent<<'\n';
}
struct ChannelStatistics {
    double sum=0,sum_square=0;
    std::size_t count=0;
    unsigned min=255,max=0;
    void add(unsigned value){sum+=value;sum_square+=value*value;++count;min=std::min(min,value);max=std::max(max,value);}
    double mean() const{return count?sum/count:0;}
    double stddev() const{return count?std::sqrt(std::max(0.,sum_square/count-mean()*mean())):0;}
};
struct RawStatistics {ChannelStatistics y,u,v;};
inline RawStatistics raw_statistics(const CameraFrame& frame) {
    if(!frame.width || !frame.height || frame.width>4096 || frame.height>4096 || frame.width%2 || frame.stride<frame.width*2 ||
       frame.raw_yuyv.size()<static_cast<std::size_t>(frame.height-1)*frame.stride+frame.width*2)
        throw std::runtime_error("invalid diagnostic YUYV data");
    RawStatistics stats;
    for(unsigned row=0;row<frame.height;++row) {
        const auto* data=frame.raw_yuyv.data()+static_cast<std::size_t>(row)*frame.stride;
        for(unsigned x=0;x<frame.width;x+=2) {
            stats.y.add(data[2*x]);stats.u.add(data[2*x+1]);
            stats.y.add(data[2*x+2]);stats.v.add(data[2*x+3]);
        }
    }
    return stats;
}
inline void print_raw_statistics(std::ostream& out,const CameraFrame& frame) {
    const auto stats=raw_statistics(frame);
    for(const auto& item:{std::make_pair("Y",stats.y),std::make_pair("U",stats.u),std::make_pair("V",stats.v)})
        out<<"raw_channel="<<item.first<<" min="<<item.second.min<<" max="<<item.second.max
           <<" mean="<<item.second.mean()<<" std="<<item.second.stddev()<<'\n';
}
} // namespace carvision
