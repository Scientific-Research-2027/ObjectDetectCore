cmake_minimum_required(VERSION 3.21)
project(ObjectDetectCore VERSION 0.1.0 LANGUAGES CXX)
option(BUILD_DESKTOP "Build Qt6 Widgets desktop" ON)
option(BUILD_TESTS "Build core regression tests" ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio dnn)
find_package(ncnn CONFIG REQUIRED)
if(TARGET ncnn::ncnn)
    set(NCNN_TARGET ncnn::ncnn)
elseif(TARGET ncnn)
    set(NCNN_TARGET ncnn)
else()
    message(FATAL_ERROR "ncnn package found, but no ncnn::ncnn/ncnn target")
endif()
add_library(detectcore STATIC
    src/core/DetectCore.cpp src/core/ModelConfig.cpp
    src/models/yolo26/Preprocessor.cpp src/models/yolo26/Postprocessor.cpp
    src/models/yolo26/Yolo26Detector.cpp runtime/ncnn/NcnnEngine.cpp)
target_include_directories(detectcore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(detectcore PUBLIC ${OpenCV_LIBS} PRIVATE ${NCNN_TARGET})
if(MSVC)
    target_compile_options(detectcore PRIVATE /W4 /EHsc)
else()
    target_compile_options(detectcore PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(detectcore_cli src/core/DetectCli.cpp)
target_link_libraries(detectcore_cli PRIVATE detectcore)
if(BUILD_DESKTOP)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)
    add_executable(detectcore_desktop WIN32 desktop/main.cpp desktop/MainWindow.cpp desktop/FrameWorker.cpp desktop/CameraDevices.cpp)
    set_target_properties(detectcore_desktop PROPERTIES AUTOMOC ON)
    target_link_libraries(detectcore_desktop PRIVATE detectcore Qt6::Widgets ${OpenCV_LIBS})
    if(WIN32)
        target_link_libraries(detectcore_desktop PRIVATE ole32 oleaut32 strmiids uuid)
    endif()
    if(WIN32)
        target_compile_definitions(detectcore_desktop PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
endif()
if(BUILD_TESTS)
    enable_testing()
    add_executable(detectcore_tests tests/test_core.cpp)
    target_include_directories(detectcore_tests PRIVATE src/models/yolo26)
    target_link_libraries(detectcore_tests PRIVATE detectcore)
    add_test(NAME detectcore_tests COMMAND detectcore_tests)
endif()

# ================================================================
# Windows only: deploy OpenCV + Qt runtime next to the built apps.
# ALL target intentionally runs on EVERY build, including no-op builds.
# Linux/Raspberry Pi 5 never runs windeployqt or copies Windows DLLs.
# ================================================================
option(DETECTCORE_AUTO_DEPLOY_WINDOWS
    "Copy the required Windows runtime DLLs after every build" ON)

if(WIN32 AND BUILD_DESKTOP AND DETECTCORE_AUTO_DEPLOY_WINDOWS)
    # Qt6_DIR comes from the Qt installation used by find_package(Qt6).
    # It is typically <Qt-prefix>/lib/cmake/Qt6.
    get_filename_component(_odc_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
    set(_odc_qt_bin "${_odc_qt_prefix}/bin")
    if(NOT EXISTS "${_odc_qt_bin}/windeployqt.exe")
        message(FATAL_ERROR
            "windeployqt.exe not found in ${_odc_qt_bin}; check Qt6_DIR/CMAKE_PREFIX_PATH")
    endif()

    # Allow an explicit override for OpenCV custom installations.
    # Stock Windows layouts: <OpenCV build>/x64/vcXX/bin, with
    # OpenCV_DIR either <build> or <build>/x64/vcXX/lib.
    set(DETECTCORE_OPENCV_BIN_DIR "" CACHE PATH
        "Directory with OpenCV Windows DLLs (e.g. opencv_world4140.dll)")
    if(NOT DETECTCORE_OPENCV_BIN_DIR)
        file(GLOB _odc_opencv_candidate_bins LIST_DIRECTORIES TRUE
            "${OpenCV_DIR}/x64/vc*/bin")
        get_filename_component(_odc_sibling_bin "${OpenCV_DIR}/../bin" ABSOLUTE)
        list(PREPEND _odc_opencv_candidate_bins
            "${_odc_sibling_bin}" "${OpenCV_DIR}/bin")
        foreach(_odc_candidate IN LISTS _odc_opencv_candidate_bins)
            file(GLOB _odc_candidate_dlls "${_odc_candidate}/opencv_*.dll")
            if(_odc_candidate_dlls)
                set(DETECTCORE_OPENCV_BIN_DIR "${_odc_candidate}" CACHE PATH
                    "Directory with OpenCV Windows DLLs" FORCE)
                break()
            endif()
        endforeach()
    endif()
    if(NOT DETECTCORE_OPENCV_BIN_DIR OR
       NOT IS_DIRECTORY "${DETECTCORE_OPENCV_BIN_DIR}")
        message(FATAL_ERROR
            "Cannot find OpenCV DLL directory near OpenCV_DIR=${OpenCV_DIR}. "
            "Set -DDETECTCORE_OPENCV_BIN_DIR=<OpenCV-bin-directory> explicitly.")
    endif()

    # Optional: NCNN is normally linked statically in this project.
    # If it was installed as a shared library, copy its DLL as well.
    get_filename_component(_odc_ncnn_prefix "${ncnn_DIR}/../../.." ABSOLUTE)
    set(_odc_ncnn_bin "${_odc_ncnn_prefix}/bin")

    set(_odc_deploy_dependencies detectcore_desktop detectcore_cli)
    if(BUILD_TESTS)
        list(APPEND _odc_deploy_dependencies detectcore_tests)
    endif()

    add_custom_target(detectcore_deploy ALL
        COMMAND "${CMAKE_COMMAND}"
            "-DODC_EXE=$<TARGET_FILE:detectcore_desktop>"
            "-DODC_OUT=$<TARGET_FILE_DIR:detectcore_desktop>"
            "-DODC_CONFIGURATION=$<CONFIG>"
            "-DODC_QT_BIN=${_odc_qt_bin}"
            "-DODC_OPENCV_BIN=${DETECTCORE_OPENCV_BIN_DIR}"
            "-DODC_NCNN_BIN=${_odc_ncnn_bin}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployWindows.cmake"
        DEPENDS ${_odc_deploy_dependencies}
        COMMENT "Deploying Qt / OpenCV / optional NCNN DLLs (Windows)"
        VERBATIM)
endif()
