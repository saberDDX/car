#include "frame_statistics.hpp"
#include <opencv2/imgcodecs.hpp>
#include <fstream>
#include <iostream>

volatile std::sig_atomic_t stopped=0;
void stop_diagnostics(int number){stopped=number;}
int main(int argc,char** argv) {
    std::signal(SIGINT,stop_diagnostics);std::signal(SIGTERM,stop_diagnostics);
    try {
        if(argc==3 && std::string(argv[1])=="--inspect") {
            const auto image=cv::imread(argv[2]);
            carvision::print_image_statistics(std::cout,image,argv[2]);
            return 0;
        }
        if(argc!=3) throw std::runtime_error("Usage: camera_diagnostics --inspect IMAGE | DEVICE OUTPUT_DIRECTORY");
        const std::filesystem::path directory=argv[2];
        std::filesystem::create_directories(directory);
        carvision::V4L2Capture camera(argv[1]);
        std::cout<<"controls_before_capture (read-only):\n";camera.print_controls(std::cout);
        int captured=0;
        for(int index=1;index<=60 && !stopped;++index) {
            carvision::CameraFrame frame;
            const bool save=index==1 || index==30 || index==60;
            if(!camera.read(frame,stopped,save)) break;
            ++captured;
            if(!save) continue;
            const std::string name="frame-"+std::to_string(index);
            const auto image_path=directory/(name+".png");
            if(!cv::imwrite(image_path.string(),frame.bgr)) throw std::runtime_error("cannot save diagnostic PNG");
            std::ofstream raw(directory/(name+".yuyv"),std::ios::binary);
            if(!raw.write(reinterpret_cast<const char*>(frame.raw_yuyv.data()),static_cast<std::streamsize>(frame.raw_yuyv.size())))
                throw std::runtime_error("cannot save diagnostic raw bytes");
            std::cout<<"frame="<<index<<" sequence="<<frame.sequence<<" width="<<frame.width<<" height="<<frame.height
                     <<" stride="<<frame.stride<<" raw_bytes="<<frame.raw_yuyv.size()<<'\n';
            carvision::print_image_statistics(std::cout,frame.bgr,image_path.string());
            carvision::print_raw_statistics(std::cout,frame);
            std::ofstream metadata(directory/(name+".txt"));
            metadata<<"format=YUYV width="<<frame.width<<" height="<<frame.height<<" stride="<<frame.stride<<" bytes="<<frame.raw_yuyv.size()<<'\n';
            carvision::print_image_statistics(metadata,frame.bgr,name);
            carvision::print_raw_statistics(metadata,frame);
            if(!metadata) throw std::runtime_error("cannot write diagnostic metadata");
        }
        std::cout<<"controls_after_capture (read-only):\n";camera.print_controls(std::cout);
        std::cout<<"diagnostic_frames="<<captured<<" npu_inference=NOT_RUN control_writes=NONE output="<<directory<<'\n';
        return captured==60?0:1;
    } catch(const std::exception& error){std::cerr<<"ERROR: "<<error.what()<<'\n';return 1;}
}
