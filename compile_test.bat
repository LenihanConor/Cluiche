@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >/dev/null 2>&1
cd /d C:\GitHub\Cluiche\Dia\DiaApplication
cl /c /std:c++17 /EHsc /I"C:\GitHub\Cluiche\Dia" /I"C:\GitHub\Cluiche\External" ApplicationStateObject.cpp ApplicationProcessingUnit.cpp ApplicationPhase.cpp 2>&1
echo.
echo Build exit code: %errorlevel%
