#pragma once
#include <QObject>
#include <QImage>
#include <QString>
#include <atomic>
#include <memory>
#include <DetectCore.h>
class FrameWorker final : public QObject {
    Q_OBJECT
public:
    enum class Source { Image, Video, Camera };
    struct Request {
        Source source=Source::Image;
        QString path;
        int cameraIndex=0;
        QString modelDirectory;
        float confidence=0.25f, iou=0.45f;
        int threads=4, generation=0;
    };
    FrameWorker(Request request,std::shared_ptr<std::atomic_bool> stop,
                std::shared_ptr<std::atomic_bool> pending);
public slots:
    void run();
signals:
    void frameReady(QImage image, QString stats, int generation);
    void error(QString message, int generation);
    void finished();
private:
    Request req_;
    std::shared_ptr<std::atomic_bool> stop_,pending_;
};
