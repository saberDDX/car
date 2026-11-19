#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace carvision {
struct Box { float x1, y1, x2, y2, score; int label; };
struct Head { const float* data; int h, w, stride; };
inline float iou(const Box& a, const Box& b) {
    const float intersection = std::max(0.f, std::min(a.x2,b.x2)-std::max(a.x1,b.x1)) *
                               std::max(0.f, std::min(a.y2,b.y2)-std::max(a.y1,b.y1));
    const float total = (a.x2-a.x1)*(a.y2-a.y1)+(b.x2-b.x1)*(b.y2-b.y1)-intersection;
    return total > 0 ? intersection / total : 0;
}
inline std::vector<Box> nms(std::vector<Box> boxes, float threshold) {
    std::stable_sort(boxes.begin(),boxes.end(),[](const Box& a,const Box& b){return a.score>b.score;});
    std::vector<Box> kept;
    for (const auto& box:boxes) {
        bool suppress=false;
        for (const auto& selected:kept)
            if (box.label==selected.label && iou(box,selected)>threshold) { suppress=true; break; }
        if (!suppress) kept.push_back(box);
    }
    return kept;
}
// Fixed contract: Rockchip optimized COCO YOLOv5, three NCHW sigmoid heads.
// Do NOT apply a second sigmoid or use YOLOv8/decoded [1,N,85] outputs here.
inline std::vector<Box> decode(const std::vector<Head>& heads, float threshold,
                               const std::vector<int>& classes) {
    constexpr float anchors[3][3][2]={{{10,13},{16,30},{33,23}},
                                     {{30,61},{62,45},{59,119}},
                                     {{116,90},{156,198},{373,326}}};
    std::vector<Box> boxes;
    for (const auto& head:heads) {
        const int level=head.stride==8 ? 0 : head.stride==16 ? 1 : head.stride==32 ? 2 : -1;
        if (level<0 || head.h<=0 || head.w<=0 || !head.data) throw std::runtime_error("invalid YOLOv5 head");
        const int plane=head.h*head.w;
        for (int a=0;a<3;++a) for (int y=0;y<head.h;++y) for(int x=0;x<head.w;++x) {
            const int cell=y*head.w+x;
            const auto value=[&](int channel){return head.data[(a*85+channel)*plane+cell];};
            const float object=value(4);
            if (!std::isfinite(object) || object<threshold) continue;
            int label=0;
            for(int c=1;c<80;++c) if(value(5+c)>value(5+label)) label=c;
            const float score=object*value(5+label);
            if (!std::isfinite(score) || score<threshold ||
                (!classes.empty() && std::find(classes.begin(),classes.end(),label)==classes.end())) continue;
            const float cx=(value(0)*2.f-.5f+x)*head.stride;
            const float cy=(value(1)*2.f-.5f+y)*head.stride;
            const float width=std::pow(value(2)*2.f,2.f)*anchors[level][a][0];
            const float height=std::pow(value(3)*2.f,2.f)*anchors[level][a][1];
            if (std::isfinite(cx+cy+width+height) && width>0 && height>0)
                boxes.push_back({cx-width/2,cy-height/2,cx+width/2,cy+height/2,score,label});
        }
    }
    return boxes;
}
} // namespace carvision
