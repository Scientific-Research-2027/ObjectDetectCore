#pragma once
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace detectcore {
struct Detection {
    int classId = -1;
    std::string className;
    float confidence = 0;
    cv::Rect2f bbox; // x,y,width,height in ORIGINAL image pixels
};
struct Timing { double preprocessMs=0, inferenceMs=0, postprocessMs=0; };
struct DetectionResult { std::vector<Detection> detections; Timing timing; };
struct Options { float confidence=0.25f, iou=0.45f; int threads=4; };
}
