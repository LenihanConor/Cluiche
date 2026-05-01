' Silent launcher for documentation viewer
' Hides the command window and just opens the browser

Set WshShell = CreateObject("WScript.Shell")
Set objFSO = CreateObject("Scripting.FileSystemObject")

' Get the script's directory (docs folder)
strDocsPath = objFSO.GetParentFolderName(WScript.ScriptFullName)
strRootPath = objFSO.GetParentFolderName(strDocsPath)

' Change to the repository root directory
WshShell.CurrentDirectory = strRootPath

' Start the batch file hidden
WshShell.Run """" & strRootPath & "\📚 View Docs.bat""", 0, False

' Wait a moment for server to start, then open browser
WScript.Sleep 3000
WshShell.Run "http://127.0.0.1:8000"
