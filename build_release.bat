@echo off
setlocal enabledelayedexpansion

REM Get script directory
set "SRC=%~dp0"
set "SRC=%SRC:~0,-1%"
set "BUILD=%SRC%\build_release"

REM Try to find Visual Studio (2022 or 2019)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=x64
) else if exist "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=x64
) else (
    echo ERROR: Visual Studio 2019/2022 not found!
    echo Please install Visual Studio with C++ development tools.
    pause
    exit /b 1
)

REM Add Rust and Python to PATH if available
if exist "%USERPROFILE%\.cargo\bin" set "PATH=%USERPROFILE%\.cargo\bin;%PATH%"
for /f "delims=" %%i in ('where python 2^>nul') do set "PYTHON_PATH=%%i" & goto :python_found
:python_found

REM Use cmake and ninja from Visual Studio
set CMAKE=cmake
set NINJA=ninja

echo === Configure Release ===
echo Source: %SRC%
echo Build: %BUILD%
%CMAKE% -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=%NINJA% -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DVIDEORECORDER=OFF -DMYSQL=OFF -DSTEAM=OFF -S "%SRC%" -B "%BUILD%"
if %ERRORLEVEL% NEQ 0 ( echo === CONFIGURE FAILED === & pause & exit /b 1 )

echo === Build Release ===
%NINJA% -C "%BUILD%" -j%NUMBER_OF_PROCESSORS% DDNet.exe
if %ERRORLEVEL% NEQ 0 ( echo === BUILD FAILED === & pause & exit /b 1 )

echo === BUILD SUCCESS === Output: %BUILD%\DDNet.exe
pause
