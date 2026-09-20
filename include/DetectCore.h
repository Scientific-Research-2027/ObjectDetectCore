#pragma once
#include <detectcore/IInferenceEngine.h>
namespace detectcore {
class DetectCore {
public:
    DetectCore();
    ~DetectCore();
    DetectCore(const DetectCore&)=delete;
    DetectCore& operator=(const DetectCore&)=delete;
    void loadModel(const std::filesystem::path& modelDirectory, int threads=4);
    DetectionResult detect(const cv::Mat& bgrImage, const Options& options={},
                           std::vector<BirdKiteEvidence>* evidence=nullptr);
    bool isLoaded() const noexcept;
    const ModelInfo& modelInfo() const;
private:
    ModelInfo model_;
    std::unique_ptr<IInferenceEngine> engine_;
    bool loaded_ = false;
};
}
