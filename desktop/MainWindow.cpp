#include "MainWindow.h"
#include <QWidget>
#include <QBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QPixmap>
#include <QScrollArea>
#include <QFileInfo>
#include <QDir>
MainWindow::MainWindow(){
    setWindowTitle("ObjectDetectCore MVP | NCNN CPU");resize(1100,730);
    auto* content=new QWidget(this);setCentralWidget(content);
    auto* root=new QVBoxLayout(content);
    auto* bar=new QHBoxLayout();root->addLayout(bar);
    auto make=[&](const QString& text,auto action){
        auto* b=new QPushButton(text,this);bar->addWidget(b);connect(b,&QPushButton::clicked,this,action);return b;
    };
    make("Load Model",[this]{
        QString path=QFileDialog::getExistingDirectory(this,"NCNN model directory",model_);
        if(!path.isEmpty()) {model_=path;status_->setText("Model đã chọn: "+model_+" (được load/kiểm tra ở worker khi Start)");}
    });
    make("Open Image",[this]{
        QString p=QFileDialog::getOpenFileName(this,"Chọn ảnh",{},"Images (*.png *.jpg *.jpeg *.bmp *.webp)");
        if(!p.isEmpty()) start(FrameWorker::Source::Image,p);
    });
    make("Open Camera",[this]{start(FrameWorker::Source::Camera);});
    make("Open Video",[this]{
        QString p=QFileDialog::getOpenFileName(this,"Chọn video",{},"Video (*.mp4 *.avi *.mkv *.mov *.wmv)");
        if(!p.isEmpty())start(FrameWorker::Source::Video,p);
    });
    startButton_=make("Start",[this]{start(lastSource_,lastPath_);});
    stopButton_=make("Stop",[this]{stop();});
    auto* settings=new QHBoxLayout();root->addLayout(settings);
    auto spin=[&](QString label,double start,double min,double max)->QDoubleSpinBox*{
        settings->addWidget(new QLabel(label,this));
        auto* s=new QDoubleSpinBox(this);s->setRange(min,max);s->setSingleStep(0.05);s->setDecimals(2);s->setValue(start);
        settings->addWidget(s);return s;
    };
    conf_=spin("Confidence",0.25,0,1);iou_=spin("IoU (RAW + NMS)",0.45,0,1);
    settings->addWidget(new QLabel("Camera index (không phải tên thiết bị)",this));
    camera_=new QSpinBox(this);camera_->setRange(0,10);settings->addWidget(camera_);
    settings->addWidget(new QLabel("NCNN CPU threads",this));
    threads_=new QSpinBox(this);threads_->setRange(1,32);threads_->setValue(4);settings->addWidget(threads_);
    settings->addStretch();
    viewer_=new QLabel("Chọn thư mục model đã export, sau đó mở ảnh hoặc video/camera",this);
    viewer_->setAlignment(Qt::AlignCenter);viewer_->setMinimumSize(600,420);
    auto* scroll=new QScrollArea(this);scroll->setWidgetResizable(true);scroll->setWidget(viewer_);
    root->addWidget(scroll,1);
    status_=new QLabel("Ready",this);root->addWidget(status_);
}
MainWindow::~MainWindow(){
    if(stopFlag_)stopFlag_->store(true);
    // QThread must not be destroyed while running. closeEvent performs asynchronous-safe draining.
    if(thread_)thread_->wait();
}
void MainWindow::closeEvent(QCloseEvent* event){
    closing_=true;restart_=false;stop();
    if(thread_ && thread_->isRunning()){
        event->ignore();status_->setText("Đang đóng camera và giải phóng worker...");
    }else event->accept();
}
void MainWindow::stop(){if(stopFlag_)stopFlag_->store(true);restart_=false;}
void MainWindow::start(FrameWorker::Source source,const QString& path){
    if(model_.isEmpty()){
        QMessageBox::warning(this,"Model chưa chọn","Chọn thư mục model.ncnn.param/.bin trước.");return;
    }
    if(source!=FrameWorker::Source::Camera && path.isEmpty()){
        status_->setText("Chưa có đường dẫn ảnh/video");return;
    }
    lastSource_=source;lastPath_=path;
    FrameWorker::Request req;
    req.source=source;req.path=path;req.cameraIndex=camera_->value();req.modelDirectory=model_;
    req.confidence=static_cast<float>(conf_->value());req.iou=static_cast<float>(iou_->value());
    req.threads=threads_->value();req.generation=++generation_;
    if(thread_ && thread_->isRunning()){
        next_=req;restart_=true;
        if(stopFlag_)stopFlag_->store(true);
        status_->setText("Đang dừng nguồn cũ để chuyển nguồn...");
        return;
    }
    launch(req);
}
void MainWindow::launch(FrameWorker::Request req){
    stopFlag_=std::make_shared<std::atomic_bool>(false);
    pending_=std::make_shared<std::atomic_bool>(false);
    QThread* t=new QThread(this);thread_=t;
    auto* worker=new FrameWorker(req,stopFlag_,pending_);
    worker->moveToThread(t);
    connect(t,&QThread::started,worker,&FrameWorker::run);
    connect(worker,&FrameWorker::frameReady,this,[this,pending=pending_](const QImage& image,const QString& stats,int generation){
        pending->store(false);
        if(generation!=generation_ || closing_)return;
        viewer_->setPixmap(QPixmap::fromImage(image).scaled(viewer_->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
        status_->setText(stats);
    },Qt::QueuedConnection);
    connect(worker,&FrameWorker::error,this,[this](const QString& message,int generation){
        if(generation==generation_&&!closing_)status_->setText("Error: "+message);
    },Qt::QueuedConnection);
    connect(worker,&FrameWorker::finished,t,&QThread::quit,Qt::QueuedConnection);
    connect(t,&QThread::finished,worker,&QObject::deleteLater);
    connect(t,&QThread::finished,this,[this,t]{
        if(thread_==t)thread_=nullptr;
        t->deleteLater();
        if(closing_){QWidget::close();return;}
        if(restart_){restart_=false;launch(next_);}
    });
    status_->setText("Đang load model và mở nguồn ở worker...");t->start();
}
