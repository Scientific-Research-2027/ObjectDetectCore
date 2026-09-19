#include "FrameWorker.h"
#include <QThread>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
std::filesystem::path pathOf(const QString& path) {
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}
cv::Mat openImage(const QString& path) {
    // Unicode filenames are supported on Windows without changing OpenCV's global locale.
    std::ifstream stream(pathOf(path), std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("Không thể mở ảnh");
    const std::streamoff size = stream.tellg();
    if (size <= 0) throw std::runtime_error("Ảnh rỗng hoặc không xác định được kích thước");
    if (static_cast<std::uintmax_t>(size) >
        static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
        size > std::numeric_limits<std::streamsize>::max())
        throw std::runtime_error("Kích thước file ảnh vượt giới hạn bộ nhớ");
    // Allocate exactly once instead of repeatedly growing the encoded input
    // buffer (which temporarily retains old allocations for large images).
    std::vector<uchar> bytes(static_cast<std::size_t>(size));
    stream.seekg(0, std::ios::beg);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Không đọc được toàn bộ dữ liệu ảnh");
    return cv::imdecode(bytes, cv::IMREAD_COLOR);
}

QImage drawDetections(const cv::Mat& bgr,
                      const std::vector<const detectcore::Detection*>& detections,
                      cv::Mat& previewScratch, std::string& labelScratch) {
    // Inference uses the ORIGINAL frame. Bound only the GUI preview: an 8K
    // camera would otherwise allocate an additional ~100 MB QImage per frame.
    // The resize destination is reused by this worker across frames.
    constexpr int maxPreviewSide = 1920;
    const cv::Mat* preview = &bgr;
    if (std::max(bgr.cols, bgr.rows) > maxPreviewSide) {
        const double ratio = static_cast<double>(maxPreviewSide) /
                             static_cast<double>(std::max(bgr.cols, bgr.rows));
        const cv::Size previewSize(std::max(1, static_cast<int>(std::lround(bgr.cols * ratio))),
                                   std::max(1, static_cast<int>(std::lround(bgr.rows * ratio))));
        cv::resize(bgr, previewScratch, previewSize, 0.0, 0.0, cv::INTER_AREA);
        preview = &previewScratch;
    }
    // One owned RGB buffer per presented frame, reused scratch only for
    // oversized previews. The temporary cv::Mat below never owns QImage data.
    QImage image(preview->cols, preview->rows, QImage::Format_RGB888);
    if (image.isNull()) throw std::bad_alloc();
    cv::Mat out(preview->rows, preview->cols, CV_8UC3, image.bits(),
                static_cast<size_t>(image.bytesPerLine()));
    cv::cvtColor(*preview, out, cv::COLOR_BGR2RGB);
    const float sx = static_cast<float>(out.cols) / static_cast<float>(bgr.cols);
    const float sy = static_cast<float>(out.rows) / static_cast<float>(bgr.rows);
    for (const auto* d : detections) {
        const cv::Rect2f box(d->bbox.x * sx, d->bbox.y * sy,
                             d->bbox.width * sx, d->bbox.height * sy);
        const cv::Rect rect = cv::Rect(box) & cv::Rect(0, 0, out.cols, out.rows);
        if (rect.empty()) continue;
        cv::rectangle(out, rect, cv::Scalar(85, 205, 0), 2);
        char confidenceText[32];
        std::snprintf(confidenceText, sizeof(confidenceText), " %.2f", d->confidence);
        labelScratch.assign(d->className, 0, std::min(d->className.size(), std::size_t{96}));
        labelScratch.append(confidenceText);
        int base = 0;
        const auto size = cv::getTextSize(labelScratch, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &base);
        const int tx = std::clamp(rect.x, 0, std::max(0, out.cols - size.width - 7));
        const int ty = std::max(rect.y - 5, size.height + 5);
        cv::rectangle(out, cv::Rect(tx, ty - size.height - 4,
                      std::min(size.width + 7, out.cols - tx), size.height + base + 7),
                      cv::Scalar(37, 49, 66), cv::FILLED);
        cv::putText(out, labelScratch, {tx + 3, ty}, cv::FONT_HERSHEY_SIMPLEX,
                    0.5, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    }
    // cv::Mat is only a temporary non-owning view; QImage retains its pixels.
    return image;
}
}

FrameWorker::FrameWorker(Request req, std::shared_ptr<std::atomic_bool> stop,
                         std::shared_ptr<std::atomic_bool> pending)
    : req_(std::move(req)), stop_(std::move(stop)), pending_(std::move(pending)) {}

void FrameWorker::run() {
    try {
        if (stop_->load()) { emit finished(); return; }
        detectcore::DetectCore core;
        core.loadModel(pathOf(req_.modelDirectory), req_.threads);
        if (stop_->load()) { emit finished(); return; }
        QStringList classes;
        for (const auto& name : core.modelInfo().names)
            classes.push_back(QString::fromStdString(name));
        emit classesReady(classes, req_.generation);

        quint64 dropped = 0;
        std::vector<const detectcore::Detection*> selected; // Reuse capacity across frames.
        cv::Mat previewScratch;
        std::string labelScratch;
        labelScratch.reserve(128);
        const auto process = [&](const cv::Mat& frame, double fps) {
            if (stop_->load()) return;
            const auto begin = Clock::now();
            const detectcore::Options options{req_.settings->confidence.load(),
                                               req_.settings->iou.load(), req_.threads};
            auto result = core.detect(frame, options);
            if (stop_->load()) return;
            // Only one owned image may wait on the GUI event queue at a time.
            if (pending_->exchange(true)) { ++dropped; return; }
            selected.clear();
            selected.reserve(result.detections.size());
            {
                std::scoped_lock lock(req_.settings->classesMutex);
                for (const auto& d : result.detections)
                    if (!req_.settings->excludedClasses.contains(d.classId))
                        selected.push_back(&d);
            }
            const int limit = req_.settings->maxDetections.load();
            if (static_cast<int>(selected.size()) > limit) {
                std::partial_sort(selected.begin(), selected.begin() + limit, selected.end(),
                    [](const auto* a, const auto* b) { return a->confidence > b->confidence; });
                selected.resize(limit);
            }
            auto image = drawDetections(frame, selected, previewScratch, labelScratch);
            if (stop_->load()) { pending_->store(false); return; }
            const double total = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
            FrameMetrics metrics{result.timing.preprocessMs, result.timing.inferenceMs,
                                 result.timing.postprocessMs, total, fps, dropped,
                                 static_cast<int>(selected.size())};
            emit frameReady(std::move(image), metrics, req_.generation);
        };

        if (req_.source == Source::Image) {
            auto image = openImage(req_.path);
            if (image.empty()) throw std::runtime_error("Không giải mã được ảnh");
            process(image, 0.0);
        } else {
            cv::VideoCapture cap;
            if (req_.source == Source::Camera) {
#ifdef _WIN32
                // Never fall back to CAP_MSMF with a CAP_DSHOW index: it may
                // silently open another physical camera with the wrong name.
                if (req_.cameraIndex < 0 || req_.cameraId.isEmpty() ||
                    !cap.open(req_.cameraIndex, cv::CAP_DSHOW))
                    throw std::runtime_error("Không thể mở camera bằng DirectShow");
#elif defined(__linux__)
                if (!req_.cameraId.startsWith("/dev/video") ||
                    !cap.open(req_.cameraId.toStdString(), cv::CAP_V4L2))
                    throw std::runtime_error("Không thể mở thiết bị camera V4L2");
#else
                throw std::runtime_error("Chưa hỗ trợ liệt kê camera trên hệ điều hành này");
#endif
            } else {
                // OpenCV/FFmpeg Unicode path support varies with the linked Windows build.
                cap.open(req_.path.toUtf8().constData());
            }
            if (!cap.isOpened()) throw std::runtime_error("Không thể mở camera/video");
            cv::Mat frame;
            int failures = 0;
            auto last = Clock::now();
            while (!stop_->load()) {
                if (!cap.read(frame) || frame.empty()) {
                    if (req_.source == Source::Video) break;
                    if (++failures >= 10) throw std::runtime_error("Camera ngắt kết nối hoặc không có frame");
                    QThread::msleep(30);
                    continue;
                }
                failures = 0;
                const auto now = Clock::now();
                const double seconds = std::chrono::duration<double>(now - last).count();
                last = now;
                process(frame, seconds > 0.0 ? 1.0 / seconds : 0.0);
            }
            cap.release(); // Also released automatically on exceptions (RAII).
        }
    } catch (const std::exception& e) {
        pending_->store(false);
        if (!stop_->load()) emit error(QString::fromUtf8(e.what()), req_.generation);
    }
    emit finished();
}
