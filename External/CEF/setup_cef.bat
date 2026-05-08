@echo off
REM ============================================================================
REM CEF Setup Script
REM
REM Downloads/builds CEF dependencies that are too large for source control:
REM   1. Builds libcef_dll_wrapper.lib (Debug + Release) from CEF source
REM   2. Copies CEF runtime DLLs and resources to bin/
REM
REM Prerequisites:
REM   - Visual Studio 2022 with C++ workload installed
REM   - CEF binary distribution extracted somewhere (pass path as argument)
REM
REM Usage:
REM   setup_cef.bat C:\path\to\cef_binary_xxx_windows64
REM ============================================================================

if "%~1"=="" (
    echo Usage: setup_cef.bat ^<path-to-cef-binary-distribution^>
    echo.
    echo Example: setup_cef.bat C:\Temp\cef_binary_146.0.12_windows64
    exit /b 1
)

set CEF_SRC=%~1
set SCRIPT_DIR=%~dp0

REM Verify source directory
if not exist "%CEF_SRC%\CMakeLists.txt" (
    echo ERROR: %CEF_SRC%\CMakeLists.txt not found.
    echo Make sure you point to the extracted CEF binary distribution root.
    exit /b 1
)

REM Find CMake from Visual Studio
set CMAKE=
for /f "delims=" %%i in ('dir /b /s "C:\Program Files\Microsoft Visual Studio\2022\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" 2^>nul') do set CMAKE=%%i
if "%CMAKE%"=="" (
    echo ERROR: Could not find CMake bundled with Visual Studio 2022.
    echo Install the "C++ CMake tools for Windows" component.
    exit /b 1
)
echo Found CMake: %CMAKE%

REM ---- Step 1: Build libcef_dll_wrapper ----
echo.
echo === Building libcef_dll_wrapper (Debug + Release, x64, /MD) ===
if exist "%CEF_SRC%\build" rmdir /s /q "%CEF_SRC%\build"
mkdir "%CEF_SRC%\build"
pushd "%CEF_SRC%\build"

"%CMAKE%" -G "Visual Studio 17 2022" -A x64 -DCEF_RUNTIME_LIBRARY_FLAG=/MD .. || goto :error
"%CMAKE%" --build . --config Debug --target libcef_dll_wrapper || goto :error
"%CMAKE%" --build . --config Release --target libcef_dll_wrapper || goto :error

popd

echo.
echo === Copying wrapper libs ===
copy /Y "%CEF_SRC%\build\libcef_dll_wrapper\Debug\libcef_dll_wrapper.lib" "%SCRIPT_DIR%lib\Debug\" || goto :error
copy /Y "%CEF_SRC%\build\libcef_dll_wrapper\Release\libcef_dll_wrapper.lib" "%SCRIPT_DIR%lib\Release\" || goto :error

REM ---- Step 2: Copy DLLs ----
echo.
echo === Copying CEF runtime DLLs ===
if not exist "%SCRIPT_DIR%bin\Debug" mkdir "%SCRIPT_DIR%bin\Debug"
if not exist "%SCRIPT_DIR%bin\Release" mkdir "%SCRIPT_DIR%bin\Release"

xcopy /Y /d "%CEF_SRC%\Debug\*.dll" "%SCRIPT_DIR%bin\Debug\" >nul
xcopy /Y /d "%CEF_SRC%\Debug\*.bin" "%SCRIPT_DIR%bin\Debug\" >nul 2>nul
xcopy /Y /d "%CEF_SRC%\Debug\*.json" "%SCRIPT_DIR%bin\Debug\" >nul 2>nul

xcopy /Y /d "%CEF_SRC%\Release\*.dll" "%SCRIPT_DIR%bin\Release\" >nul
xcopy /Y /d "%CEF_SRC%\Release\*.bin" "%SCRIPT_DIR%bin\Release\" >nul 2>nul
xcopy /Y /d "%CEF_SRC%\Release\*.json" "%SCRIPT_DIR%bin\Release\" >nul 2>nul

echo.
echo === CEF setup complete ===
echo.
echo Wrapper libs:
dir /b "%SCRIPT_DIR%lib\Debug\libcef_dll_wrapper.lib" 2>nul && echo   lib\Debug\libcef_dll_wrapper.lib [OK]
dir /b "%SCRIPT_DIR%lib\Release\libcef_dll_wrapper.lib" 2>nul && echo   lib\Release\libcef_dll_wrapper.lib [OK]
echo.
echo Runtime DLLs:
dir /b "%SCRIPT_DIR%bin\Debug\libcef.dll" 2>nul && echo   bin\Debug\libcef.dll [OK]
dir /b "%SCRIPT_DIR%bin\Release\libcef.dll" 2>nul && echo   bin\Release\libcef.dll [OK]
exit /b 0

:error
echo.
echo ERROR: CEF setup failed.
popd 2>nul
exit /b 1
