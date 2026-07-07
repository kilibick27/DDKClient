@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
set PATH=C:\Users\vladh\.cargo\bin;C:\Users\vladh\AppData\Local\Programs\Python\Python313;%PATH%

cd /d "D:\NewOS\NewOS\TClient-10.8.7"
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" -j%NUMBER_OF_PROCESSORS% DDNet.exe
