#include "Postprocessor.h"
#include <opencv2/dnn/dnn.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace detectcore {
std::vector<Detection> postprocessRaw(const Tensor& raw, const LetterboxResult& prep,
                                      const ModelInfo& model, float conf, float iou) {
    const size_t nc=model.names.size();
    if(nc==0 || raw.rows != static_cast<int>(nc)+4 || raw.cols<1 ||
       raw.values.size()!=static_cast<size_t>(raw.rows)*raw.cols)
        throw std::runtime_error("NCNN output không khớp RAW [4+classes, candidates]; dừng xử lý an toàn. rows="+
            std::to_string(raw.rows)+" cols="+std::to_string(raw.cols)+" classes="+std::to_string(nc));
    if(!(conf>=0 && conf<=1 && iou>=0 && iou<=1)) throw std::invalid_argument("Threshold không hợp lệ");
    if(prep.scale<=0 || prep.originalWidth<=0 || prep.originalHeight<=0) throw std::logic_error("Letterbox không hợp lệ");
    std::vector<cv::Rect> rectangles;
    std::vector<float> scores;
    std::vector<int> classes;
    std::vector<cv::Rect2f> exact;
    for(int n=0;n<raw.cols;++n){
        int cls=-1; float best=conf;
        for(int c=0;c<static_cast<int>(nc);++c){
            const float s=raw.at(c+4,n);
            if(std::isfinite(s) && s>=best){ best=s; cls=c; }
        }
        if(cls<0) continue;
        const float x=raw.at(0,n),y=raw.at(1,n),w=raw.at(2,n),h=raw.at(3,n);
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0) continue;
        float x1=std::clamp((x-w*0.5f-prep.padLeft)/prep.scale,0.f,static_cast<float>(prep.originalWidth));
        float y1=std::clamp((y-h*0.5f-prep.padTop)/prep.scale,0.f,static_cast<float>(prep.originalHeight));
        float x2=std::clamp((x+w*0.5f-prep.padLeft)/prep.scale,0.f,static_cast<float>(prep.originalWidth));
        float y2=std::clamp((y+h*0.5f-prep.padTop)/prep.scale,0.f,static_cast<float>(prep.originalHeight));
        if(x2<=x1||y2<=y1) continue;
        exact.emplace_back(x1,y1,x2-x1,y2-y1);
        // NMS uses integer rectangles, final boxes remain floating-point.
        int ix=static_cast<int>(std::floor(x1)), iy=static_cast<int>(std::floor(y1));
        rectangles.emplace_back(ix,iy,std::max(1,static_cast<int>(std::ceil(x2))-ix),
                                      std::max(1,static_cast<int>(std::ceil(y2))-iy));
        scores.push_back(best); classes.push_back(cls);
    }
    std::vector<Detection> out;
    // Class-aware NMS: different classes must not suppress each other.
    for(int c=0;c<static_cast<int>(nc);++c){
        std::vector<cv::Rect> r; std::vector<float> s; std::vector<size_t> ids;
        for(size_t i=0;i<classes.size();++i) if(classes[i]==c){r.push_back(rectangles[i]);s.push_back(scores[i]);ids.push_back(i);}
        if(r.empty()) continue;
        std::vector<int> keep; cv::dnn::NMSBoxes(r,s,conf,iou,keep);
        for(int local:keep){size_t ix=ids.at(static_cast<size_t>(local));
            out.push_back({c,model.names.at(static_cast<size_t>(c)),scores[ix],exact[ix]});}
    }
    std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.confidence>b.confidence;});
    if(out.size()>300) out.resize(300);
    return out;
}
}
