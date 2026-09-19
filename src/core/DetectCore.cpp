#include <DetectCore.h>
#include "ModelConfig.h"
#include "../models/yolo26/Preprocessor.h"
#include "../models/yolo26/Postprocessor.h"
#include <chrono>
#include <stdexcept>
namespace detectcore {
using Clock=std::chrono::steady_clock;
static double ms(Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();}
DetectCore::DetectCore()=default;
DetectCore::~DetectCore()=default;
void DetectCore::loadModel(const std::filesystem::path& path,int threads){
    if(threads<1||threads>64) throw std::invalid_argument("threads phải trong [1,64]");
    // Strong exception safety: keep previous model on load failure.
    auto model=readModelInfo(path);
    auto engine=createNcnnEngine();
    engine->load(model,threads);
    engine_=std::move(engine);model_=std::move(model);loaded_=true;
}
bool DetectCore::isLoaded()const noexcept{return loaded_;}
const ModelInfo& DetectCore::modelInfo()const {if(!loaded_)throw std::logic_error("Model chưa load");return model_;}
DetectionResult DetectCore::detect(const cv::Mat& image,const Options& options){
    if(!loaded_)throw std::logic_error("Model chưa load");
    auto t0=Clock::now(); auto pre=preprocess(image,model_.width,model_.height);auto t1=Clock::now();
    auto raw=engine_->infer(pre.rgb);auto t2=Clock::now();
    auto boxes=postprocessRaw(raw,pre,model_,options.confidence,options.iou);auto t3=Clock::now();
    return {std::move(boxes),{ms(t0,t1),ms(t1,t2),ms(t2,t3)}};
}
}
