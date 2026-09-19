#include "CameraDevices.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dshow.h>
#include <memory>

namespace {
template<typename T> struct ComReleaser {
    void operator()(T* p) const noexcept { if (p) p->Release(); }
};
template<typename T> using ComPtr = std::unique_ptr<T, ComReleaser<T>>;

struct ComApartment {
    HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ~ComApartment() { if (SUCCEEDED(result)) CoUninitialize(); }
    bool usable() const { return SUCCEEDED(result) || result == RPC_E_CHANGED_MODE; }
};
}

QVector<CameraDevice> enumerateCameras() {
    QVector<CameraDevice> devices;
    ComApartment com;
    if (!com.usable()) return devices;

    ICreateDevEnum* rawEnum = nullptr;
    if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
            IID_ICreateDevEnum, reinterpret_cast<void**>(&rawEnum)))) return devices;
    ComPtr<ICreateDevEnum> systemEnum(rawEnum);

    IEnumMoniker* rawMonikers = nullptr;
    if (systemEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &rawMonikers, 0) != S_OK)
        return devices;
    ComPtr<IEnumMoniker> monikers(rawMonikers);

    int backendIndex = 0;
    IMoniker* rawMoniker = nullptr;
    while (monikers->Next(1, &rawMoniker, nullptr) == S_OK) {
        ComPtr<IMoniker> moniker(rawMoniker);
        rawMoniker = nullptr;
        QString name;
        IPropertyBag* rawBag = nullptr;
        if (SUCCEEDED(moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag,
                reinterpret_cast<void**>(&rawBag)))) {
            ComPtr<IPropertyBag> bag(rawBag);
            VARIANT value;
            VariantInit(&value);
            if (SUCCEEDED(bag->Read(L"FriendlyName", &value, nullptr)) && value.vt == VT_BSTR)
                name = QString::fromWCharArray(value.bstrVal);
            VariantClear(&value);
        }
        LPOLESTR rawId = nullptr;
        QString id;
        if (SUCCEEDED(moniker->GetDisplayName(nullptr, nullptr, &rawId)) && rawId) {
            id = QString::fromWCharArray(rawId);
            CoTaskMemFree(rawId);
        }
        // Do NOT skip an enumerated moniker when computing the DirectShow index.
        if (!name.trimmed().isEmpty() && !id.isEmpty())
            devices.push_back({name.trimmed(), id, backendIndex});
        ++backendIndex;
    }
    return devices;
}

#elif defined(__linux__)
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>

QVector<CameraDevice> enumerateCameras() {
    QVector<CameraDevice> devices;
    const std::filesystem::path sysfs("/sys/class/video4linux");
    std::error_code ec;
    if (!std::filesystem::is_directory(sysfs, ec)) return devices;
    for (std::filesystem::directory_iterator it(sysfs, ec), end; !ec && it != end; it.increment(ec)) {
        const auto node = it->path().filename().string();
        if (node.rfind("video", 0) != 0) continue;
        const auto path = std::filesystem::path("/dev") / node;
        const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        v4l2_capability caps{};
        const bool queried = ::ioctl(fd, VIDIOC_QUERYCAP, &caps) == 0;
        ::close(fd);
        if (!queried) continue;
        const auto flags = (caps.capabilities & V4L2_CAP_DEVICE_CAPS) ? caps.device_caps : caps.capabilities;
        if (!(flags & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE))) continue;
        std::ifstream label(it->path() / "name");
        std::string name;
        std::getline(label, name);
        if (name.empty()) {
            const char* card = reinterpret_cast<const char*>(caps.card);
            const char* end = std::find(card, card + sizeof(caps.card), '\0');
            name.assign(card, end);
        }
        if (!name.empty()) devices.push_back({QString::fromStdString(name), QString::fromStdString(path.string()), -1});
    }
    std::sort(devices.begin(), devices.end(), [](const CameraDevice& a, const CameraDevice& b) {
        return a.id.localeAwareCompare(b.id) < 0;
    });
    return devices;
}
#else
QVector<CameraDevice> enumerateCameras() { return {}; }
#endif
