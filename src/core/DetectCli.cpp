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
static int run(std::filesystem::path model,std::filesystem::path image,float confidence,float iou,bool audit){
    try {
        detectcore::DetectCore core;core.loadModel(model);
        auto rgb=readImage(image);
        std::vector<detectcore::BirdKiteEvidence> evidence;
        auto result=core.detect(rgb,{confidence,iou,4},audit?&evidence:nullptr);
        std::cout<<std::fixed<<std::setprecision(6);
        std::cout<<"{\"detections\":[";
        for(size_t i=0;i<result.detections.size();++i){
            const auto& d=result.detections[i];
            if(i)std::cout<<",";
            std::cout<<"{\"class_id\":"<<d.classId<<",\"score\":"<<d.confidence
                     <<",\"xyxy\":["<<d.bbox.x<<","<<d.bbox.y<<","<<d.bbox.x+d.bbox.width
                     <<","<<d.bbox.y+d.bbox.height<<"]}";
        }
        std::cout<<"]";
        if (audit) {
            // These are paired scores at the same output candidate, BEFORE NMS;
            // downstream tooling can distinguish an output-decoding bug from
            // model confusion, without changing any GUI detection behavior.
            std::cout<<",\"bird_kite_evidence\":[";
            for (size_t i=0;i<evidence.size();++i) {
                const auto& e=evidence[i];
                if (i) std::cout<<",";
                std::cout<<"{\"class_id\":"<<e.predictedClassId
                         <<",\"candidate_index\":"<<e.candidateIndex
                         <<",\"bird_score\":"<<e.birdScore
                         <<",\"kite_score\":"<<e.kiteScore
                         <<",\"xyxy\":["<<e.bbox.x<<","<<e.bbox.y<<"," 
                         <<e.bbox.x+e.bbox.width<<","<<e.bbox.y+e.bbox.height<<"]}";
            }
            std::cout<<"]";
        }
        std::cout<<",\"timing_ms\":{\"preprocess\":"<<result.timing.preprocessMs
                 <<",\"inference\":"<<result.timing.inferenceMs
                 <<",\"postprocess\":"<<result.timing.postprocessMs<<"}}\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"Error: "<<e.what()<<"\n";return 1;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
    if(argc!=5 && argc!=6){std::wcerr<<L"Usage: detectcore_cli MODEL_DIR IMAGE_PATH CONF IOU [--bird-kite-audit]\n";return 2;}
    if(argc==6 && std::wstring(argv[5])!=L"--bird-kite-audit")return 2;
    try { return run(argv[1],argv[2],std::stof(argv[3]),std::stof(argv[4]),argc==6); }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
#else
int main(int argc,char** argv){
    if(argc!=5 && argc!=6){std::cerr<<"Usage: detectcore_cli MODEL_DIR IMAGE_PATH CONF IOU [--bird-kite-audit]\n";return 2;}
    if(argc==6 && std::string(argv[5])!="--bird-kite-audit")return 2;
    try{return run(argv[1],argv[2],std::stof(argv[3]),std::stof(argv[4]),argc==6);}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
#endif
