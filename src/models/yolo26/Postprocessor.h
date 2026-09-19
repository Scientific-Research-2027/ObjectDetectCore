#pragma once
#include <detectcore/IInferenceEngine.h>
#include "Preprocessor.h"
namespace detectcore {
// Explicitly RAW YOLO26 NCNN one-to-many output: rows=4+nc, cols=N, xywh in model pixels.
std::vector<Detection> postprocessRaw(const Tensor&, const LetterboxResult&, const ModelInfo&, float confidence, float iou);
}
