@echo off
REM Convenience wrapper for running GoogleTests
REM NOTE: Dirty tests are now the DEFAULT behavior (automatic)
REM
REM Usage:
REM   run_dirty.bat          - Run dirty tests (default behavior, same as just running GoogleTests.exe)
REM   run_dirty.bat --all    - Run ALL tests (override dirty filter)

setlocal

set SCRIPT_DIR=%~dp0
set EXE_PATH=%SCRIPT_DIR%..\..\bin\exe\Debug\GoogleTests.exe

REM Check if exe exists
if not exist "%EXE_PATH%" (
    echo ERROR: GoogleTests.exe not found at: %EXE_PATH%
    echo Build the project first.
    exit /b 1
)

REM Check for --all flag
if "%1"=="--all" (
    echo Running ALL tests (overriding dirty filter)...
    "%EXE_PATH%" --gtest_filter=*
) else (
    echo Running tests (dirty filter is automatic)...
    "%EXE_PATH%"
)

exit /b %errorlevel%
