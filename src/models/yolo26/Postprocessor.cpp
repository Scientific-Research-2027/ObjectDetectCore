#include "Postprocessor.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace detectcore {
namespace {
constexpr std::size_t kMaxDetections = 300; // Preserve the existing public core limit.

struct Candidate {
    cv::Rect2f box;
    float score;
    int classId;
    int index; // Stable tie-breaker for reproducible results.
};

// Work in original-image float coordinates: integer rounding before NMS can
// change IoU significantly for small objects (e.g. distant birds or kites).
float intersectionOverUnion(const cv::Rect2f& a, const cv::Rect2f& b) noexcept {
    const float left = std::max(a.x, b.x);
    const float top = std::max(a.y, b.y);
    const float right = std::min(a.x + a.width, b.x + b.width);
    const float bottom = std::min(a.y + a.height, b.y + b.height);
    if (right <= left || bottom <= top) return 0.f;
    const double intersection = static_cast<double>(right - left) * (bottom - top);
    const double areaA = static_cast<double>(a.width) * a.height;
    const double areaB = static_cast<double>(b.width) * b.height;
    const double unionArea = areaA + areaB - intersection;
    return unionArea > 0.0 ? static_cast<float>(intersection / unionArea) : 0.f;
}
} // namespace

std::vector<Detection> postprocessRaw(const Tensor& raw, const LetterboxResult& prep,
                                      const ModelInfo& model, float conf, float iou,
                                      std::vector<BirdKiteEvidence>* evidence) {
    const std::size_t nc = model.names.size();
    if (nc == 0 || nc > static_cast<std::size_t>(std::numeric_limits<int>::max() - 4) ||
        raw.rows != static_cast<int>(nc) + 4 || raw.cols < 1 || raw.cols > 300000 ||
        raw.values.size() != static_cast<std::size_t>(raw.rows) * static_cast<std::size_t>(raw.cols))
        throw std::runtime_error("NCNN output khong khop RAW [4+classes, candidates]");
    if (!std::isfinite(conf) || conf < 0.f || conf > 1.f ||
        !std::isfinite(iou) || iou < 0.f || iou > 1.f)
        throw std::invalid_argument("Threshold khong hop le");
    if (!(prep.scaleX > 0.f) || !(prep.scaleY > 0.f) ||
        !std::isfinite(prep.scaleX) || !std::isfinite(prep.scaleY) ||
        prep.originalWidth <= 0 || prep.originalHeight <= 0)
        throw std::logic_error("Letterbox khong hop le");

    // A diagnostic probe, not an implicit bird->kite classification rule.
    // Matching class names are checked at runtime, allowing custom model order.
    int birdId = -1, kiteId = -1;
    if (evidence) {
        evidence->clear();
        for (std::size_t c = 0; c < nc; ++c) {
            if (model.names[c] == "bird") birdId = static_cast<int>(c);
            if (model.names[c] == "kite") kiteId = static_cast<int>(c);
        }
        if (birdId >= 0 && kiteId >= 0) evidence->reserve(kMaxDetections);
    }

    // NCNN RAW layout: [cx,cy,w,h, class0 ... classN] x candidate count.
    // Access contiguous row pointers instead of bounds-checked Tensor::at()
    // inside the 80 x 8400 hot loop; shapes are validated above.
    const int count = raw.cols;
    const float* const data = raw.values.data();
    const float* const cx = data;
    const float* const cy = data + count;
    const float* const widths = data + 2 * count;
    const float* const heights = data + 3 * count;
    std::vector<Candidate> candidates;
    candidates.reserve(std::min(count, 512));
    const float imageWidth = static_cast<float>(prep.originalWidth);
    const float imageHeight = static_cast<float>(prep.originalHeight);
    for (int n = 0; n < count; ++n) {
        int bestClass = -1;
        float bestScore = conf;
        for (int c = 0; c < static_cast<int>(nc); ++c) {
            const float score = data[(static_cast<std::size_t>(c) + 4) * count + n];
            if (std::isfinite(score) && score >= bestScore && score <= 1.f) {
                bestScore = score;
                bestClass = c;
            }
        }
        if (bestClass < 0 || !std::isfinite(cx[n]) || !std::isfinite(cy[n]) ||
            !std::isfinite(widths[n]) || !std::isfinite(heights[n]) ||
            widths[n] <= 0.f || heights[n] <= 0.f) continue;
        const float x1 = (cx[n] - widths[n] * 0.5f - prep.padLeft) / prep.scaleX;
        const float y1 = (cy[n] - heights[n] * 0.5f - prep.padTop) / prep.scaleY;
        const float x2 = (cx[n] + widths[n] * 0.5f - prep.padLeft) / prep.scaleX;
        const float y2 = (cy[n] + heights[n] * 0.5f - prep.padTop) / prep.scaleY;
        if (!std::isfinite(x1) || !std::isfinite(y1) ||
            !std::isfinite(x2) || !std::isfinite(y2)) continue;
        const float left = std::clamp(x1, 0.f, imageWidth);
        const float top = std::clamp(y1, 0.f, imageHeight);
        const float right = std::clamp(x2, 0.f, imageWidth);
        const float bottom = std::clamp(y2, 0.f, imageHeight);
        if (right <= left || bottom <= top) continue;
        candidates.push_back({cv::Rect2f(left, top, right - left, bottom - top),
                              bestScore, bestClass, n});
    }

    // Highest scores first, independent of class. Once 300 boxes survive,
    // lower-scoring boxes cannot enter the top 300, so NMS work is bounded.
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.index < b.index;
    });
    std::vector<Detection> kept;
    kept.reserve(std::min(kMaxDetections, candidates.size()));
    for (const auto& candidate : candidates) {
        bool suppressed = false;
        for (const auto& selected : kept) {
            if (candidate.classId == selected.classId &&
                intersectionOverUnion(candidate.box, selected.bbox) > iou) {
                suppressed = true;
                break;
            }
        }
        if (suppressed) continue;
        kept.push_back({candidate.classId, model.names[static_cast<std::size_t>(candidate.classId)],
                        candidate.score, candidate.box});
        if (evidence && birdId >= 0 && kiteId >= 0 &&
            (candidate.classId == birdId || candidate.classId == kiteId)) {
            const float birdScore = data[(static_cast<std::size_t>(birdId) + 4) * count + candidate.index];
            const float kiteScore = data[(static_cast<std::size_t>(kiteId) + 4) * count + candidate.index];
            evidence->push_back({candidate.box, candidate.classId, candidate.index,
                                 std::isfinite(birdScore) ? birdScore : -1.f,
                                 std::isfinite(kiteScore) ? kiteScore : -1.f});
        }
        if (kept.size() == kMaxDetections) break;
    }
    return kept;
}
} // namespace detectcore
