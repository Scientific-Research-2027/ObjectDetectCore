# Invoked by CMake during Windows builds, not during configure.
foreach(_required ODC_EXE ODC_OUT ODC_CONFIGURATION ODC_QT_BIN ODC_OPENCV_BIN)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "Missing required -D${_required}")
    endif()
endforeach()
if(NOT EXISTS "${ODC_EXE}")
    message(FATAL_ERROR "Desktop executable does not exist: ${ODC_EXE}")
endif()
if(NOT EXISTS "${ODC_QT_BIN}/windeployqt.exe")
    message(FATAL_ERROR "Qt deployment tool does not exist: ${ODC_QT_BIN}/windeployqt.exe")
endif()
if(NOT IS_DIRECTORY "${ODC_OPENCV_BIN}")
    message(FATAL_ERROR "OpenCV DLL folder does not exist: ${ODC_OPENCV_BIN}")
endif()

# Select only the DLLs matching the actual build configuration.
# VideoIO FFmpeg plugins have a separate name and may lack a Debug 'd' suffix.
file(GLOB _opencv_all_dlls "${ODC_OPENCV_BIN}/opencv_*.dll")
set(_opencv_selected "")
set(_opencv_primary_found FALSE)
foreach(_dll IN LISTS _opencv_all_dlls)
    get_filename_component(_dll_name "${_dll}" NAME)
    if(ODC_CONFIGURATION STREQUAL "Debug")
        if(_dll_name MATCHES "d\\.dll$" OR
           _dll_name MATCHES "^opencv_videoio_ffmpeg.*\\.dll$")
            list(APPEND _opencv_selected "${_dll}")
            if(_dll_name MATCHES "^opencv_(world|core)[0-9]+d\\.dll$")
                set(_opencv_primary_found TRUE)
            endif()
        endif()
    else()
        if(NOT _dll_name MATCHES "d\\.dll$")
            list(APPEND _opencv_selected "${_dll}")
            if(_dll_name MATCHES "^opencv_(world|core)[0-9]+\\.dll$")
                set(_opencv_primary_found TRUE)
            endif()
        endif()
    endif()
endforeach()
if(NOT _opencv_primary_found)
    message(FATAL_ERROR
        "Could not find a matching OpenCV ${ODC_CONFIGURATION} world/core DLL "
        "in ${ODC_OPENCV_BIN}. Do not mix OpenCV Debug and Release.")
endif()

# DLLs are placed next to desktop, CLI, and tests, whose default output
# directory is shared for this project's Visual Studio generator.
file(COPY ${_opencv_selected} DESTINATION "${ODC_OUT}")

if(DEFINED ODC_NCNN_BIN AND IS_DIRECTORY "${ODC_NCNN_BIN}")
    file(GLOB _ncnn_dlls "${ODC_NCNN_BIN}/ncnn*.dll")
    foreach(_dll IN LISTS _ncnn_dlls)
        get_filename_component(_name "${_dll}" NAME)
        if(ODC_CONFIGURATION STREQUAL "Debug")
            if(_name MATCHES "d\\.dll$")
                file(COPY "${_dll}" DESTINATION "${ODC_OUT}")
            endif()
        else()
            if(NOT _name MATCHES "d\\.dll$")
                file(COPY "${_dll}" DESTINATION "${ODC_OUT}")
            endif()
        endif()
    endforeach()
endif()

# windeployqt is the supported way to deploy Qt plugins (platforms/qwindows.dll).
# PATH is modified only for this build-time process, not globally on the PC.
set(ENV{PATH} "${ODC_QT_BIN};${ODC_OPENCV_BIN};$ENV{PATH}")
if(ODC_CONFIGURATION STREQUAL "Debug")
    set(_qt_mode --debug)
else()
    set(_qt_mode --release)
endif()
execute_process(
    COMMAND "${ODC_QT_BIN}/windeployqt.exe" ${_qt_mode}
        --no-translations --dir "${ODC_OUT}" "${ODC_EXE}"
    RESULT_VARIABLE _qt_deploy_rc
    OUTPUT_VARIABLE _qt_deploy_output
    ERROR_VARIABLE _qt_deploy_error)
if(NOT _qt_deploy_rc EQUAL 0)
    message(FATAL_ERROR
        "windeployqt failed (exit=${_qt_deploy_rc}):\n"
        "${_qt_deploy_output}\n${_qt_deploy_error}")
endif()

foreach(_qt_dll Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll)
    if(ODC_CONFIGURATION STREQUAL "Debug")
        string(REPLACE ".dll" "d.dll" _qt_dll "${_qt_dll}")
    endif()
    if(NOT EXISTS "${ODC_OUT}/${_qt_dll}")
        message(FATAL_ERROR "Qt DLL is still missing after deployment: ${_qt_dll}")
    endif()
endforeach()
if(ODC_CONFIGURATION STREQUAL "Debug")
    set(_qt_platform "qwindowsd.dll")
else()
    set(_qt_platform "qwindows.dll")
endif()
if(NOT EXISTS "${ODC_OUT}/platforms/${_qt_platform}")
    message(FATAL_ERROR "Qt platform plugin is missing: ${_qt_platform}")
endif()
message(STATUS "Windows deployment completed: ${ODC_OUT} (${ODC_CONFIGURATION})")
