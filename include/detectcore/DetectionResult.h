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
// Optional diagnostic only. Scores are RAW values at the same YOLO candidate;
// they are NOT calibrated probabilities and must not override the model's label.
struct BirdKiteEvidence {
    cv::Rect2f bbox;
    int predictedClassId = -1;
    int candidateIndex = -1;
    float birdScore = 0.f;
    float kiteScore = 0.f;
};
struct Timing { double preprocessMs=0, inferenceMs=0, postprocessMs=0; };
struct DetectionResult { std::vector<Detection> detections; Timing timing; };
struct Options { float confidence=0.25f, iou=0.45f; int threads=4; };
}
