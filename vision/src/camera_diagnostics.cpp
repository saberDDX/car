#include "frame_statistics.hpp"
#include <opencv2/imgcodecs.hpp>
#include <fstream>
#include <iostream>

volatile std::sig_atomic_t stopped=0;
void stop_diagnostics(int number){stopped=number;}
int capture_format(const std::string& device,const std::filesystem::path& directory,uint32_t format) {
        std::filesystem::create_directories(directory);
        carvision::V4L2Capture camera(device,format);
        std::cout<<"diagnostic_format="<<carvision::capture_format_name(format)<<'\n';
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
            std::ofstream raw(directory/(name+(format==V4L2_PIX_FMT_YUYV?".yuyv":".jpg")),std::ios::binary);
            if(!raw.write(reinterpret_cast<const char*>(frame.raw_data.data()),static_cast<std::streamsize>(frame.raw_data.size())))
                throw std::runtime_error("cannot save diagnostic raw bytes");
            std::cout<<"format="<<carvision::capture_format_name(format)<<" frame="<<index<<" sequence="<<frame.sequence<<" width="<<frame.width<<" height="<<frame.height
                     <<" stride="<<frame.stride<<" raw_bytes="<<frame.raw_data.size()<<'\n';
            carvision::print_image_statistics(std::cout,frame.bgr,image_path.string());
            if(format==V4L2_PIX_FMT_YUYV) carvision::print_raw_statistics(std::cout,frame);
            std::ofstream metadata(directory/(name+".txt"));
            metadata<<"format="<<carvision::capture_format_name(format)<<" width="<<frame.width<<" height="<<frame.height<<" stride="<<frame.stride<<" bytes="<<frame.raw_data.size()<<'\n';
            carvision::print_image_statistics(metadata,frame.bgr,name);
            if(format==V4L2_PIX_FMT_YUYV) carvision::print_raw_statistics(metadata,frame);
            if(!metadata) throw std::runtime_error("cannot write diagnostic metadata");
        }
        std::cout<<"controls_after_capture (read-only):\n";camera.print_controls(std::cout);
        std::cout<<"diagnostic_format="<<carvision::capture_format_name(format)<<" frames="<<captured<<" npu_inference=NOT_RUN control_writes=NONE output="<<directory<<'\n';
        return captured==60?0:1;
}
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
        int completed=0;
        for(const uint32_t format:{V4L2_PIX_FMT_YUYV,V4L2_PIX_FMT_MJPEG}) {
            if(stopped) break;
            const auto target=format==V4L2_PIX_FMT_YUYV?directory:directory/"mjpg";
            try {if(capture_format(argv[1],target,format)==0) ++completed;}
            catch(const std::exception& error) {
                std::cerr<<"diagnostic_format="<<carvision::capture_format_name(format)<<" ERROR: "<<error.what()<<'\n';
                std::ofstream(target/"error.txt")<<error.what()<<'\n';
            }
        }
        std::cout<<"formats_completed="<<completed<<"/2 npu_inference=NOT_RUN control_writes=NONE\n";
        return completed==2?0:1;
    } catch(const std::exception& error){std::cerr<<"ERROR: "<<error.what()<<'\n';return 1;}
}
