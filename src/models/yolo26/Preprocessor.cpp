#include "Preprocessor.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace detectcore {
LetterboxResult preprocess(const cv::Mat& src, int w, int h) {
    if (src.empty() || src.depth()!=CV_8U || src.channels()!=3)
        throw std::invalid_argument("Ảnh đầu vào phải là cv::Mat BGR CV_8UC3 không rỗng");
    if(w<=0 || h<=0) throw std::invalid_argument("Kích thước model không hợp lệ");
    LetterboxResult result;
    result.originalWidth=src.cols; result.originalHeight=src.rows;
    result.scale=std::min(static_cast<float>(w)/src.cols, static_cast<float>(h)/src.rows);
    int rw=std::max(1, std::min(w, static_cast<int>(std::round(src.cols*result.scale))));
    int rh=std::max(1, std::min(h, static_cast<int>(std::round(src.rows*result.scale))));
    cv::Mat resized, padded(h,w,CV_8UC3,cv::Scalar(114,114,114));
    cv::resize(src,resized,{rw,rh},0,0,cv::INTER_LINEAR);
    result.padLeft=(w-rw)/2; result.padTop=(h-rh)/2;
    resized.copyTo(padded(cv::Rect(result.padLeft,result.padTop,rw,rh)));
    cv::cvtColor(padded,result.rgb,cv::COLOR_BGR2RGB);
    return result;
}
}
