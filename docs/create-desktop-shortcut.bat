@echo off
REM Creates a desktop shortcut for the documentation viewer

echo.
echo Creating desktop shortcut for Documentation Viewer...
echo.

set SCRIPT_DIR=%~dp0..
set DESKTOP=%USERPROFILE%\Desktop

REM Create VBScript to make shortcut
echo Set oWS = WScript.CreateObject("WScript.Shell") > CreateShortcut.vbs
echo sLinkFile = "%DESKTOP%\Cluiche Docs.lnk" >> CreateShortcut.vbs
echo Set oLink = oWS.CreateShortcut(sLinkFile) >> CreateShortcut.vbs
echo oLink.TargetPath = "%SCRIPT_DIR%\📚 View Docs.bat" >> CreateShortcut.vbs
echo oLink.WorkingDirectory = "%SCRIPT_DIR%\" >> CreateShortcut.vbs
echo oLink.Description = "Open Cluiche Documentation Viewer" >> CreateShortcut.vbs
echo oLink.IconLocation = "%%SystemRoot%%\System32\imageres.dll,13" >> CreateShortcut.vbs
echo oLink.Save >> CreateShortcut.vbs

REM Execute the VBScript
cscript //nologo CreateShortcut.vbs

REM Clean up
del CreateShortcut.vbs

echo.
echo ============================================
echo   Desktop shortcut created successfully!
echo
echo   Look for "Cluiche Docs" on your desktop
echo ============================================
echo.
pause
