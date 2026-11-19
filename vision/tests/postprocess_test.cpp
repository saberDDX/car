#include "yolo.hpp"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
int main() {
    // An activated head with one object at cell (0,0), anchor (10,13).
    std::vector<float> tensor(255,0.f);
    tensor[0]=tensor[1]=tensor[2]=tensor[3]=.5f;
    tensor[4]=.8f; tensor[5]=.75f;
    const auto boxes=carvision::decode({{tensor.data(),1,1,8}},.25f,{0});
    require(boxes.size()==1,"one decoded person");
    require(std::abs(boxes[0].score-.6f)<1e-6f,"objectness times class confidence");
    require(std::abs(boxes[0].x1+1.f)<1e-6f && std::abs(boxes[0].x2-9.f)<1e-6f,"anchor/grid decode without extra sigmoid");
    require(carvision::decode({{tensor.data(),1,1,8}},.25f,{2}).empty(),"class filter");
    // Multi-cell tensor exercises channel-major addressing, not HWC.
    std::vector<float> grid(255*4,0.f);
    for(int c=0;c<4;++c) grid[c*4+3]=.5f;
    grid[4*4+3]=.9f; grid[7*4+3]=.9f;
    auto shifted=carvision::decode({{grid.data(),2,2,8}},.25f,{2});
    require(shifted.size()==1 && shifted[0].label==2 && std::abs(shifted[0].x1-7.f)<1e-6f,"NCHW indexing and nonzero grid cell");
    auto overlapping=boxes;
    overlapping.push_back(boxes[0]); overlapping.back().score=.5f;
    overlapping.push_back(boxes[0]); overlapping.back().label=2;
    require(carvision::nms(overlapping,.45f).size()==2,"NMS suppresses same-class duplicates, retains different classes");
    require(carvision::decode({{tensor.data(),1,1,8}},.9f,{0}).empty(),"confidence rejection");
    std::cout<<"PASS: YOLOv5 decode, layout, confidence, class filter and class-aware NMS\n";
}
