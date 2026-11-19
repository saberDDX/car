#include "yolo.hpp"
#include <rknn_api.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <dlfcn.h>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>

using Clock=std::chrono::steady_clock;
double ms(Clock::time_point a,Clock::time_point b) {return std::chrono::duration<double,std::milli>(b-a).count();}
void check(int result,const char* operation) {
    if(result!=RKNN_SUCC) throw std::runtime_error(std::string(operation)+" failed, RKNN status="+std::to_string(result));
}
class Runtime {
    void* library_=nullptr;
    template<class T> T symbol(const char* name) {
        dlerror(); void* address=dlsym(library_,name); const char* error=dlerror();
        if(error || !address) throw std::runtime_error(std::string("missing RKNN symbol ")+name+": "+(error?error:"null"));
        return reinterpret_cast<T>(address);
    }
public:
    decltype(&rknn_init) init;
    decltype(&rknn_query) query;
    decltype(&rknn_inputs_set) inputs_set;
    decltype(&rknn_run) run;
    decltype(&rknn_outputs_get) outputs_get;
    decltype(&rknn_outputs_release) outputs_release;
    decltype(&rknn_destroy) destroy;
    explicit Runtime(const std::string& path) {
        library_=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);
        if(!library_) throw std::runtime_error(std::string("dlopen: ")+dlerror());
        try {
            init=symbol<decltype(init)>("rknn_init"); query=symbol<decltype(query)>("rknn_query");
            inputs_set=symbol<decltype(inputs_set)>("rknn_inputs_set"); run=symbol<decltype(run)>("rknn_run");
            outputs_get=symbol<decltype(outputs_get)>("rknn_outputs_get");
            outputs_release=symbol<decltype(outputs_release)>("rknn_outputs_release");
            destroy=symbol<decltype(destroy)>("rknn_destroy");
        } catch(...) {dlclose(library_); library_=nullptr; throw;}
    }
    ~Runtime(){if(library_) dlclose(library_);}
    Runtime(const Runtime&)=delete;
    Runtime& operator=(const Runtime&)=delete;
};
class Context {
    Runtime& runtime_;
    std::vector<char> model_; // Keep model storage alive until destroy.
public:
    rknn_context handle=0;
    Context(Runtime& runtime,const std::string& path):runtime_(runtime) {
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if(!file) throw std::runtime_error("cannot open model: "+path);
        const auto length=file.tellg();
        if(length<=0 || static_cast<unsigned long long>(length)>std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("invalid model file size");
        model_.resize(static_cast<std::size_t>(length)); file.seekg(0);
        if(!file.read(model_.data(),length)) throw std::runtime_error("short model read");
        check(runtime_.init(&handle,model_.data(),static_cast<uint32_t>(model_.size()),0,nullptr),"rknn_init");
    }
    ~Context(){if(handle) runtime_.destroy(handle);}
    Context(const Context&)=delete;
    Context& operator=(const Context&)=delete;
};
class Outputs {
    Runtime& runtime_; rknn_context context_; bool acquired_=false;
public:
    std::vector<rknn_output> values;
    Outputs(Runtime& runtime,rknn_context context,uint32_t count):runtime_(runtime),context_(context),values(count) {
        for(uint32_t i=0;i<count;++i){values[i].index=i; values[i].want_float=1;}
    }
    void acquire(){check(runtime_.outputs_get(context_,static_cast<uint32_t>(values.size()),values.data(),nullptr),"rknn_outputs_get");acquired_=true;}
    ~Outputs(){if(acquired_) runtime_.outputs_release(context_,static_cast<uint32_t>(values.size()),values.data());}
    Outputs(const Outputs&)=delete;
    Outputs& operator=(const Outputs&)=delete;
};
struct Options {
    std::string model,image,output="detections.jpg",library="/lib/librknnrt.so";
    int repeat=1,warmup=0;
    float confidence=.25f,nms=.45f;
    std::vector<int> classes{0,2,3,5,7};
};
Options options(int argc,char** argv) {
    Options opt;
    for(int i=1;i<argc;++i) {
        const std::string key=argv[i];
        if(i+1>=argc) throw std::runtime_error("each option requires a value");
        const std::string value=argv[++i];
        if(key=="--model") opt.model=value;
        else if(key=="--image") opt.image=value;
        else if(key=="--output") opt.output=value;
        else if(key=="--library") opt.library=value;
        else if(key=="--repeat") opt.repeat=std::stoi(value);
        else if(key=="--warmup") opt.warmup=std::stoi(value);
        else if(key=="--confidence") opt.confidence=std::stof(value);
        else if(key=="--nms") opt.nms=std::stof(value);
        else if(key=="--classes") {
            opt.classes.clear();
            if(value!="all") {
                std::istringstream list(value); std::string token;
                while(std::getline(list,token,',')) {const int c=std::stoi(token);if(c<0||c>=80)throw std::runtime_error("class outside COCO range");opt.classes.push_back(c);}
                if(opt.classes.empty()) throw std::runtime_error("empty classes");
            }
        } else throw std::runtime_error("unknown option: "+key);
    }
    if(opt.model.empty() || opt.image.empty() || opt.repeat<1 || opt.repeat>10000 || opt.warmup<0 || opt.warmup>1000 ||
       !(opt.confidence>0 && opt.confidence<=1) || !(opt.nms>0 && opt.nms<=1)) throw std::runtime_error("invalid arguments");
    return opt;
}
std::string class_name(int label) {
    switch(label){case 0:return "person";case 1:return "bicycle";case 2:return "car";case 3:return "motorcycle";case 5:return "bus";case 7:return "truck";default:return "class_"+std::to_string(label);}
}
void summary(const char* name,std::vector<double> samples) {
    std::sort(samples.begin(),samples.end());
    const auto quantile=[&](double q){return samples[static_cast<std::size_t>(std::ceil(q*samples.size()))-1];};
    const double average=std::accumulate(samples.begin(),samples.end(),0.)/samples.size();
    std::cout<<name<<"_ms mean="<<average<<" p50="<<quantile(.5)<<" p95="<<quantile(.95)<<" max="<<samples.back()<<'\n';
}
int execute(const Options& opt) {
    cv::Mat image=cv::imread(opt.image);
    if(image.empty()) throw std::runtime_error("cannot read image: "+opt.image);
    Runtime runtime(opt.library);
    const auto loading=Clock::now(); Context context(runtime,opt.model);
    std::cout<<"model_init_ms="<<ms(loading,Clock::now())<<'\n';
    rknn_sdk_version sdk{}; check(runtime.query(context.handle,RKNN_QUERY_SDK_VERSION,&sdk,sizeof(sdk)),"query SDK version");
    std::cout<<"runtime="<<sdk.api_version<<" driver="<<sdk.drv_version<<'\n';
    rknn_input_output_num count{}; check(runtime.query(context.handle,RKNN_QUERY_IN_OUT_NUM,&count,sizeof(count)),"query tensor counts");
    if(count.n_input!=1 || count.n_output!=3) throw std::runtime_error("expected optimized YOLOv5: one input and three heads");
    rknn_tensor_attr input{}; check(runtime.query(context.handle,RKNN_QUERY_INPUT_ATTR,&input,sizeof(input)),"query input");
    const bool nchw=input.fmt==RKNN_TENSOR_NCHW;
    if(input.n_dims!=4 || input.dims[0]!=1 || (!nchw && input.fmt!=RKNN_TENSOR_NHWC)) throw std::runtime_error("unsupported input layout");
    const int height=static_cast<int>(input.dims[nchw?2:1]),width=static_cast<int>(input.dims[nchw?3:2]);
    if(width!=640 || height!=640 || input.dims[nchw?1:3]!=3) throw std::runtime_error("this model contract requires 1x3x640x640 RGB");
    std::vector<rknn_tensor_attr> attributes(count.n_output);
    bool strides[3]={false,false,false};
    for(uint32_t i=0;i<count.n_output;++i) {
        auto& attr=attributes[i];attr.index=i;
        check(runtime.query(context.handle,RKNN_QUERY_OUTPUT_ATTR,&attr,sizeof(attr)),"query output");
        if(attr.n_dims!=4 || attr.fmt!=RKNN_TENSOR_NCHW || attr.dims[0]!=1 || attr.dims[1]!=255 || attr.dims[2]!=attr.dims[3])
            throw std::runtime_error("unsupported output: expected [1,255,H,W] NCHW sigmoid head");
        const int grid=static_cast<int>(attr.dims[2]);
        const int level=grid==80?0:grid==40?1:grid==20?2:-1;
        if(level<0 || strides[level]) throw std::runtime_error("invalid or duplicate head grid");
        strides[level]=true;
        std::cout<<"output["<<i<<"] shape=1,255,"<<grid<<','<<grid<<" type="<<attr.type<<" zp="<<attr.zp<<" scale="<<attr.scale<<'\n';
    }
    const float ratio=std::min(static_cast<float>(width)/image.cols,static_cast<float>(height)/image.rows);
    const int resized_w=std::max(1,static_cast<int>(std::round(image.cols*ratio)));
    const int resized_h=std::max(1,static_cast<int>(std::round(image.rows*ratio)));
    const int left=(width-resized_w)/2,top=(height-resized_h)/2;
    std::vector<double> pre,input_ms,run_ms,get_ms,post,total;
    std::vector<carvision::Box> boxes;
    for(int iteration=0;iteration<opt.warmup+opt.repeat;++iteration) {
        const auto t0=Clock::now();
        cv::Mat resized,rgb,canvas(height,width,CV_8UC3,cv::Scalar(0,0,0));
        cv::resize(image,resized,cv::Size(resized_w,resized_h));
        resized.copyTo(canvas(cv::Rect(left,top,resized_w,resized_h)));
        cv::cvtColor(canvas,rgb,cv::COLOR_BGR2RGB);
        const auto t1=Clock::now();
        rknn_input in{};in.index=0;in.buf=rgb.data;in.size=static_cast<uint32_t>(rgb.total()*rgb.elemSize());
        in.type=RKNN_TENSOR_UINT8;in.fmt=RKNN_TENSOR_NHWC;in.pass_through=0;
        check(runtime.inputs_set(context.handle,1,&in),"rknn_inputs_set");const auto t2=Clock::now();
        check(runtime.run(context.handle,nullptr),"rknn_run");const auto t3=Clock::now();
        Outputs output(runtime,context.handle,count.n_output);output.acquire();const auto t4=Clock::now();
        std::vector<carvision::Head> heads;
        for(uint32_t i=0;i<count.n_output;++i) {
            const auto& attr=attributes[i];const auto& value=output.values[i];
            if(!value.buf || value.size<static_cast<uint64_t>(attr.n_elems)*sizeof(float)) throw std::runtime_error("short float output buffer");
            const int grid=static_cast<int>(attr.dims[2]);
            heads.push_back({static_cast<const float*>(value.buf),grid,grid,width/grid});
        }
        boxes=carvision::nms(carvision::decode(heads,opt.confidence,opt.classes),opt.nms);
        for(auto& box:boxes) {
            box.x1=std::clamp((box.x1-left)/ratio,0.f,static_cast<float>(image.cols));
            box.x2=std::clamp((box.x2-left)/ratio,0.f,static_cast<float>(image.cols));
            box.y1=std::clamp((box.y1-top)/ratio,0.f,static_cast<float>(image.rows));
            box.y2=std::clamp((box.y2-top)/ratio,0.f,static_cast<float>(image.rows));
        }
        const auto t5=Clock::now();
        if(iteration>=opt.warmup) {
            pre.push_back(ms(t0,t1));input_ms.push_back(ms(t1,t2));run_ms.push_back(ms(t2,t3));
            get_ms.push_back(ms(t3,t4));post.push_back(ms(t4,t5));total.push_back(ms(t0,t5));
        }
    }
    std::cout<<std::fixed<<std::setprecision(3)<<"warmup="<<opt.warmup<<" measured="<<opt.repeat<<'\n';
    summary("preprocess",pre);summary("input_set",input_ms);summary("rknn_run",run_ms);
    summary("output_get",get_ms);summary("postprocess",post);summary("processing",total);
    std::cout<<"processing timing excludes image file I/O, drawing, output release, initialization and display; it is not camera end-to-end latency.\n";
    for(const auto& box:boxes) {
        std::cout<<"detection class="<<box.label<<" name="<<class_name(box.label)<<" confidence="<<box.score
                 <<" xyxy="<<box.x1<<','<<box.y1<<','<<box.x2<<','<<box.y2<<'\n';
        cv::rectangle(image,cv::Point(static_cast<int>(box.x1),static_cast<int>(box.y1)),cv::Point(static_cast<int>(box.x2),static_cast<int>(box.y2)),cv::Scalar(0,255,0),2);
        cv::putText(image,class_name(box.label)+" "+cv::format("%.2f",box.score),cv::Point(static_cast<int>(box.x1),std::max(15,static_cast<int>(box.y1)-4)),cv::FONT_HERSHEY_SIMPLEX,.5,cv::Scalar(0,255,0),1);
    }
    if(!cv::imwrite(opt.output,image)) throw std::runtime_error("cannot write output image");
    std::cout<<"inference=PASS detections="<<boxes.size()<<" output="<<opt.output<<'\n';
    return 0;
}
int main(int argc,char** argv) {
    try{return execute(options(argc,argv));}
    catch(const std::exception& error) {
        std::cerr<<"ERROR: "<<error.what()<<"\nUsage: detect_image --model FILE --image FILE [--output FILE] [--library FILE]"
                   " [--warmup N] [--repeat N] [--classes 0,2,3,5,7|all] [--confidence 0.25] [--nms 0.45]\n";
        return 1;
    }
}
