////////////////////////////////////////////////////////////////////////////////
// CluicheEditor entry point
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <include/cef_app.h>

#include <DiaUICEF/CEFProcessHandler.h>

#include "ApplicationFlow/ProcessingUnits/CluicheEditorProcessingUnit.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nCmdShow*/)
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

	editorPU->Start();
	editorPU->Update();
	editorPU->Stop();

	delete editorPU;
	return 0;
}
