#include "MainWindow.h"
#include <QBoxLayout>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QGridLayout>
#include <QIcon>
#include <QLineEdit>
#include <QMetaType>
#include <QPixmap>
#include <QSizePolicy>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QWidget>
#include <algorithm>
#include <utility>

namespace {
// The main window has a dark Qt style sheet. A native QMessageBox can inherit
// white label text while retaining a white platform dialog background.
void showWarning(QWidget* parent, const QString& title, const QString& message) {
    QMessageBox dialog(QMessageBox::Warning, title, message, QMessageBox::Ok, parent);
    dialog.setStyleSheet(R"QSS(
        QMessageBox { background-color: #192633; }
        QMessageBox QLabel { color: #eaf2fb; background: transparent; }
        QMessageBox QPushButton { color: #ffffff; background-color: #2475e6;
                                 border: 1px solid #3180ef; border-radius: 5px;
                                 padding: 6px 18px; min-width: 48px; }
    )QSS");
    dialog.exec();
}

QStringList missingModelFiles(const QString& directory) {
    const QDir dir(directory);
    QStringList missing;
    for (const auto* name : {"model.ncnn.param", "model.ncnn.bin",
                             "detectcore.cfg", "classes.txt"}) {
        if (!QFileInfo(dir.filePath(QString::fromLatin1(name))).isFile())
            missing.append(QString::fromLatin1(name));
    }
    return missing;
}

// Vector-drawn icons at 2x DPI: no platform-dependent glyphs or file resources.
QIcon toolbarIcon(bool camera, bool refresh) {
    QPixmap pixels(44, 44);
    pixels.fill(Qt::transparent);
    pixels.setDevicePixelRatio(2.0);
    QPainter p(&pixels);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#eaf2fb"), 1.7, Qt::SolidLine,
                  Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if (camera) {
        p.drawRoundedRect(QRectF(2.5, 7.0, 17.0, 11.0), 2.1, 2.1);
        p.drawRoundedRect(QRectF(6.0, 4.5, 6.0, 3.0), 0.7, 0.7);
        p.drawEllipse(QPointF(11, 12.5), 3.0, 3.0);
    }
    if (refresh) {
        if (camera) {
            p.setBrush(QColor("#192633"));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QRectF(11.0, 10.0, 11.0, 11.0));
            p.setPen(QPen(QColor("#63c7ff"), 1.65, Qt::SolidLine,
                          Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(12.3, 11.3, 8.3, 8.3), 45 * 16, 275 * 16);
            p.drawLine(QPointF(19.1, 11.5), QPointF(20.7, 13.1));
            p.drawLine(QPointF(20.7, 13.1), QPointF(18.4, 13.4));
        } else {
            p.drawArc(QRectF(4.5, 4.5, 13, 13), 45 * 16, 290 * 16);
            p.drawLine(QPointF(15.0, 4.8), QPointF(18.0, 5.2));
            p.drawLine(QPointF(18.0, 5.2), QPointF(17.6, 8.2));
        }
    }
    p.end();
    return QIcon(pixels);
}
}

ImageView::ImageView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 260);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
void ImageView::setImage(QImage image) {
    image_ = std::move(image); // Qt implicitly shared image releases previous pixels here.
    update();
}
void ImageView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#14202c"));
    if (image_.isNull()) {
        painter.setPen(QColor("#9eacbc"));
        painter.drawText(rect(), Qt::AlignCenter,
                         tr("Load an NCNN model, then open an image, video or camera"));
        return;
    }
    const QSize fitted = image_.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect dest(QPoint((width() - fitted.width()) / 2, (height() - fitted.height()) / 2), fitted);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(dest, image_);
}

MainWindow::MainWindow() {
    qRegisterMetaType<FrameMetrics>("FrameMetrics");
    setWindowTitle("ObjectDetectCore | NCNN CPU");
    resize(1560, 850);
    setMinimumSize(990, 630);
    setStyleSheet(R"QSS(
        QMainWindow, QWidget#content { background: #192633; color: #eaf2fb; }
        QLabel { color: #eaf2fb; background: transparent; }
        QPushButton { color: white; background: #2475e6; border: 1px solid #3180ef;
                      border-radius: 5px; padding: 9px 14px; font-weight: 600; }
        QPushButton:hover { background: #398bf7; }
        QPushButton:pressed { background: #145bc1; }
        QPushButton:disabled { color: #8894a5; background: #283745; border-color: #364758; }
        QPushButton#refresh { background: #253647; border: 1px solid #344b5f; padding: 0px; }
        QPushButton#refresh:hover { background: #35516a; }
        QLineEdit#classSearch { background: #293846; color: #eaf2fb;
                    border: 1px solid #405367; border-radius: 5px;
                    padding: 7px 8px; selection-background-color: #256fda; }
        QPushButton#classAction { padding: 6px 8px; font-weight: 400; }
        QComboBox, QDoubleSpinBox, QSpinBox { background: #253442; color: #f1f6fa;
                    border: 1px solid #35495a; border-radius: 5px; padding: 7px 9px;
                    min-height: 18px; selection-background-color: #256fda; }
        QComboBox::drop-down { border: 0; width: 22px; }
        QComboBox QAbstractItemView { background: #233241; color: white;
                    border: 1px solid #44607a; selection-background-color: #286dcd; }
        QGroupBox { border: 1px solid #344859; border-radius: 5px; margin-top: 10px;
                    padding: 13px 9px 9px 9px; font-weight: 600; color: #eaf2fb; }
        QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left;
                    left: 10px; padding: 0px 5px; }
        QScrollArea { background: #192633; border: 0; }
        QScrollArea > QWidget > QWidget { background: #192633; }
        QCheckBox { color: #dce8f5; spacing: 7px; padding: 3px; }
        QStatusBar { background: #16232e; color: #eaf2fb; border-top: 1px solid #2a3c4e; }
        QStatusBar::item { border: 0; }
        QScrollBar:vertical { background: #1a2a38; width: 10px; }
        QScrollBar::handle:vertical { background: #486178; border-radius: 4px; min-height: 30px; }
    )QSS");

    auto* content = new QWidget(this);
    content->setObjectName("content");
    setCentralWidget(content);
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(10, 7, 10, 5);
    root->setSpacing(11);

    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(12);
    toolbar->addStretch();
    auto addButton = [&](const QString& label, auto action) {
        auto* button = new QPushButton(label, content);
        toolbar->addWidget(button);
        connect(button, &QPushButton::clicked, this, action);
        return button;
    };
    addButton("Load model", [this] {
        const QString selected = QFileDialog::getExistingDirectory(this, "NCNN model directory", model_);
        if (selected.isEmpty()) return;
        const QStringList missing = missingModelFiles(selected);
        if (!missing.isEmpty()) {
            showWarning(this, "Invalid NCNN model directory",
                        "The selected directory is missing:\n" + missing.join("\n") +
                        "\n\nSelect the folder containing all four model files "
                        "(for example, models/yolo26n_ncnn_model).");
            return; // Preserve the previously selected, potentially working model.
        }
        model_ = selected;
        modelLabel_->setText("Selected: " + QFileInfo(model_).fileName());
        statusBar()->showMessage("Model selected; applied on the next start");
    });
    auto* refreshWindowButton = addButton("Refresh Window", [this] { refreshWindow(); });
    refreshWindowButton->setIcon(toolbarIcon(false, true));
    refreshWindowButton->setIconSize(QSize(19, 19));
    refreshWindowButton->setToolTip("Reprocess the image or restart the current video/camera worker");
    addButton("Open image...", [this] {
        const QString path = QFileDialog::getOpenFileName(this, "Select image", {},
                                                          "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
        if (!path.isEmpty()) start(FrameWorker::Source::Image, path);
    });
    addButton("Open Video", [this] {
        const QString path = QFileDialog::getOpenFileName(this, "Select video", {},
                                                          "Videos (*.mp4 *.avi *.mkv *.mov *.wmv)");
        if (!path.isEmpty()) start(FrameWorker::Source::Video, path);
    });
    cameraCombo_ = new QComboBox(content);
    cameraCombo_->setMinimumWidth(130);
    cameraCombo_->setMaximumWidth(270);
    cameraCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    cameraCombo_->setMinimumContentsLength(13);
    cameraCombo_->setToolTip("Camera device name (not OpenCV camera index)");
    toolbar->addWidget(cameraCombo_);
    auto* refresh = addButton("", [this] { refreshCameras(); });
    refresh->setObjectName("refresh");
    refresh->setIcon(toolbarIcon(true, true));
    refresh->setIconSize(QSize(22, 22));
    refresh->setFixedSize(40, 37);
    refresh->setAccessibleName("Refresh cameras");
    refresh->setToolTip("Refresh connected cameras");
    cameraButton_ = addButton("Start camera", [this] { startCamera(); });
    cameraButton_->setIcon(toolbarIcon(true, false));
    cameraButton_->setIconSize(QSize(19, 19));
    root->addLayout(toolbar);
    connect(cameraCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if (thread_ && !restart_ && !closing_ && stopFlag_ && !stopFlag_->load() &&
                    activeSource_ == FrameWorker::Source::Camera &&
                    !cameraCombo_->currentData().toString().isEmpty())
                    start(FrameWorker::Source::Camera); // Switch camera without opening another capture concurrently.
            });

    auto* workspace = new QHBoxLayout;
    workspace->setSpacing(14);
    viewer_ = new ImageView(content);
    workspace->addWidget(viewer_, 1);

    auto* panel = new QWidget(content);
    panel->setFixedWidth(294);
    auto* right = new QVBoxLayout(panel);
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(9);
    workspace->addWidget(panel);
    root->addLayout(workspace, 1);

    auto* settingsBox = new QGroupBox("Detection settings", panel);
    auto* form = new QGridLayout(settingsBox);
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(6);
    form->setContentsMargins(7, 9, 7, 8);
    auto makeFloat = [&](int row, const QString& name, double value) {
        form->addWidget(new QLabel(name, settingsBox), row, 0);
        auto* widget = new QDoubleSpinBox(settingsBox);
        widget->setRange(0.0, 1.0);
        widget->setSingleStep(0.05);
        widget->setDecimals(2);
        widget->setValue(value);
        form->addWidget(widget, row, 1);
        return widget;
    };
    conf_ = makeFloat(0, "Confidence", 0.25);
    iou_ = makeFloat(1, "IoU", 0.45);
    form->addWidget(new QLabel("Max detections", settingsBox), 2, 0);
    maxDetections_ = new QSpinBox(settingsBox);
    // The current detection core caps post-NMS results at 300; higher UI
    // values would be misleading until that backend limit is changed.
    maxDetections_->setRange(1, 300);
    maxDetections_->setValue(300);
    form->addWidget(maxDetections_, 2, 1);
    form->setColumnStretch(1, 1);
    right->addWidget(settingsBox);
    connect(conf_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { settings_->confidence.store(static_cast<float>(value)); });
    connect(iou_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { settings_->iou.store(static_cast<float>(value)); });
    connect(maxDetections_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int value) { settings_->maxDetections.store(value); });

    auto* metricsBox = new QGroupBox("Live metrics", panel);
    auto* metricLayout = new QVBoxLayout(metricsBox);
    metricLayout->setContentsMargins(9, 8, 9, 7);
    metricLayout->setSpacing(2);
    auto addMetric = [&](const QString& name) {
        auto* line = new QHBoxLayout;
        auto* title = new QLabel(name, metricsBox);
        auto* value = new QLabel("--", metricsBox);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        line->addWidget(title);
        line->addStretch();
        line->addWidget(value);
        metricLayout->addLayout(line);
        return value;
    };
    preprocessLabel_ = addMetric("Preprocess");
    inferenceLabel_ = addMetric("Inference");
    postprocessLabel_ = addMetric("Postprocess");
    totalLabel_ = addMetric("Total");
    fpsLabel_ = addMetric("Throughput");
    droppedLabel_ = addMetric("Dropped");
    detectionLabel_ = new QLabel("Detections: 0", metricsBox);
    detectionLabel_->setStyleSheet("color: #3cc8f4; font-size: 16px; font-weight: 700; margin-top: 7px;");
    metricLayout->addWidget(detectionLabel_);
    right->addWidget(metricsBox);

    auto* filterBox = new QGroupBox("Class filter", panel);
    auto* filterLayout = new QVBoxLayout(filterBox);
    filterLayout->setContentsMargins(5, 5, 5, 5);
    filterLayout->setSpacing(6);
    classSearch_ = new QLineEdit(filterBox);
    classSearch_->setObjectName("classSearch");
    classSearch_->setPlaceholderText("Search classes...");
    classSearch_->setClearButtonEnabled(true);
    filterLayout->addWidget(classSearch_);
    connect(classSearch_, &QLineEdit::textChanged, this,
            [this](const QString& query) { filterClasses(query); });

    auto* classActions = new QHBoxLayout;
    classActions->setSpacing(5);
    selectAllClasses_ = new QPushButton("Select all", filterBox);
    clearClasses_ = new QPushButton("Clear", filterBox);
    for (auto* button : {selectAllClasses_, clearClasses_}) {
        button->setObjectName("classAction");
        button->setEnabled(false);
        classActions->addWidget(button);
    }
    selectedClassCount_ = new QLabel("0 selected", filterBox);
    selectedClassCount_->setStyleSheet("color: #b6c4d2; font-size: 12px;");
    classActions->addWidget(selectedClassCount_);
    classActions->addStretch();
    filterLayout->addLayout(classActions);
    connect(selectAllClasses_, &QPushButton::clicked, this,
            [this] { setAllClassesChecked(true); });
    connect(clearClasses_, &QPushButton::clicked, this,
            [this] { setAllClassesChecked(false); });
    auto* scroll = new QScrollArea(filterBox);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: 1px solid #344859; border-radius: 4px; }");
    classContent_ = new QWidget(scroll);
    auto* classLayout = new QVBoxLayout(classContent_);
    classLayout->setSpacing(1);
    classLayout->setContentsMargins(4, 4, 4, 4);
    classLayout->addWidget(new QLabel("Load a model to show classes", classContent_));
    classLayout->addStretch();
    scroll->setWidget(classContent_);
    filterLayout->addWidget(scroll);
    right->addWidget(filterBox, 1);

    // CPU worker count is kept accessible without changing the screenshot's main panel.
    threads_ = new QSpinBox(panel);
    threads_->setRange(1, 32);
    threads_->setValue(4);
    threads_->setToolTip("NCNN CPU threads (used on next start)");
    threads_->setVisible(false);
    modelLabel_ = new QLabel("No model selected", this);
    modelLabel_->setStyleSheet("color: #aabccb; padding-right: 10px;");
    statusBar()->showMessage("Ready");
    statusBar()->addPermanentWidget(modelLabel_);
    refreshCameras();
}

MainWindow::~MainWindow() {
    if (stopFlag_) stopFlag_->store(true);
    // Normally closeEvent defers destruction until QThread::finished.
    if (thread_) thread_->wait();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    closing_ = true;
    restart_ = false;
    if (stopFlag_) stopFlag_->store(true);
    if (thread_) {
        event->ignore();
        statusBar()->showMessage("Closing capture and releasing model...");
    } else {
        event->accept();
    }
}

void MainWindow::refreshCameras() {
    const QString previous = cameraCombo_->currentData().toString();
    cameraDevices_ = enumerateCameras();
    const QSignalBlocker block(cameraCombo_);
    cameraCombo_->clear();
    int selected = -1;
    for (int i = 0; i < cameraDevices_.size(); ++i) {
        const auto& d = cameraDevices_[i];
        QString label = d.name;
        // Duplicate human-readable names are disambiguated without showing OpenCV indices.
        int duplicate = 0;
        for (int j = 0; j <= i; ++j) if (cameraDevices_[j].name == d.name) ++duplicate;
        if (duplicate > 1) label += QString(" (%1)").arg(duplicate);
        cameraCombo_->addItem(label, d.id);
        cameraCombo_->setItemData(i, d.name, Qt::ToolTipRole);
        if (d.id == previous) selected = i;
    }
    if (cameraDevices_.isEmpty()) {
        cameraCombo_->addItem("No camera connected", QString());
        cameraCombo_->setEnabled(false);
    } else {
        cameraCombo_->setEnabled(true);
        cameraCombo_->setCurrentIndex(selected < 0 ? 0 : selected);
    }
    updateCameraButton();
}

void MainWindow::startCamera() {
    if (thread_ && !restart_ && activeSource_ == FrameWorker::Source::Camera &&
        stopFlag_ && !stopFlag_->load()) {
        stop();
        return;
    }
    // Re-query before opening: hot-plug may have moved DirectShow enumeration slots.
    refreshCameras();
    if (cameraCombo_->currentData().toString().isEmpty()) {
        statusBar()->showMessage("No camera available; connect a device and refresh");
        return;
    }
    start(FrameWorker::Source::Camera);
}

void MainWindow::start(FrameWorker::Source source, const QString& path) {
    if (closing_) return;
    if (model_.isEmpty()) {
        showWarning(this, "Model not selected",
                    "Choose the NCNN model directory first.\n\n"
                    "Use Load model and select the folder containing "
                    "model.ncnn.param, model.ncnn.bin, detectcore.cfg and classes.txt.");
        return;
    }
    const QStringList missing = missingModelFiles(model_);
    if (!missing.isEmpty()) {
        showWarning(this, "NCNN model files missing",
                    "The selected model directory is missing:\n" + missing.join("\n") +
                    "\n\nSelect a complete model using Load model.");
        return;
    }
    if (source != FrameWorker::Source::Camera && path.isEmpty()) {
        statusBar()->showMessage("Choose an image or video first");
        return;
    }
    FrameWorker::Request req;
    req.source = source;
    req.path = path;
    req.modelDirectory = model_;
    req.threads = threads_->value();
    req.generation = ++generation_;
    req.settings = settings_;
    if (source == FrameWorker::Source::Camera) {
        const QString wanted = cameraCombo_->currentData().toString();
        const auto found = std::find_if(cameraDevices_.cbegin(), cameraDevices_.cend(),
                         [&](const CameraDevice& d) { return d.id == wanted; });
        if (found == cameraDevices_.cend()) {
            statusBar()->showMessage("Selected camera has been disconnected");
            return;
        }
        req.cameraId = found->id;
        req.cameraIndex = found->backendIndex;
    }
    lastSource_ = source;
    lastPath_ = path;
    hasSource_ = true;
    if (thread_) {
        next_ = std::move(req);
        restart_ = true;
        if (stopFlag_) stopFlag_->store(true);
        statusBar()->showMessage("Switching source; closing the previous capture...");
        updateCameraButton();
        return;
    }
    launch(std::move(req));
}

void MainWindow::stop() {
    restart_ = false;
    if (stopFlag_) stopFlag_->store(true);
    statusBar()->showMessage("Stopping capture and releasing model...");
    updateCameraButton();
}

void MainWindow::refreshWindow() {
    // GUI repaints only while the event loop is responsive. Restart the worker
    // asynchronously so the next result reflects current live settings; the
    // existing launch/restart path releases the old capture before reopening.
    viewer_->update();
    if (closing_) return;
    if (!hasSource_) {
        statusBar()->showMessage("Window refreshed; open an image, video or camera first");
        return;
    }
    if (lastSource_ == FrameWorker::Source::Camera &&
        cameraCombo_->currentData().toString().isEmpty()) {
        statusBar()->showMessage("Camera disconnected; refresh the camera list first");
        return;
    }
    statusBar()->showMessage("Refreshing detection and releasing the previous worker...");
    start(lastSource_, lastPath_);
}

void MainWindow::updateCameraButton() {
    if (!cameraButton_) return;
    const bool active = thread_ && !restart_ && activeSource_ == FrameWorker::Source::Camera &&
                        stopFlag_ && !stopFlag_->load();
    cameraButton_->setText(active ? "Stop camera" : "Start camera");
    cameraButton_->setEnabled(active || !cameraDevices_.empty());
}

void MainWindow::launch(FrameWorker::Request req) {
    if (classModelDirectory_ != req.modelDirectory) {
        // Do this only AFTER the previous worker has exited. Otherwise a
        // new model can process its first (possibly only) frame using class
        // exclusions belonging to a different model's class-ID mapping.
        clearPresentation();
        showClasses({}, req.modelDirectory);
    }
    stopFlag_ = std::make_shared<std::atomic_bool>(false);
    pending_ = std::make_shared<std::atomic_bool>(false);
    activeSource_ = req.source;
    auto* thread = new QThread(this);
    thread_ = thread;
    const QString modelDirectory = req.modelDirectory;
    auto* worker = new FrameWorker(std::move(req), stopFlag_, pending_);
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &FrameWorker::run);
    connect(worker, &FrameWorker::frameReady, this,
            [this, pending = pending_](QImage image, FrameMetrics metrics, int generation) {
                pending->store(false);
                if (generation != generation_ || closing_ || (stopFlag_ && stopFlag_->load())) return;
                viewer_->setImage(std::move(image));
                showMetrics(metrics);
                statusBar()->showMessage("Ready");
            }, Qt::QueuedConnection);
    connect(worker, &FrameWorker::classesReady, this,
            [this, modelDirectory](const QStringList& names, int generation) {
                if (generation == generation_ && !closing_)
                    showClasses(names, modelDirectory);
            }, Qt::QueuedConnection);
    connect(worker, &FrameWorker::error, this,
            [this, modelDirectory](const QString& message, int generation) {
                if (generation == generation_ && !closing_) {
                    clearPresentation(); // Never show old boxes as a failed run's result.
                    if (classModelDirectory_ != modelDirectory)
                        showClasses({}, modelDirectory);
                    statusBar()->showMessage("Error: " + message);
                }
            }, Qt::QueuedConnection);
    connect(worker, &FrameWorker::finished, thread, &QThread::quit, Qt::QueuedConnection);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread] {
        if (thread_ == thread) thread_ = nullptr;
        thread->deleteLater();
        if (closing_) { QWidget::close(); return; }
        if (restart_) {
            restart_ = false;
            launch(std::move(next_));
        } else {
            updateCameraButton();
        }
    });
    statusBar()->showMessage("Loading model and opening source in worker...");
    updateCameraButton();
    thread->start();
}

void MainWindow::showMetrics(const FrameMetrics& m) {
    const auto ms = [](double d) { return QString::number(d, 'f', 2) + " ms"; };
    preprocessLabel_->setText(ms(m.preprocessMs));
    inferenceLabel_->setText(ms(m.inferenceMs));
    postprocessLabel_->setText(ms(m.postprocessMs));
    totalLabel_->setText(ms(m.totalMs));
    fpsLabel_->setText(QString::number(m.fps, 'f', 1) + " FPS");
    droppedLabel_->setText(QString::number(m.dropped));
    detectionLabel_->setText(QString("Detections: %1").arg(m.detections));
}

void MainWindow::clearPresentation() {
    viewer_->setImage({});
    for (auto* metric : {preprocessLabel_, inferenceLabel_, postprocessLabel_,
                         totalLabel_, fpsLabel_, droppedLabel_})
        metric->setText("--");
    detectionLabel_->setText("Detections: 0");
}

void MainWindow::showClasses(const QStringList& names, const QString& modelDirectory) {
    // Class IDs refer to a particular model. Identical class names in another
    // model directory must not silently inherit exclusions from the old model.
    if (classNames_ == names && classModelDirectory_ == modelDirectory) return;
    classNames_ = names;
    classModelDirectory_ = modelDirectory;
    {
        std::scoped_lock lock(settings_->classesMutex);
        settings_->excludedClasses.clear();
    }
    auto* layout = static_cast<QVBoxLayout*>(classContent_->layout());
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (auto* widget = item->widget()) delete widget;
        delete item;
    }
    classChecks_.clear();
    noMatchingClasses_ = nullptr;
    selectAllClasses_->setEnabled(!names.isEmpty());
    clearClasses_->setEnabled(!names.isEmpty());
    if (names.isEmpty()) {
        layout->addWidget(new QLabel("No classes", classContent_));
    } else {
        for (int id = 0; id < names.size(); ++id) {
            // Display zero-based model class IDs, e.g. "33  kite".
            auto* item = new QCheckBox(QString("%1  %2").arg(id).arg(names.at(id)), classContent_);
            item->setChecked(true);
            item->setToolTip(QString("Class ID: %1").arg(id));
            layout->addWidget(item);
            classChecks_.push_back(item);
            connect(item, &QCheckBox::toggled, this, [this, id](bool enabled) {
                {
                    std::scoped_lock lock(settings_->classesMutex);
                    if (enabled) settings_->excludedClasses.remove(id);
                    else settings_->excludedClasses.insert(id);
                }
                updateSelectedClassCount();
            });
        }
        noMatchingClasses_ = new QLabel("No matching classes", classContent_);
        noMatchingClasses_->setStyleSheet("color: #9eacbc; padding: 7px;");
        layout->addWidget(noMatchingClasses_);
    }
    layout->addStretch();
    filterClasses(classSearch_->text());
    updateSelectedClassCount();
}

void MainWindow::filterClasses(const QString& query) {
    const QString search = query.trimmed();
    bool anyMatch = false;
    for (int id = 0; id < classChecks_.size(); ++id) {
        const bool matches = search.isEmpty() ||
            classNames_.at(id).contains(search, Qt::CaseInsensitive) ||
            QString::number(id).contains(search, Qt::CaseInsensitive);
        classChecks_[id]->setVisible(matches);
        anyMatch |= matches;
    }
    if (noMatchingClasses_) noMatchingClasses_->setVisible(!anyMatch);
}

void MainWindow::setAllClassesChecked(bool enabled) {
    // Bulk changes use one short lock and suppress per-widget notifications.
    {
        std::scoped_lock lock(settings_->classesMutex);
        settings_->excludedClasses.clear();
        if (!enabled) {
            for (int id = 0; id < classChecks_.size(); ++id)
                settings_->excludedClasses.insert(id);
        }
    }
    for (auto* item : classChecks_) {
        const QSignalBlocker blocked(item);
        item->setChecked(enabled);
    }
    updateSelectedClassCount();
}

void MainWindow::updateSelectedClassCount() {
    const int count = static_cast<int>(std::count_if(
        classChecks_.cbegin(), classChecks_.cend(),
        [](const QCheckBox* item) { return item->isChecked(); }));
    selectedClassCount_->setText(QString("%1 selected").arg(count));
}
