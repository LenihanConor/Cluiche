////////////////////////////////////////////////////////////////////////////////
// DiaUICEF Subprocess Entry Point
// CEF launches this executable for renderer, GPU, and plugin processes.
////////////////////////////////////////////////////////////////////////////////

#include <include/cef_app.h>
#include <windows.h>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
	CefMainArgs main_args(hInstance);
	return CefExecuteProcess(main_args, nullptr, nullptr);
}
