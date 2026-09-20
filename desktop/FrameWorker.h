#pragma once

#include <QObject>
#include <QImage>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QSet>
#include <atomic>
#include <memory>
#include <mutex>
#include <DetectCore.h>
#include "VideoPlayback.h"

// Live settings are shared with the GUI: atomic scalars + a short locked class set.
// No Qt widgets or OpenCV capture objects are accessed across threads.
struct LiveSettings {
    std::atomic<float> confidence{0.25f};
    std::atomic<float> iou{0.45f};
    std::atomic<int> maxDetections{300};
    std::mutex classesMutex;
    QSet<int> excludedClasses;
};

struct FrameMetrics {
    double preprocessMs = 0.0;
    double inferenceMs = 0.0;
    double postprocessMs = 0.0;
    double totalMs = 0.0;
    double fps = 0.0;
    quint64 dropped = 0;
    int detections = 0;
};
Q_DECLARE_METATYPE(FrameMetrics)

class FrameWorker final : public QObject {
    Q_OBJECT
public:
    enum class Source { Image, Video, Camera };
    struct Request {
        Source source = Source::Image;
        QString path;
        QString cameraId; // Stable device ID from CameraDevices; on Linux a /dev/videoN path.
        int cameraIndex = -1; // Resolved DirectShow position; not exposed to the UI.
        QString modelDirectory;
        int threads = 4;
        int generation = 0;
        std::shared_ptr<LiveSettings> settings;
        std::shared_ptr<VideoPlayback> playback; // Non-null for Video only.
        std::int64_t initialVideoPositionMs = 0;
    };
    FrameWorker(Request request, std::shared_ptr<std::atomic_bool> stop,
                std::shared_ptr<std::atomic_bool> pending);
public slots:
    void run();
signals:
    void frameReady(QImage image, FrameMetrics metrics, int generation);
    void classesReady(QStringList classes, int generation);
    void videoOpened(qint64 durationMs, bool seekable, int generation);
    void videoPosition(qint64 positionMs, int generation);
    void playbackNotice(QString message, int generation);
    void error(QString message, int generation);
    void finished();
private:
    Request req_;
    std::shared_ptr<std::atomic_bool> stop_, pending_;
};
