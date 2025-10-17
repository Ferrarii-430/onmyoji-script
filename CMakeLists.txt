cmake_minimum_required(VERSION 3.30.5)
project(qtTest LANGUAGES CXX)

# ------------------------------
# 编译设置
# ------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ------------------------------
# 路径配置
# ------------------------------
# Qt 安装目录（MinGW 版本）
set(CMAKE_PREFIX_PATH
        "D:/QT/6.9.2/mingw_64"
        "D:/vcpkg/installed/x64-mingw-static"
)
#-DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic

# OpenCV
find_package(OpenCV REQUIRED)

# ------------------------------
# Qt 组件
# ------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# ------------------------------
# 源码文件
# ------------------------------
add_executable(qtTest WIN32
        main.cpp
        src/widget/main/mainwindow.cpp
        src/widget/main/mainwindow.h
        src/widget/main/mainwindow.ui
        ExecutionSteps.cpp
        ExecutionSteps.h
        src/common/ConfigTypeEnum.cpp
        src/common/ConfigTypeEnum.h
        src/common/Logger.cpp
        src/common/Logger.h
        src/utils/common.cpp
        src/utils/common.h
        src/widget/editTask/edittaskdialog.cpp
        src/widget/editTask/edittaskdialog.h
        src/widget/editTask/edittaskdialog.ui
        src/widget/typeOpenCVForm/typeopencvform.cpp
        src/widget/typeOpenCVForm/typeopencvform.h
        src/widget/typeOpenCVForm/typeopencvform.ui
        src/widget/waitForm/waitform.cpp
        src/widget/waitForm/waitform.h
        src/widget/waitForm/waitform.ui
        src/widget/typeOpenCVForm/ScreenCaptureWidget.cpp
        src/widget/typeOpenCVForm/ScreenCaptureWidget.h
        src/widget/ocrForm/OcrForm.cpp
        src/widget/ocrForm/OcrForm.h
        src/widget/ocrForm/ui_OcrForm.h
        src/utils/MouseSimulator.cpp
        src/utils/MouseSimulator.h
        src/common/SettingManager.cpp
        src/common/SettingManager.h
        src/widget/setting/settingdialog.cpp
        src/widget/setting/settingdialog.h
        src/widget/setting/settingdialog.ui
        src/common/ConfigManager.h
)

# ------------------------------
# 宏定义 & 包含目录
# ------------------------------
add_definitions(-DPROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

include_directories(
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/common
)

# ------------------------------
# 链接库
# ------------------------------
target_link_libraries(qtTest
        PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        ${OpenCV_LIBS}
)

# ------------------------------
# 拷贝资源文件
# ------------------------------
file(COPY ${CMAKE_SOURCE_DIR}/src/injection/
        DESTINATION ${CMAKE_BINARY_DIR}/)

file(COPY ${CMAKE_SOURCE_DIR}/src/resource/
        DESTINATION ${CMAKE_BINARY_DIR}/src/resource/)

# ------------------------------
# Qt 自动复制依赖 (Qt 6.5+)
# ------------------------------
qt_finalize_executable(qtTest)


# ------------------------------
# Qt DLL 拷贝（MinGW 动态库模式）
# ------------------------------
if(WIN32 AND MINGW)
    # Qt 二进制目录
    set(QT_BIN_DIR "D:/QT/6.9.2/mingw_64/bin")
    set(QT_PLUGINS_DIR "D:/QT/6.9.2/mingw_64/plugins")

    # Qt 核心 DLL
    foreach(QT_LIB Core Gui Widgets)
        add_custom_command(TARGET qtTest POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_BIN_DIR}/Qt6${QT_LIB}.dll"
                "$<TARGET_FILE_DIR:qtTest>")
    endforeach()

    # 平台插件 qwindows.dll
    add_custom_command(TARGET qtTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:qtTest>/plugins/platforms")
    add_custom_command(TARGET qtTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_PLUGINS_DIR}/platforms/qwindows.dll"
            "$<TARGET_FILE_DIR:qtTest>/plugins/platforms/")
endif()

# ------------------------------
# 可选: 打印调试信息
# ------------------------------
message(STATUS "Qt prefix path: ${CMAKE_PREFIX_PATH}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
