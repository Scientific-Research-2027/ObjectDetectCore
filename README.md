# ObjectDetectCore

## 1. Kiểm tra SDK Windows trước khi build (PowerShell)
```powershell
cmake --version
where.exe cl
cl
Get-ChildItem 'C:\Qt\6.11.2' -Directory | Select-Object FullName
Get-ChildItem 'C:\SDK\opencv-4.14.0\opencv\build' -Filter OpenCVConfig.cmake -Recurse | Select-Object FullName
# Thay C:\SDK\ncnn bằng đường dẫn thư mục SDK NCNN mà bạn thực sự đã tải/biên dịch:
Get-ChildItem 'C:\SDK\ncnn' -Filter '*ncnn*config.cmake' -Recurse | Select-Object FullName
```
## 2. Export YOLO26n sang NCNN

Cần internet để cài dependency/tải weights, hoặc dùng `.pt` mà đã có; bước này phải chạy trên máy. Python môi trường riêng:

```powershell
py -3 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install ultralytics ncnn pnnx pyyaml opencv-python numpy
python -m pip freeze > export-requirements-lock.txt
python tools/export_yolo26.py 'D:\ObjectDetectCore\...\models\yolo26n.pt' --imgsz 640
python tools/prepare_ncnn.py 'D:\ObjectDetectCore\...\models\yolo26n_ncnn_model'
```

Đường dẫn `D:\ObjectDetectCore\...` **chỉ là ví dụ**: thay bằng file `.pt` thực tế và thư mục NCNN xuất ra mà script thông báo. `tools/export_yolo26.py` lưu version + export args trong `detectcore-export.json`. `tools/prepare_ncnn.py` đọc `metadata.yaml` và `.param`, lấy tên tensor từ graph **thực**, viết `classes.txt`, `detectcore.cfg`. Không truyền trực tiếp `.pt` vào ứng dụng C++.

## 3. Configure / build / test Windows

- Cấu hình CMake
```powershell
cmake -S . -B build-release `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/msvc2022_64" `
  -DOpenCV_DIR="C:/SDK/opencv-4.14.0/opencv/build/x64/vc16/lib" `
  -Dncnn_DIR="C:/SDK/ncnn/install/lib/cmake/ncnn" `
  -DBUILD_DESKTOP=ON `
  -DBUILD_TESTS=ON `
  -DDETECTCORE_AUTO_DEPLOY_WINDOWS=ON
```

- Complie chương trình
```powershell
cmake --build build-release --config Release --parallel
```

- Kiểm tra tệp thực thi
```powershell
Test-Path ".\build-release\Release\detectcore_desktop.exe"
```
- Thực thi chương trình
```powershell
.\build-release\Release\detectcore_desktop.exe
```

## Sơ đồ thư mục nên cấu hình theo

```text
C:\
├── Qt\6.11.2\msvc2022_64
│
├── SDK/
│   ├── onnxruntime-1.26.0\lib
│   ├── ncnn\build
```