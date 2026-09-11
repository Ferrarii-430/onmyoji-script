@echo off
chcp 65001 >nul
REM ============================================================
REM  yys-script 构建与打包脚本
REM  用法:
REM     build.bat            使用已存在的 CMake 构建结果进行打包
REM     build.bat build      先执行 CMake 构建，再打包
REM     build.bat clean      打包前清空 build\ 输出目录
REM     build.bat build clean
REM  流程:
REM     1) 自动定位 Qt / MinGW / CMake 工具链（优先从 CMakeCache.txt 读取）
REM     2) 可选：执行 CMake Release 构建
REM     3) 在独立暂存目录 build\package 中全新组装发布包
REM        （每次删除重建，杜绝上次打包/已废弃源文件的残留混入压缩包）
REM     4) 复制主程序并 strip 剥离调试符号（仅打包副本，开发目录保留符号）
REM     5) windeployqt 收集 Qt 运行时依赖（按需精简，详见步骤内注释）
REM     6) 复制 onnxruntime.dll、remote_capture_call.exe、src\resource 资源树
REM     7) 校验关键文件并压缩为 yys-script-[版本]-release.zip
REM ============================================================
setlocal enabledelayedexpansion

REM ---------- 参数解析 ----------
set "DO_BUILD=0"
set "DO_CLEAN=0"
set "VERSION="
for %%A in (%*) do (
    if /i "%%A"=="build" (
        set "DO_BUILD=1"
    ) else if /i "%%A"=="clean" (
        set "DO_CLEAN=1"
    ) else (
        set "VERSION=%%A"
    )
)

REM ---------- 基础路径 ----------
set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build"
set "BUILD_OUT=%BUILD_DIR%\package"
set "SRC_RESOURCE=%ROOT%\src\resource"
set "README=%ROOT%\README.md"

REM ---------- 版本号 ----------
if "%VERSION%"=="" (
    if exist "%README%" (
        for /f "tokens=2 delims= " %%v in ('findstr /r /c:"^### v[0-9]" "%README%"') do (
            set "VERSION=%%v"
            goto :got_version
        )
    )
)
:got_version
if "%VERSION%"=="" (
    echo [错误] 未能确定版本号，请手动传入，例如：build.bat v1.5.2
    exit /b 1
)
echo [信息] 版本号: %VERSION%

REM ---------- 定位 CMake 构建目录 ----------
set "CMAKE_BUILD_DIR="
for /d %%D in ("%ROOT%\cmake-build*") do (
    if exist "%%D\yys-script.exe" set "CMAKE_BUILD_DIR=%%D"
)
if not defined CMAKE_BUILD_DIR (
    for /d %%D in ("%ROOT%\cmake-build*") do (
        if exist "%%D\Makefile" set "CMAKE_BUILD_DIR=%%D"
    )
)
if not defined CMAKE_BUILD_DIR (
    set "CMAKE_BUILD_DIR=%ROOT%\cmake-build-release-qt_mingw"
)

REM ---------- 从 CMakeCache.txt 读取工具链路径 ----------
set "QT_DIR="
set "MINGW_DIR="
set "CACHE=%CMAKE_BUILD_DIR%\CMakeCache.txt"
if exist "%CACHE%" (
    REM 通过 Qt6_DIR 反推 Qt 安装根目录，例如 D:/QT/6.9.2/mingw_64/lib/cmake/Qt6
    for /f "tokens=2 delims==" %%a in ('findstr /b /c:"Qt6_DIR:PATH=" "%CACHE%" 2^>nul') do (
        set "QT_CMAKE=%%~a"
        set "QT_CMAKE=!QT_CMAKE:/lib/cmake/Qt6=!"
        set "QT_CMAKE=!QT_CMAKE:\lib\cmake\Qt6=!"
        if exist "!QT_CMAKE!\bin\windeployqt.exe" set "QT_DIR=!QT_CMAKE!"
    )
    REM 通过 C++ 编译器路径反推 MinGW 根目录
    for /f "tokens=2 delims==" %%a in ('findstr /b /c:"CMAKE_CXX_COMPILER:FILEPATH=" "%CACHE%" 2^>nul') do (
        set "CXX=%%~a"
        for %%b in ("!CXX!") do set "MINGW_DIR=%%~dpb.."
        for %%b in ("!MINGW_DIR!") do set "MINGW_DIR=%%~fb"
    )
)

REM ---------- 回退到默认工具链路径 ----------
if not defined QT_DIR (
    if exist "D:\QT\6.9.2\mingw_64\bin\windeployqt.exe" set "QT_DIR=D:\QT\6.9.2\mingw_64"
)
if not defined MINGW_DIR (
    if exist "D:\QT\Tools\mingw1310_64\bin\c++.exe" set "MINGW_DIR=D:\QT\Tools\mingw1310_64"
)

if not defined QT_DIR (
    echo [错误] 无法定位 Qt 工具链，请检查 Qt 安装路径或设置 QT_DIR 环境变量
    exit /b 1
)
if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo [错误] 未找到 windeployqt.exe：%QT_DIR%\bin\windeployqt.exe
    exit /b 1
)
echo [信息] Qt 目录: %QT_DIR%
echo [信息] MinGW 目录: %MINGW_DIR%

if not defined MINGW_DIR (
    echo [警告] 无法定位 MinGW 目录，将跳过 exe 符号剥离
)

REM ---------- 定位 CMake 可执行文件 ----------
set "CMAKE_EXE="
for %%P in (
    "D:\Qt\Tools\CMake_64\bin\cmake.exe"
    "D:\CMake\bin\cmake.exe"
    "C:\Program Files\CMake\bin\cmake.exe"
    "C:\Program Files (x86)\CMake\bin\cmake.exe"
) do (
    if not defined CMAKE_EXE (
        if exist "%%~P" set "CMAKE_EXE=%%~P"
    )
)
if not defined CMAKE_EXE (
    where cmake >nul 2>nul
    if not errorlevel 1 set "CMAKE_EXE=cmake"
)
if not defined CMAKE_EXE (
    echo [错误] 无法定位 cmake.exe，请将其加入 PATH 或安装 CMake
    exit /b 1
)
echo [信息] CMake: %CMAKE_EXE%

REM ---------- 可选：执行 CMake 构建 ----------
if "%DO_BUILD%"=="1" (
    echo.
    echo [构建] 开始 CMake Release 构建...
    if not exist "%CMAKE_BUILD_DIR%" mkdir "%CMAKE_BUILD_DIR%"

    "%CMAKE_EXE%" -S "%ROOT%" -B "%CMAKE_BUILD_DIR%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%"
    if errorlevel 1 (
        echo [错误] CMake 配置失败
        exit /b 1
    )

    "%CMAKE_EXE%" --build "%CMAKE_BUILD_DIR%" --config Release --parallel
    if errorlevel 1 (
        echo [错误] CMake 构建失败
        exit /b 1
    )
    echo [构建] 完成
) else (
    if not exist "%CMAKE_BUILD_DIR%\yys-script.exe" (
        echo [错误] 未找到构建产物 %CMAKE_BUILD_DIR%\yys-script.exe
        echo [提示] 可运行  build.bat build  先执行构建
        exit /b 1
    )
)

echo [信息] 构建目录: %CMAKE_BUILD_DIR%

REM ---------- 准备打包暂存目录 ----------
if "%DO_CLEAN%"=="1" (
    echo [信息] 清空旧打包目录: %BUILD_DIR%
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)
REM 暂存目录每次删除重建：保证压缩包内容与当前源目录严格一致。
REM 旧方案直接在 build\ 里增量 xcopy，不清理已删除的源文件，曾把废弃的
REM v3 OCR 模型、误执行 cmake 产生的 CMakeFiles\ 等一并打进压缩包。
if exist "%BUILD_OUT%" rmdir /s /q "%BUILD_OUT%"
mkdir "%BUILD_OUT%" >nul 2>nul

REM ---------- 1. 复制主程序并剥离调试符号 ----------
echo [步骤 1/7] 复制主程序 yys-script.exe
copy /y "%CMAKE_BUILD_DIR%\yys-script.exe" "%BUILD_OUT%\yys-script.exe" >nul
if errorlevel 1 ( echo [错误] 复制 yys-script.exe 失败 & exit /b 1 )

REM vcpkg 静态链接的 OpenCV 使 Release exe 内嵌约 25MB DWARF 调试符号，
REM 仅对打包副本 strip（37MB 降至 12MB）；开发构建目录中的 exe 保留符号便于调试。
if defined MINGW_DIR (
    if exist "%MINGW_DIR%\bin\strip.exe" (
        echo [信息] strip 剥离调试符号: yys-script.exe
        "%MINGW_DIR%\bin\strip.exe" --strip-all "%BUILD_OUT%\yys-script.exe"
        if errorlevel 1 ( echo [错误] strip yys-script.exe 失败 & exit /b 1 )
    ) else (
        echo [警告] 未找到 %MINGW_DIR%\bin\strip.exe，跳过符号剥离，包体积会偏大
    )
)

REM ---------- 2. 使用 windeployqt 收集 Qt 依赖 ----------
echo [步骤 2/7] 使用 windeployqt 收集 Qt 运行时依赖
set "WINDEPLOYQT=%QT_DIR%\bin\windeployqt.exe"
REM 精简参数（本程序为纯 Widgets 应用，未使用 QOpenGLWidget / QtQuick / SVG）:
REM   --no-opengl-sw                  不部署软件渲染器 opengl32sw.dll（约 20MB），
REM                                   仅无 GPU 的机器需要；目标机器要运行游戏，必有 GPU
REM   --no-system-d3d-compiler        不部署 D3Dcompiler_47.dll（约 4MB），仅 Qt Quick/RHI 着色器编译需要
REM   --exclude-plugins qsvg,qsvgicon 不部署 SVG 插件及 Qt6Svg.dll（程序无任何 SVG 资源）
REM   --skip-plugin-types generic     不部署触摸屏插件 qtuiotouchplugin.dll
"%WINDEPLOYQT%" --release --no-translations --no-opengl-sw --no-system-d3d-compiler --exclude-plugins qsvg,qsvgicon --skip-plugin-types generic --dir "%BUILD_OUT%" "%BUILD_OUT%\yys-script.exe"
if errorlevel 1 (
    echo [错误] windeployqt 执行失败
    exit /b 1
)

REM --- TLS 后端插件兜底：Qt6::Network 访问 https 必须加载至少一个 TLS 后端插件 ---
REM Qt 6.7+ 的 windeployqt 将插件部署到 exe 同级的扁平子目录（tls\ 等），
REM 正常会随 Qt6Network.dll 一起部署 tls\qschannelbackend.dll；
REM 个别参数组合下可能漏掉，这里显式补一次，避免 "No functional TLS backend was found"。
if not exist "%BUILD_OUT%\tls\qschannelbackend.dll" (
    if exist "%QT_DIR%\plugins\tls\qschannelbackend.dll" (
        copy /y "%QT_DIR%\plugins\tls\qschannelbackend.dll" "%BUILD_OUT%\tls\qschannelbackend.dll" >nul
        if errorlevel 1 ( echo [错误] 复制 qschannelbackend.dll 失败 & exit /b 1 )
        echo [信息] 已补复制 TLS 插件: tls\qschannelbackend.dll
    ) else (
        echo [警告] 未找到 qschannelbackend.dll，HTTPS 请求将失败
    )
)

REM --- MinGW 运行时 DLL 兜底 ---
REM windeployqt 会自动从 Qt 安装目录 bin\ 部署 libgcc_s_seh-1 / libstdc++-6 / libwinpthread-1，
REM 此处仅在缺失时从编译器目录兜底补齐，不再无条件覆盖复制。
if defined MINGW_DIR (
    for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
        if not exist "%BUILD_OUT%\%%F" (
            if exist "%MINGW_DIR%\bin\%%F" (
                copy /y "%MINGW_DIR%\bin\%%F" "%BUILD_OUT%\%%F" >nul
                echo [信息] 已补复制 MinGW 运行时 DLL: %%F
            )
        )
    )
)

REM ---------- 3. 复制 onnxruntime.dll ----------
echo [步骤 3/7] 复制 onnxruntime.dll
if exist "%CMAKE_BUILD_DIR%\onnxruntime.dll" (
    copy /y "%CMAKE_BUILD_DIR%\onnxruntime.dll" "%BUILD_OUT%\onnxruntime.dll" >nul
    if errorlevel 1 ( echo [错误] 复制 onnxruntime.dll 失败 & exit /b 1 )
) else if exist "%ROOT%\src\lib\onnxruntime.dll" (
    copy /y "%ROOT%\src\lib\onnxruntime.dll" "%BUILD_OUT%\onnxruntime.dll" >nul
    if errorlevel 1 ( echo [错误] 复制 onnxruntime.dll 失败 & exit /b 1 )
) else (
    echo [警告] 未找到 onnxruntime.dll
)

REM ---------- 4. 复制 remote_capture_call.exe ----------
echo [步骤 4/7] 复制 remote_capture_call.exe
if exist "%CMAKE_BUILD_DIR%\remote_capture_call.exe" (
    copy /y "%CMAKE_BUILD_DIR%\remote_capture_call.exe" "%BUILD_OUT%\remote_capture_call.exe" >nul
    if errorlevel 1 ( echo [错误] 复制 remote_capture_call.exe 失败 & exit /b 1 )
) else if exist "%ROOT%\src\injection\remote_capture_call.exe" (
    copy /y "%ROOT%\src\injection\remote_capture_call.exe" "%BUILD_OUT%\remote_capture_call.exe" >nul
    if errorlevel 1 ( echo [错误] 复制 remote_capture_call.exe 失败 & exit /b 1 )
) else (
    echo [警告] 未找到 remote_capture_call.exe
)

REM ---------- 5. 复制 src\resource 资源树 ----------
echo [步骤 5/7] 复制 src\resource 资源树
REM 暂存目录是全新组装的，无需再清理旧文件，直接完整复制
xcopy "%SRC_RESOURCE%\*" "%BUILD_OUT%\src\resource\" /y /i /e >nul
if errorlevel 1 ( echo [错误] 复制 src\resource 失败 & exit /b 1 )
REM 剔除开发期文件（源码零引用，不属于运行时资产）:
REM   example_cofing.json —— config.json 的开发样例，无任何代码/文档引用
REM   log\dx11_log.txt    —— 空占位日志，运行时由注入器写入，目录会自动创建
if exist "%BUILD_OUT%\src\resource\example_cofing.json" del /f /q "%BUILD_OUT%\src\resource\example_cofing.json" >nul 2>nul
if exist "%BUILD_OUT%\src\resource\log\dx11_log.txt" del /f /q "%BUILD_OUT%\src\resource\log\dx11_log.txt" >nul 2>nul
REM 确保运行时可写目录存在
if not exist "%BUILD_OUT%\src\resource\thumbnail" mkdir "%BUILD_OUT%\src\resource\thumbnail"
if not exist "%BUILD_OUT%\src\resource\log" mkdir "%BUILD_OUT%\src\resource\log"

REM ---------- 6. 校验关键文件 ----------
echo [步骤 6/7] 校验关键文件
set "MISSING=0"
for %%F in (
    "yys-script.exe"
    "remote_capture_call.exe"
    "Qt6Core.dll"
    "Qt6Gui.dll"
    "Qt6Widgets.dll"
    "Qt6Network.dll"
    "libgcc_s_seh-1.dll"
    "libstdc++-6.dll"
    "libwinpthread-1.dll"
    "onnxruntime.dll"
    "platforms\qwindows.dll"
    "tls\qschannelbackend.dll"
    "src\resource\config.json"
    "src\resource\setting.json"
    "src\resource\classes.txt"
    "src\resource\onmyoji-yolo-v5.onnx"
    "src\resource\yolo_label_catalog.json"
    "src\resource\hook\libdx11_hook.dll"
    "src\resource\RapidOCR\models\ch_PP-OCRv4_det_infer.onnx"
    "src\resource\RapidOCR\models\ch_ppocr_mobile_v2.0_cls_infer.onnx"
    "src\resource\RapidOCR\models\rec_ch_PP-OCRv4_infer.onnx"
    "src\resource\RapidOCR\models\dict_chinese.txt"
) do (
    if not exist "%BUILD_OUT%\%%~F" (
        echo [校验失败] 缺失: %%~F
        set "MISSING=1"
    )
)
if "%MISSING%"=="1" (
    echo [错误] 关键文件缺失，打包结果可能无法正常运行
    exit /b 1
)
echo [校验通过] 关键文件齐全

REM ---------- 7. 压缩 ----------
echo [步骤 7/7] 压缩发布包
set "ZIP=%ROOT%\yys-script-%VERSION%-release.zip"
echo [信息] 生成 %ZIP%
if exist "%ZIP%" del /f /q "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%BUILD_OUT%\*' -DestinationPath '%ZIP%' -Force"
if errorlevel 1 ( echo [错误] 压缩失败 & exit /b 1 )

for %%S in ("%ZIP%") do set "ZIP_SIZE=%%~zS"
set /a ZIP_MB=ZIP_SIZE/1048576
echo.
echo [完成] 已生成: %ZIP%
echo [信息] 包大小: 约 %ZIP_MB% MB
echo [提示] 解压后请使用管理员身份运行 yys-script.exe
endlocal
