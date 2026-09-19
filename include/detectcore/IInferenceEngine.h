#pragma once
#include <filesystem>
#include <memory>
#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <detectcore/DetectionResult.h>
namespace detectcore {
struct ModelInfo {
    std::filesystem::path directory;
    std::string inputName, outputName;
    int width=0, height=0;
    std::vector<std::string> names;
};
struct Tensor {
    int rows=0, cols=0; // logical [rows, cols], row-major float32
    std::vector<float> values;
    float at(int r,int c) const { return values.at(static_cast<size_t>(r)*cols+c); }
};
class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;
    virtual void load(const ModelInfo&, int threads)=0;
    virtual Tensor infer(const cv::Mat& rgbLetterboxed)=0;
};
std::unique_ptr<IInferenceEngine> createNcnnEngine();
}
