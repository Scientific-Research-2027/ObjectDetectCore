#include "FrameWorker.h"
#include <QThread>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
namespace {
std::filesystem::path pathOf(const QString& path) {
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}
cv::Mat openImage(const QString& path) {
    // std::ifstream with std::filesystem::path handles Unicode filenames on Windows.
    std::ifstream stream(pathOf(path),std::ios::binary);
    if(!stream) throw std::runtime_error("Không thể mở ảnh");
    std::vector<uchar> bytes((std::istreambuf_iterator<char>(stream)),{});
    if(bytes.empty()) throw std::runtime_error("Ảnh rỗng");
    return cv::imdecode(bytes,cv::IMREAD_COLOR);
}
QString formatStats(const detectcore::DetectionResult& result,double fps) {
    return QString("FPS: %1 | P: %2 ms | I: %3 ms | O: %4 ms | Boxes: %5")
        .arg(fps,0,'f',1).arg(result.timing.preprocessMs,0,'f',1)
        .arg(result.timing.inferenceMs,0,'f',1).arg(result.timing.postprocessMs,0,'f',1)
        .arg(result.detections.size());
}
QImage drawDetections(const cv::Mat& bgr,const detectcore::DetectionResult& result) {
    cv::Mat out=bgr.clone();
    for(const auto& d:result.detections){
        auto rect=cv::Rect(static_cast<int>(d.bbox.x),static_cast<int>(d.bbox.y),
                           static_cast<int>(d.bbox.width),static_cast<int>(d.bbox.height));
        cv::rectangle(out,rect,cv::Scalar(0,230,0),2);
        std::ostringstream os;os<<d.className<<" "<<std::fixed<<std::setprecision(2)<<d.confidence;
        cv::putText(out,os.str(),{rect.x,std::max(16,rect.y-5)},cv::FONT_HERSHEY_SIMPLEX,
                    0.5,cv::Scalar(0,230,0),1,cv::LINE_AA);
    }
    cv::Mat rgb;cv::cvtColor(out,rgb,cv::COLOR_BGR2RGB);
    return QImage(rgb.data,rgb.cols,rgb.rows,static_cast<qsizetype>(rgb.step),QImage::Format_RGB888).copy();
}
}
FrameWorker::FrameWorker(Request req,std::shared_ptr<std::atomic_bool> stop,
                         std::shared_ptr<std::atomic_bool> pending)
    : req_(std::move(req)),stop_(std::move(stop)),pending_(std::move(pending)) {}
void FrameWorker::run() {
    try{
        detectcore::DetectCore core;
        core.loadModel(pathOf(req_.modelDirectory),req_.threads);
        const detectcore::Options opts{req_.confidence,req_.iou,req_.threads};
        auto process=[&](const cv::Mat& frame,double fps){
            auto result=core.detect(frame,opts);
            if(stop_->load())return;
            if(!pending_->exchange(true))emit frameReady(drawDetections(frame,result),formatStats(result,fps),req_.generation);
        };
        if(req_.source==Source::Image){
            auto image=openImage(req_.path);
            if(image.empty())throw std::runtime_error("Không giải mã được ảnh");
            if(!stop_->load())process(image,0.0);
        } else {
            cv::VideoCapture cap;
            if(req_.source==Source::Camera){
#ifdef _WIN32
                if(!cap.open(req_.cameraIndex,cv::CAP_DSHOW)) cap.open(req_.cameraIndex,cv::CAP_MSMF);
#else
                cap.open(req_.cameraIndex);
#endif
            } else {
                // OpenCV/FFmpeg's support for Unicode video filenames depends on its Windows build.
                cap.open(req_.path.toUtf8().constData());
            }
            if(!cap.isOpened())throw std::runtime_error("Không mở được camera/video; kiểm tra chỉ số hoặc đường dẫn/backend");
            cv::Mat frame;int failures=0;
            auto last=std::chrono::steady_clock::now();
            while(!stop_->load()) {
                if(!cap.read(frame)||frame.empty()){
                    if(req_.source==Source::Video)break; // end-of-file
                    if(++failures>=10)throw std::runtime_error("Camera mất kết nối hoặc không trả frame");
                    QThread::msleep(30);continue;
                }
                failures=0;
                const auto now=std::chrono::steady_clock::now();
                const double elapsed=std::chrono::duration<double>(now-last).count();
                last=now;
                process(frame,elapsed>0?1.0/elapsed:0.0);
            }
            cap.release();
        }
    }catch(const std::exception& e){
        if(!stop_->load())emit error(QString::fromUtf8(e.what()),req_.generation);
    }
    emit finished();
}
