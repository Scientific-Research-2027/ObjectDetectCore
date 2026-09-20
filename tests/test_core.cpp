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
    // For ambiguous bird-shaped kites, the winning raw bird score cannot be
    // corrected merely by changing NMS, confidence thresholds or class names.
    ModelInfo coco; coco.names.resize(80);
    for(int id=0;id<80;++id)coco.names[id]="class_"+std::to_string(id);
    coco.names[14]="bird"; coco.names[33]="kite";
    Tensor pair;pair.rows=84;pair.cols=2;
    pair.values.assign(static_cast<size_t>(pair.rows)*pair.cols,0.f);
    pair.values[0]=320; pair.values[1]=500; // cx
    pair.values[2]=320; pair.values[3]=320; // cy
    pair.values[4]=100; pair.values[5]=100; // width
    pair.values[6]=100; pair.values[7]=100; // height
    pair.values[(4+14)*2+0]=.90f;pair.values[(4+33)*2+0]=.82f;
    pair.values[(4+14)*2+1]=.71f;pair.values[(4+33)*2+1]=.92f;
    std::vector<BirdKiteEvidence> probe;
    auto pairDetections=postprocessRaw(pair,pre,coco,.25f,.45f,&probe);
    CHECK(pairDetections.size()==2 && probe.size()==2);
    CHECK(pairDetections[0].classId==33 && pairDetections[1].classId==14);
    CHECK(std::abs(probe[0].birdScore-.71f)<1e-6f &&
          std::abs(probe[0].kiteScore-.92f)<1e-6f);
    CHECK(std::abs(probe[1].birdScore-.90f)<1e-6f &&
          std::abs(probe[1].kiteScore-.82f)<1e-6f);
    CHECK(probe[0].candidateIndex==1 && probe[1].candidateIndex==0);
    ModelInfo renamed=coco;renamed.names[14]="avian";
    auto unrecognized=postprocessRaw(pair,pre,renamed,.25f,.45f,&probe);
    CHECK(unrecognized.size()==2 && probe.empty());
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
    std::cout<<"PASS letterbox, float NMS, bird/kite raw-score audit, invalid input, unloaded model\n";
    return 0;
}
