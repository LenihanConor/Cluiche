////////////////////////////////////////////////////////////////////////////////
// CluicheEditor entry point — v2 bootstrap
//
// Project path injection:
//   The project path is NOT parsed here.  EditorModelModule::DoStart() reads
//   GetCommandLineW() directly, which is process-wide and always available.
//   This avoids any coupling between Main and individual modules.
//
// Module registration:
//   Each module's .cpp registers itself at static-init time via DIA_MODULE.
//   As long as those translation units are linked in, no explicit include or
//   call is needed here.
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <stdio.h>
#include <include/cef_app.h>

#include <DiaUICEF/CEFProcessHandler.h>

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestLoaderV2.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    // CEF subprocess guard: must come before ANY Dia initialization.
    // Pass a CEFProcessHandler so renderer/helper processes register the
    // dia:// custom scheme (OnRegisterCustomSchemes runs in all processes).
    CefMainArgs mainArgs(hInstance);
    CefRefPtr<Dia::UICEF::CEFProcessHandler> cefApp = new Dia::UICEF::CEFProcessHandler("");
    int exitCode = CefExecuteProcess(mainArgs, cefApp.get(), nullptr);
    if (exitCode >= 0)
        return exitCode;

    // Load the editor application manifest.
    // The manifest describes processing units, modules, stages, and streams.
    Dia::ApplicationFlow::ApplicationManifestV2 manifest;
    Dia::ApplicationFlow::LoadResult loadResult =
        Dia::ApplicationFlow::ApplicationManifestLoaderV2::LoadFromFile("Data/editor.diaapp", manifest);

    if (loadResult != Dia::ApplicationFlow::LoadResult::kSuccess)
    {
        printf("CluicheEditor: failed to load manifest 'Data/editor.diaapp' (result: %d)\n",
               static_cast<int>(loadResult));
        return 1;
    }

    // Create and run the application.
    // TypeRegistry::Global() is populated by DIA_MODULE static registrations.
    Dia::ApplicationFlow::TypeRegistry& registry = Dia::ApplicationFlow::TypeRegistry::Global();
    Dia::ApplicationFlow::Application app(manifest, registry);

    if (!app.Start())
    {
        printf("CluicheEditor: application failed to start\n");
        return 1;
    }

    // Main loop — EditorPU runs at 60 Hz (inline on the main thread).
    // Application::Update returns false once all modules are inactive
    // (i.e. the application has fully shut down).
    const float kFrameTimeSec = 1.0f / 60.0f;
    while (app.Update(kFrameTimeSec))
    {
    }

    return 0;
}
