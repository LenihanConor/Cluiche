@echo off
cd ..
call venv\Scripts\activate

echo Clearing documentation cache...
if exist ".mkdocs-site" rmdir /s /q ".mkdocs-site" >nul 2>&1
if exist "site" rmdir /s /q "site" >nul 2>&1
echo Cache cleared.
echo.

mkdocs serve
