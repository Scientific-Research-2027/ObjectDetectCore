#pragma once
#include "FrameWorker.h"
#include <QMainWindow>
#include <QPointer>
#include <QThread>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <memory>
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void start(FrameWorker::Source source, const QString& path={});
    void stop();
    void launch(FrameWorker::Request req);
    QLabel *viewer_=nullptr,*status_=nullptr;
    QPushButton *startButton_=nullptr,*stopButton_=nullptr;
    QDoubleSpinBox *conf_=nullptr,*iou_=nullptr;
    QSpinBox *camera_=nullptr,*threads_=nullptr;
    QString model_,lastPath_;
    FrameWorker::Source lastSource_=FrameWorker::Source::Image;
    QPointer<QThread> thread_;
    std::shared_ptr<std::atomic_bool> stopFlag_,pending_;
    int generation_=0;
    bool closing_=false;
    bool restart_=false;
    FrameWorker::Request next_;
};
