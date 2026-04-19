@echo off
cd Cluiche\Tests\GoogleTests
"C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" GoogleTests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /v:minimal
if %ERRORLEVEL% EQU 0 (
    echo Build successful!
    cd ..\..\..
    echo Running tests...
    Cluiche\bin\exe\Debug\GoogleTests.exe --gtest_filter=*Application*
) else (
    echo Build failed with error %ERRORLEVEL%
)
