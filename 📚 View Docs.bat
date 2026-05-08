@echo off
REM Documentation Viewer Launcher
REM Starts MkDocs server and opens browser

echo.
echo ============================================
echo   Cluiche Documentation Viewer
echo ============================================
echo.
echo Starting documentation server...
echo.

REM Check if venv exists
if not exist "venv\Scripts\activate.bat" (
    echo ERROR: Virtual environment not found!
    echo Please run: python -m venv venv
    echo Then run: venv\Scripts\pip install -r docs\requirements.txt
    pause
    exit /b 1
)

REM Activate virtual environment and start server
call venv\Scripts\activate

echo Clearing documentation cache...
if exist ".mkdocs-site" rmdir /s /q ".mkdocs-site" >nul 2>&1
if exist "site" rmdir /s /q "site" >nul 2>&1
echo Cache cleared.
echo.

echo Server starting at http://127.0.0.1:8000
echo.
echo Opening browser in 2 seconds...
echo.

REM Wait 2 seconds then open browser
start /b timeout /t 2 /nobreak >nul
start http://127.0.0.1:8000

echo ============================================
echo   Documentation viewer is now running!
echo
echo   Browser: http://127.0.0.1:8000
echo
echo   Press Ctrl+C to stop the server
echo ============================================
echo.

REM Start MkDocs server
mkdocs serve
