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
    CHECK(std::abs(pre.scaleX-2.f)<1e-5 && std::abs(pre.scaleY-2.f)<1e-5);
    // Rounded resize height must have its own inverse scale (not nominal scale).
    const auto uneven=preprocess(cv::Mat(997,1279,CV_8UC3,cv::Scalar(0,0,0)),640,640);
    CHECK(uneven.scaleX>0 && uneven.scaleY>0);
    CHECK(std::abs(uneven.scaleX-uneven.scaleY)>1.e-6f);
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
    // Same-class overlap is removed, different class overlap is retained.
    Tensor multi; multi.rows=6; multi.cols=3;
    multi.values={320,320,320, 320,320,320, 100,100,100, 100,100,100,
                  .95f,.90f,.02f, .01f,.02f,.99f};
    auto retained=postprocessRaw(multi,pre,info,.25f,.45f);
    CHECK(retained.size()==2 && retained[0].classId==1 && retained[1].classId==0);
    // Reject NaN thresholds and malformed tensor shape rather than reading outside buffers.
    bool badThreshold=false;
    try {(void)postprocessRaw(out,pre,info,std::nanf(""),.45f);}
    catch(const std::invalid_argument&){badThreshold=true;}
    CHECK(badThreshold);
    // Fractional adjacent boxes: integer-rectangle NMS used to change overlap.
    LetterboxResult fractional; fractional.originalWidth=100;fractional.originalHeight=100;
    fractional.scale=1; fractional.scaleX=1;fractional.scaleY=1;
    ModelInfo one;one.names={"object"};
    Tensor narrow;narrow.rows=5;narrow.cols=2;
    narrow.values={10.49f,11.49f, 10.f,10.f, 1.01f,1.01f, 1.01f,1.01f, .9f,.8f};
    CHECK(postprocessRaw(narrow,fractional,one,.25f,.25f).size()==2);
    bool noModel=false;
    try{ DetectCore core;(void)core.detect(blank); }
    catch(const std::logic_error&){noModel=true;}
    CHECK(noModel);
    std::cout<<"PASS letterbox scales, float class-aware NMS, invalid thresholds/shape, unloaded model\n";
    return 0;
}
