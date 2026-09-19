#pragma once

#include <QString>
#include <QVector>

// A device identifier is retained separately from the presentation name.
// Windows: DirectShow enumerator order MUST match cv::CAP_DSHOW; the moniker ID
// allows re-resolving an index when USB devices are attached/removed.
// Linux: /dev/videoN is opened by path (not by an unstable positional index).
struct CameraDevice {
    QString name;
    QString id;
    int backendIndex = -1; // Windows/DirectShow only. Never displayed to the user.
};

QVector<CameraDevice> enumerateCameras();
