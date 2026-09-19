#pragma once
#include <opencv2/core.hpp>
namespace detectcore {
struct LetterboxResult {
    cv::Mat rgb;
    float scale=1.0f;
    int padLeft=0, padTop=0;
    int originalWidth=0, originalHeight=0;
};
LetterboxResult preprocess(const cv::Mat& bgr, int width, int height);
}
