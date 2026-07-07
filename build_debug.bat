@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
set PATH=C:\Users\vladh\.cargo\bin;C:\Users\vladh\AppData\Local\Programs\Python\Python313;%PATH%

set CMAKE="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set NINJA="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set SRC=D:\NewOS\NewOS\TClient-10.8.7
set BUILD=D:\NewOS\NewOS\TClient-10.8.7\build_debug

echo === Configure Debug ===
%CMAKE% -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=%NINJA% -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DVIDEORECORDER=OFF -DMYSQL=OFF -DSTEAM=OFF -S "%SRC%" -B "%BUILD%"
if %ERRORLEVEL% NEQ 0 ( echo === CONFIGURE FAILED === & pause & exit /b 1 )

echo === Build Debug ===
%NINJA% -C "%BUILD%" -j%NUMBER_OF_PROCESSORS% DDNet.exe
if %ERRORLEVEL% NEQ 0 ( echo === BUILD FAILED === & pause & exit /b 1 )

echo === BUILD SUCCESS === Output: %BUILD%\DDNet.exe
pause
