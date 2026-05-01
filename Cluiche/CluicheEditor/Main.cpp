////////////////////////////////////////////////////////////////////////////////
// CluicheEditor entry point
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <include/cef_app.h>

#include <DiaUICEF/CEFProcessHandler.h>

#include "ApplicationFlow/ProcessingUnits/CluicheEditorProcessingUnit.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR lpCmdLine, int /*nCmdShow*/)
{
	// CEF subprocess guard: must come before ANY Dia initialization.
	// Pass a CEFProcessHandler so renderer/helper processes register the
	// dia:// custom scheme (OnRegisterCustomSchemes runs in all processes).
	CefMainArgs mainArgs(hInstance);
	CefRefPtr<Dia::UICEF::CEFProcessHandler> app = new Dia::UICEF::CEFProcessHandler("");
	int exitCode = CefExecuteProcess(mainArgs, app.get(), nullptr);
	if (exitCode >= 0)
		return exitCode;

	// Main editor process
	Cluiche::Editor::CluicheEditorProcessingUnit* editorPU =
		new Cluiche::Editor::CluicheEditorProcessingUnit();

	// Parse the single project path argument (strip surrounding quotes, convert to UTF-8).
	if (lpCmdLine != nullptr && lpCmdLine[0] != L'\0')
	{
		LPWSTR arg = lpCmdLine;
		while (*arg == L' ' || *arg == L'\t') ++arg;
		if (*arg == L'"')
		{
			++arg;
			LPWSTR end = arg;
			while (*end && *end != L'"') ++end;
			*end = L'\0';
		}

		char utf8[260] = { 0 };
		WideCharToMultiByte(CP_UTF8, 0, arg, -1, utf8, sizeof(utf8), nullptr, nullptr);
		editorPU->SetProjectPath(utf8);
	}

	editorPU->Start();
	editorPU->Update();
	editorPU->Stop();

	delete editorPU;
	return 0;
}
