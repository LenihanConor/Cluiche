////////////////////////////////////////////////////////////////////////////////
// CluicheEditor entry point
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <include/cef_app.h>

#include "ApplicationFlow/ProcessingUnits/CluicheEditorProcessingUnit.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
	// CEF subprocess guard: must come before ANY Dia initialization.
	// If this is a CEF helper process (renderer, GPU, etc.) CefExecuteProcess returns >= 0.
	CefMainArgs mainArgs(hInstance);
	int exitCode = CefExecuteProcess(mainArgs, nullptr, nullptr);
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
