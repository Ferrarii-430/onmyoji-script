@echo off
REM ============================================================
REM  yys-script 打包脚本
REM  用法:
REM     build.bat            版本号自动从 README.md 的「### vX.Y」读取
REM     build.bat v1.5       手动指定版本号
REM  流程:
REM     1) 定位 CMake 构建目录(名称可能变化, 匹配 cmake-build*)下的 yys-script.exe
REM        复制覆盖到 build\yys-script.exe
REM     2) src\resource\hook\libdx11_hook.dll 覆盖到 build\src\resource\hook\
REM     3) src\resource\screenshot\ 下所有文件覆盖到 build\src\resource\screenshot\
REM     4) 将 build\ 内的文件压缩为 yys-script-[版本]-release.zip
REM ============================================================
setlocal enabledelayedexpansion

REM 项目根目录 = 本脚本所在目录(带结尾反斜杠)
set "ROOT=%~dp0"
set "BUILD=%ROOT%build"

REM ---------- 版本号 ----------
set "VERSION=%~1"
if "%VERSION%"=="" (
    for /f "tokens=2 delims= " %%v in ('findstr /r /c:"^### v[0-9]" "%ROOT%README.md"') do (
        set "VERSION=%%v"
        goto :got_version
    )
)
:got_version
if "%VERSION%"=="" (
    echo [错误] 未能确定版本号, 请手动传入, 例如:  build.bat v1.5
    exit /b 1
)
echo [信息] 版本号: %VERSION%

REM ---------- 1. 定位并复制 yys-script.exe ----------
set "CMAKE_BUILD_DIR="
for /d %%D in ("%ROOT%cmake-build*") do (
    if exist "%%D\yys-script.exe" set "CMAKE_BUILD_DIR=%%D"
)
if not defined CMAKE_BUILD_DIR (
    echo [错误] 未找到含 yys-script.exe 的 CMake 构建目录 ^(cmake-build*^), 请先完成构建
    exit /b 1
)
echo [信息] 构建目录: !CMAKE_BUILD_DIR!

if not exist "%BUILD%" mkdir "%BUILD%"
echo [步骤 1/4] 复制 yys-script.exe
copy /y "!CMAKE_BUILD_DIR!\yys-script.exe" "%BUILD%\yys-script.exe" >nul
if errorlevel 1 ( echo [错误] 复制 yys-script.exe 失败 & exit /b 1 )

REM ---------- 2. 复制 hook DLL ----------
echo [步骤 2/4] 复制 libdx11_hook.dll
if not exist "%BUILD%\src\resource\hook" mkdir "%BUILD%\src\resource\hook"
copy /y "%ROOT%src\resource\hook\libdx11_hook.dll" "%BUILD%\src\resource\hook\libdx11_hook.dll" >nul
if errorlevel 1 ( echo [错误] 复制 libdx11_hook.dll 失败 & exit /b 1 )

REM ---------- 3. 复制 screenshot 目录 ----------
echo [步骤 3/4] 复制 screenshot 目录
if not exist "%BUILD%\src\resource\screenshot" mkdir "%BUILD%\src\resource\screenshot"
xcopy "%ROOT%src\resource\screenshot\*" "%BUILD%\src\resource\screenshot\" /y /i /e >nul
if errorlevel 1 ( echo [错误] 复制 screenshot 失败 & exit /b 1 )

REM ---------- 4. 压缩 build 目录 ----------
set "ZIP=%ROOT%yys-script-%VERSION%-release.zip"
echo [步骤 4/4] 压缩为 %ZIP%
if exist "%ZIP%" del /f /q "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%BUILD%\*' -DestinationPath '%ZIP%' -Force"
if errorlevel 1 ( echo [错误] 压缩失败 & exit /b 1 )

echo.
echo [完成] 已生成: %ZIP%
endlocal
