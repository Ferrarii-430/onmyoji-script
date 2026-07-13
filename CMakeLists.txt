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

if(WIN32)
    enable_language(RC)
    string(APPEND CMAKE_RC_FLAGS " -I \"${CMAKE_CURRENT_SOURCE_DIR}/img\"")
endif()

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

link_directories(${CMAKE_SOURCE_DIR}/src/lib)

# ------------------------------
# 源码文件
# ------------------------------
add_executable(qtTest WIN32
        # 应用入口
        src/app/main.cpp
        resources.qrc

        # core: 基础设施（日志/配置/路径/事件循环工具）
        src/core/AppPaths.h
        src/core/ConfigTypeEnum.cpp
        src/core/ConfigTypeEnum.h
        src/core/EventLoopUtils.cpp
        src/core/EventLoopUtils.h
        src/core/Logger.cpp
        src/core/Logger.h
        src/core/ProfileStore.cpp
        src/core/ProfileStore.h
        src/core/SettingManager.cpp
        src/core/SettingManager.h

        # platform: Windows 平台能力（进程/DPI/鼠标模拟）
        src/platform/DPIHelper.h
        src/platform/MouseSimulator.cpp
        src/platform/MouseSimulator.h
        src/platform/ProcessUtils.cpp
        src/platform/ProcessUtils.h

        # game: 游戏窗口与截图
        src/game/GameWindow.cpp
        src/game/GameWindow.h
        src/game/capture/CaptureService.cpp
        src/game/capture/CaptureService.h
        src/game/capture/Dx11CaptureShared.h
        src/game/capture/Dx11HookCapture.cpp
        src/game/capture/Dx11HookCapture.h
        src/game/capture/WindowCapture.cpp
        src/game/capture/WindowCapture.h

        # vision: 图像识别（模板匹配/YOLO/OCR/几何工具）
        src/vision/ClassNameCache.cpp
        src/vision/ClassNameCache.h
        src/vision/DetectionCatalog.cpp
        src/vision/DetectionCatalog.h
        src/vision/Geometry.cpp
        src/vision/Geometry.h
        src/vision/OcrEngine.cpp
        src/vision/OcrEngine.h
        src/vision/TemplateMatcher.cpp
        src/vision/TemplateMatcher.h
        src/vision/YOLODetector.cpp
        src/vision/YOLODetector.h

        # engine: 脚本动作编排与任务执行
        src/engine/ScriptActions.cpp
        src/engine/ScriptActions.h
        src/engine/TaskRunner.cpp
        src/engine/TaskRunner.h
        src/engine/scenarios/Arena.cpp
        src/engine/scenarios/Arena.h
        src/engine/scenarios/BorderBreakthrough.cpp
        src/engine/scenarios/BorderBreakthrough.h

        # ui: 界面层
        src/ui/main/mainwindow.cpp
        src/ui/main/mainwindow.h
        src/ui/main/mainwindow.ui
        src/ui/editTask/edittaskdialog.cpp
        src/ui/editTask/edittaskdialog.h
        src/ui/editTask/edittaskdialog.ui
        src/ui/typeOpenCVForm/typeopencvform.cpp
        src/ui/typeOpenCVForm/typeopencvform.h
        src/ui/typeOpenCVForm/typeopencvform.ui
        src/ui/typeOpenCVForm/ScreenCaptureWidget.cpp
        src/ui/typeOpenCVForm/ScreenCaptureWidget.h
        src/ui/waitForm/waitform.cpp
        src/ui/waitForm/waitform.h
        src/ui/waitForm/waitform.ui
        src/ui/ocrForm/OcrForm.cpp
        src/ui/ocrForm/OcrForm.h
        src/ui/ocrForm/RoiPreviewWidget.cpp
        src/ui/ocrForm/RoiPreviewWidget.h
        src/ui/ocrForm/ui_OcrForm.h
        src/ui/yoloForm/YoloForm.cpp
        src/ui/yoloForm/YoloForm.h
        src/ui/theme/Theme.cpp
        src/ui/theme/Theme.h
        src/ui/setting/settingdialog.cpp
        src/ui/setting/settingdialog.h
        src/ui/setting/settingdialog.ui
)

if(WIN32)
    target_sources(qtTest PRIVATE img/logo.rc)
endif()

# ------------------------------
# 宏定义 & 包含目录
# ------------------------------
add_definitions(-DPROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

include_directories(
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/include
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
        onnxruntime
)

# ------------------------------
# 拷贝资源文件
# ------------------------------
file(COPY ${CMAKE_SOURCE_DIR}/src/injection/
        DESTINATION ${CMAKE_BINARY_DIR}/)

# 注意：hook 目录下的 libdx11_hook.dll 运行时会被注入到游戏进程（LoadLibrary），
# 只要游戏/脚本仍持有它，构建目录里的这份就被系统锁定，configure 阶段的 file(COPY)
# 直接覆盖会报 "Permission denied" 而中断配置。这里先在批量拷贝中排除 hook 目录，
# 再在构建阶段用 copy_if_different 单独拷贝——内容未变化时会跳过，避免覆盖被占用的文件。
file(COPY ${CMAKE_SOURCE_DIR}/src/resource/
        DESTINATION ${CMAKE_BINARY_DIR}/src/resource/
        PATTERN "hook" EXCLUDE)

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

    # 复制 ONNX Runtime DLL
    add_custom_command(TARGET qtTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/lib/onnxruntime.dll"
            "$<TARGET_FILE_DIR:qtTest>"
    )

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
# 单独拷贝注入用的 hook DLL（copy_if_different：内容一致则跳过，避免覆盖被游戏进程锁定的文件）
# ------------------------------
add_custom_command(TARGET qtTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/src/resource/hook")
add_custom_command(TARGET qtTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/resource/hook/libdx11_hook.dll"
        "${CMAKE_BINARY_DIR}/src/resource/hook/libdx11_hook.dll")

# ------------------------------
# 可选: 打印调试信息
# ------------------------------
message(STATUS "Qt prefix path: ${CMAKE_PREFIX_PATH}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
