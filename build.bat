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
REM     3) 使用 windeployqt 收集 Qt 运行时依赖
REM     4) 复制 MinGW 运行时 DLL、onnxruntime.dll
REM     5) 复制 remote_capture_call.exe
REM     6) 复制 src\resource\ 完整资源树（OCR 引擎/模型/截图/YOLO 等）
REM     7) 校验关键文件是否齐全
REM     8) 压缩为 yys-script-[版本]-release.zip
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
set "BUILD_OUT=%ROOT%\build"
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

if not defined MINGW_DIR (
    echo [警告] 无法定位 MinGW 目录，将跳过 MinGW 运行时 DLL 复制
) else (
    echo [信息] MinGW 目录: %MINGW_DIR%
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

REM ---------- 准备打包输出目录 ----------
if "%DO_CLEAN%"=="1" (
    echo [信息] 清空旧打包目录: %BUILD_OUT%
    if exist "%BUILD_OUT%" rmdir /s /q "%BUILD_OUT%"
)
if not exist "%BUILD_OUT%" mkdir "%BUILD_OUT%"

REM ---------- 1. 复制主程序 ----------
echo [步骤 1/7] 复制主程序 yys-script.exe
copy /y "%CMAKE_BUILD_DIR%\yys-script.exe" "%BUILD_OUT%\yys-script.exe" >nul
if errorlevel 1 ( echo [错误] 复制 yys-script.exe 失败 & exit /b 1 )

REM ---------- 2. 使用 windeployqt 收集 Qt 依赖 ----------
echo [步骤 2/7] 使用 windeployqt 收集 Qt 运行时依赖
set "WINDEPLOYQT=%QT_DIR%\bin\windeployqt.exe"
"%WINDEPLOYQT%" --release --no-translations --dir "%BUILD_OUT%" "%BUILD_OUT%\yys-script.exe"
if errorlevel 1 (
    echo [错误] windeployqt 执行失败
    exit /b 1
)

REM ---------- 3. 复制 MinGW 运行时 DLL ----------
echo [步骤 3/7] 复制 MinGW 运行时 DLL
if defined MINGW_DIR (
    for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
        if exist "%MINGW_DIR%\bin\%%F" (
            copy /y "%MINGW_DIR%\bin\%%F" "%BUILD_OUT%\%%F" >nul
            if errorlevel 1 ( echo [错误] 复制 %%F 失败 & exit /b 1 )
            echo [信息] 已复制 %%F
        ) else (
            echo [警告] 未找到 MinGW 运行时 DLL: %%F
        )
    )
)

REM ---------- 4. 复制 onnxruntime.dll ----------
echo [步骤 4/7] 复制 onnxruntime.dll
if exist "%CMAKE_BUILD_DIR%\onnxruntime.dll" (
    copy /y "%CMAKE_BUILD_DIR%\onnxruntime.dll" "%BUILD_OUT%\onnxruntime.dll" >nul
    if errorlevel 1 ( echo [错误] 复制 onnxruntime.dll 失败 & exit /b 1 )
) else if exist "%ROOT%\src\lib\onnxruntime.dll" (
    copy /y "%ROOT%\src\lib\onnxruntime.dll" "%BUILD_OUT%\onnxruntime.dll" >nul
    if errorlevel 1 ( echo [错误] 复制 onnxruntime.dll 失败 & exit /b 1 )
) else (
    echo [警告] 未找到 onnxruntime.dll
)
REM 同时把 onnxruntime.dll 放到 RapidOCR-json.exe 同目录，避免子进程找不到依赖
if exist "%BUILD_OUT%\onnxruntime.dll" (
    copy /y "%BUILD_OUT%\onnxruntime.dll" "%BUILD_OUT%\src\resource\RapidOCR\onnxruntime.dll" >nul
)

REM ---------- 5. 复制 remote_capture_call.exe ----------
echo [步骤 5/7] 复制 remote_capture_call.exe
if exist "%CMAKE_BUILD_DIR%\remote_capture_call.exe" (
    copy /y "%CMAKE_BUILD_DIR%\remote_capture_call.exe" "%BUILD_OUT%\remote_capture_call.exe" >nul
    if errorlevel 1 ( echo [错误] 复制 remote_capture_call.exe 失败 & exit /b 1 )
) else if exist "%ROOT%\src\injection\remote_capture_call.exe" (
    copy /y "%ROOT%\src\injection\remote_capture_call.exe" "%BUILD_OUT%\remote_capture_call.exe" >nul
    if errorlevel 1 ( echo [错误] 复制 remote_capture_call.exe 失败 & exit /b 1 )
) else (
    echo [警告] 未找到 remote_capture_call.exe
)

REM ---------- 6. 复制 src\resource 资源树 ----------
echo [步骤 6/7] 复制 src\resource 资源树
if not exist "%BUILD_OUT%\src\resource" mkdir "%BUILD_OUT%\src\resource"
xcopy "%SRC_RESOURCE%\*" "%BUILD_OUT%\src\resource\" /y /i /e >nul
if errorlevel 1 ( echo [错误] 复制 src\resource 失败 & exit /b 1 )
REM 确保运行时可写目录存在
if not exist "%BUILD_OUT%\src\resource\thumbnail" mkdir "%BUILD_OUT%\src\resource\thumbnail"
if not exist "%BUILD_OUT%\src\resource\log" mkdir "%BUILD_OUT%\src\resource\log"

REM ---------- 7. 校验关键文件 ----------
echo [步骤 7/7] 校验关键文件
set "MISSING=0"
for %%F in (
    "yys-script.exe"
    "remote_capture_call.exe"
    "Qt6Core.dll"
    "Qt6Gui.dll"
    "Qt6Widgets.dll"
    "onnxruntime.dll"
    "platforms\qwindows.dll"
    "src\resource\config.json"
    "src\resource\setting.json"
    "src\resource\classes.txt"
    "src\resource\onmyoji-yolo-v5.onnx"
    "src\resource\yolo_label_catalog.json"
    "src\resource\hook\libdx11_hook.dll"
    "src\resource\RapidOCR\RapidOCR-json.exe"
    "src\resource\RapidOCR\models\rec_ch_PP-OCRv4_infer.onnx"
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

REM ---------- 8 清空运行截图目录（避免旧截图混入压缩包） ----------
echo [步骤 8/8] 清空 thumbnail 目录
if exist "%BUILD_OUT%\src\resource\thumbnail" (
    del /f /q "%BUILD_OUT%\src\resource\thumbnail\*" >nul 2>nul
)
echo [信息] 已清空: %BUILD_OUT%\src\resource\thumbnail

REM ---------- 9. 压缩 ----------
set "ZIP=%ROOT%\yys-script-%VERSION%-release.zip"
echo.
echo [压缩] 生成 %ZIP%
if exist "%ZIP%" del /f /q "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%BUILD_OUT%\*' -DestinationPath '%ZIP%' -Force"
if errorlevel 1 ( echo [错误] 压缩失败 & exit /b 1 )

echo.
echo [完成] 已生成: %ZIP%
echo [提示] 解压后请使用管理员身份运行 yys-script.exe
endlocal
