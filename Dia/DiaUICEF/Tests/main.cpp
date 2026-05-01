////////////////////////////////////////////////////////////////////////////////
// DiaUICEF Test Subprocess
// CEF helper process for testing DiaUICEF in isolation (dev/test only).
// Production applications (e.g. CluicheEditor) handle this via CefExecuteProcess()
// at the top of their own main(), making this exe unnecessary at that point.
////////////////////////////////////////////////////////////////////////////////

#include <include/cef_app.h>
#include <windows.h>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
	CefMainArgs main_args(hInstance);
	return CefExecuteProcess(main_args, nullptr, nullptr);
}
