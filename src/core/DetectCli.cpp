#include <DetectCore.h>
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <iomanip>
#include <stdexcept>
static cv::Mat readImage(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary);
    if(!f)throw std::runtime_error("Cannot open image");
    std::vector<uchar> b((std::istreambuf_iterator<char>(f)),{});
    return cv::imdecode(b,cv::IMREAD_COLOR);
}
static int run(std::filesystem::path model,std::filesystem::path image,float confidence,float iou){
    try {
        detectcore::DetectCore core;core.loadModel(model);
        auto rgb=readImage(image);
        auto result=core.detect(rgb,{confidence,iou,4});
        std::cout<<std::fixed<<std::setprecision(6);
        std::cout<<"{\"detections\":[";
        for(size_t i=0;i<result.detections.size();++i){
            const auto& d=result.detections[i];
            if(i)std::cout<<",";
            std::cout<<"{\"class_id\":"<<d.classId<<",\"score\":"<<d.confidence
                     <<",\"xyxy\":["<<d.bbox.x<<","<<d.bbox.y<<","<<d.bbox.x+d.bbox.width
                     <<","<<d.bbox.y+d.bbox.height<<"]}";
        }
        std::cout<<"],\"timing_ms\":{\"preprocess\":"<<result.timing.preprocessMs
                 <<",\"inference\":"<<result.timing.inferenceMs
                 <<",\"postprocess\":"<<result.timing.postprocessMs<<"}}\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"Error: "<<e.what()<<"\n";return 1;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
    if(argc!=5){std::wcerr<<L"Usage: detectcore_cli MODEL_DIR IMAGE_PATH CONF IOU\n";return 2;}
    try { return run(argv[1],argv[2],std::stof(argv[3]),std::stof(argv[4])); }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
#else
int main(int argc,char** argv){
    if(argc!=5){std::cerr<<"Usage: detectcore_cli MODEL_DIR IMAGE_PATH CONF IOU\n";return 2;}
    try{return run(argv[1],argv[2],std::stof(argv[3]),std::stof(argv[4]));}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
#endif
