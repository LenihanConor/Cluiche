@echo off
cd Cluiche\Tests\GoogleTests
"C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" GoogleTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
if %ERRORLEVEL% EQU 0 (
    echo Build successful!
    cd ..\..\..
    echo Running tests...
    Cluiche\bin\Debug\x64\GoogleTests.exe --gtest_filter=*Application*
) else (
    echo Build failed with error %ERRORLEVEL%
)
