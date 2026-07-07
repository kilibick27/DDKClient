@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
set PATH=C:\Users\vladh\.cargo\bin;C:\Users\vladh\AppData\Local\Programs\Python\Python313;%PATH%

set CMAKE="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set NINJA="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

echo === Configuring DEBUG build ===
%CMAKE% -G Ninja ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_MAKE_PROGRAM=%NINJA% ^
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
  -DVIDEORECORDER=OFF ^
  -DMYSQL=OFF ^
  -DSTEAM=OFF ^
  -S "D:\NewOS\NewOS\TClient-10.8.7" ^
  -B "D:\NewOS\NewOS\TClient-10.8.7\build_debug"

echo === Configure done, build dir: build_debug ===
