#include "yolo.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=4) return 2;
    try {
        std::vector<std::vector<float>> data(3);
        std::vector<carvision::Head> heads;
        for(int i=0;i<3;++i) {
            const int stride=8<<i,grid=640/stride;
            data[i].resize(255*grid*grid);
            std::ifstream file(argv[i+1],std::ios::binary);
            if(!file.read(reinterpret_cast<char*>(data[i].data()),static_cast<std::streamsize>(data[i].size()*sizeof(float))))
                throw std::runtime_error("cannot read float head");
            if(file.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("head has trailing data");
            heads.push_back({data[i].data(),grid,grid,stride});
        }
        auto boxes=carvision::nms(carvision::decode(heads,.25f,{}),.45f);
        std::cout<<std::setprecision(9);
        for(const auto& b:boxes)std::cout<<b.label<<' '<<b.score<<' '<<b.x1<<' '<<b.y1<<' '<<b.x2<<' '<<b.y2<<'\n';
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
