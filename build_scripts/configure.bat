@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64

:: Добавляем Rust/Cargo и Python в PATH
set PATH=C:\Users\vladh\.cargo\bin;C:\Users\vladh\AppData\Local\Programs\Python\Python313;%PATH%

echo --- Versions ---
rustc --version
cargo --version
python --version
echo ----------------

"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" ^
  -DVIDEORECORDER=OFF ^
  -DMYSQL=OFF ^
  -DSTEAM=OFF ^
  "D:\NewOS\NewOS\TClient-10.8.7"
