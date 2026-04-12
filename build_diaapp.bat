@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >/dev/null 2>&1
cd /d C:\GitHub\Cluiche
msbuild Dia\DiaApplication\DiaApplication.vcxproj /p:Configuration=Debug /p:Platform=Win32 /v:m /nologo
