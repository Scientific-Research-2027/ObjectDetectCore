#pragma once

#include "CameraDevices.h"
#include "FrameWorker.h"
#include <QMainWindow>
#include <QWidget>
#include <QPointer>
#include <QThread>
#include <QImage>
#include <QVector>
#include <memory>

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPaintEvent;
class QPushButton;
class QSlider;
class QToolButton;
class QSpinBox;

class ImageView final : public QWidget {
public:
    explicit ImageView(QWidget* parent = nullptr);
    void setImage(QImage image);
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QImage image_;
};

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void refreshCameras();
    void startCamera();
    void start(FrameWorker::Source source, const QString& path = {});
    void stop();
    void launch(FrameWorker::Request request);
    void showMetrics(const FrameMetrics& metrics);
    void showClasses(const QStringList& names, const QString& modelDirectory);
    void clearPresentation();
    void updateCameraButton();
    void refreshWindow();
    void setVideoPaused(bool paused);
    void seekVideo(qint64 positionMs);
    void updateVideoControls();
    void displayVideoPosition(qint64 positionMs);
    void filterClasses(const QString& query);
    void setAllClassesChecked(bool enabled);
    void updateSelectedClassCount();

    ImageView* viewer_ = nullptr;
    QLabel *preprocessLabel_ = nullptr, *inferenceLabel_ = nullptr;
    QLabel *postprocessLabel_ = nullptr, *totalLabel_ = nullptr;
    QLabel *fpsLabel_ = nullptr, *droppedLabel_ = nullptr, *detectionLabel_ = nullptr;
    QLabel* modelLabel_ = nullptr;
    QComboBox* cameraCombo_ = nullptr;
    QPushButton* cameraButton_ = nullptr;
    QWidget* videoBar_ = nullptr;
    QToolButton *playPauseButton_ = nullptr, *videoStopButton_ = nullptr;
    QToolButton *skipBackButton_ = nullptr, *skipForwardButton_ = nullptr;
    QToolButton *videoStartButton_ = nullptr, *videoEndButton_ = nullptr;
    QSlider* videoSlider_ = nullptr;
    QLabel *videoTimeLabel_ = nullptr, *videoDurationLabel_ = nullptr;
    QDoubleSpinBox *conf_ = nullptr, *iou_ = nullptr;
    QSpinBox *maxDetections_ = nullptr, *threads_ = nullptr;
    QWidget* classContent_ = nullptr;
    QLineEdit* classSearch_ = nullptr;
    QLabel *selectedClassCount_ = nullptr, *noMatchingClasses_ = nullptr;
    QPushButton *selectAllClasses_ = nullptr, *clearClasses_ = nullptr;
    QVector<QCheckBox*> classChecks_;
    QVector<CameraDevice> cameraDevices_;
    QStringList classNames_;
    QString classModelDirectory_;
    QString model_, lastPath_;
    FrameWorker::Source lastSource_ = FrameWorker::Source::Image;
    FrameWorker::Source activeSource_ = FrameWorker::Source::Image;
    QPointer<QThread> thread_;
    std::shared_ptr<LiveSettings> settings_ = std::make_shared<LiveSettings>();
    std::shared_ptr<std::atomic_bool> stopFlag_, pending_;
    std::shared_ptr<VideoPlayback> videoPlayback_;
    qint64 videoDurationMs_ = 0, videoPositionMs_ = 0;
    bool videoSeekable_ = false;
    int generation_ = 0;
    bool closing_ = false;
    bool restart_ = false;
    bool hasSource_ = false;
    FrameWorker::Request next_;
};
