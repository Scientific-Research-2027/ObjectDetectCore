#include <DetectCore.h>
#include "Preprocessor.h"
#include "Postprocessor.h"
#include <opencv2/core.hpp>
#include <cstdlib>
#define CHECK(expr) do { if(!(expr)) { std::cerr<<"FAILED "<<#expr<<" line "<<__LINE__<<"\n"; return 1; } } while(false)
#include <cmath>
#include <iostream>
#include <stdexcept>
int main() {
    using namespace detectcore;
    cv::Mat blank(240,320,CV_8UC3,cv::Scalar(0,0,0));
    auto pre=preprocess(blank,640,640);
    CHECK(pre.rgb.cols==640 && pre.rgb.rows==640);
    CHECK(pre.padLeft==0 && pre.padTop==80 && std::abs(pre.scale-2.f)<1e-5);
    ModelInfo info;info.names={"first","second"};
    Tensor out;out.rows=6;out.cols=2;out.values={320,320,320,320,100,100,100,100,0.90f,0.85f,0.10f,0.05f};
    auto boxes=postprocessRaw(out,pre,info,0.25f,0.45f);
    CHECK(boxes.size()==1 && boxes[0].className=="first");
    CHECK(std::abs(boxes[0].bbox.x-135.f)<1.f && std::abs(boxes[0].bbox.y-95.f)<1.f);
    Tensor invalid=out;invalid.rows=5;
    bool rejected=false;
    try { (void)postprocessRaw(invalid,pre,info,0.25f,0.45f); }
    catch(const std::runtime_error&){rejected=true;}
    CHECK(rejected);
    bool noModel=false;
    try{ DetectCore core;(void)core.detect(blank); }
    catch(const std::logic_error&){noModel=true;}
    CHECK(noModel);
    std::cout<<"PASS preprocess(letterbox), raw decoding, NMS, shape rejection, unloaded model\n";
    return 0;
}
