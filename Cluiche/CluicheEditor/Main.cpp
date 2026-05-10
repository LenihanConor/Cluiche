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

#include <chrono>
#include <thread>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    // CEF subprocess guard: must come before ANY Dia initialization.
    CefMainArgs mainArgs(hInstance);
    CefRefPtr<Dia::UICEF::CEFProcessHandler> cefApp = new Dia::UICEF::CEFProcessHandler("");
    int exitCode = CefExecuteProcess(mainArgs, cefApp.get(), nullptr);
    if (exitCode >= 0)
        return exitCode;

    Dia::ApplicationFlow::ApplicationManifestV2 manifest;
    Dia::ApplicationFlow::LoadResult loadResult =
        Dia::ApplicationFlow::ApplicationManifestLoaderV2::LoadFromFile("assets/configs/editor.diaapp", manifest);

    if (loadResult != Dia::ApplicationFlow::LoadResult::kSuccess)
    {
        printf("CluicheEditor: failed to load manifest 'assets/configs/editor.diaapp' (result: %d)\n",
               static_cast<int>(loadResult));
        return 1;
    }

    Dia::ApplicationFlow::TypeRegistry& registry = Dia::ApplicationFlow::TypeRegistry::Global();
    Dia::ApplicationFlow::Application app(manifest, registry);

    if (!app.Start())
    {
        printf("CluicheEditor: application failed to start\n");
        return 1;
    }

    // Frame-paced main loop at 120 Hz.  The editor is GUI-only; an uncapped
    // loop wastes CPU spinning without matching renderer output.  Sleep off
    // any time left in the frame budget after Update returns.
    using Clock    = std::chrono::steady_clock;
    using Duration = std::chrono::duration<float>;
    constexpr Duration kFrameBudget{1.0f / 120.0f};

    auto lastFrameStart = Clock::now();
    while (true)
    {
        const auto frameStart = Clock::now();
        const float deltaTime = std::chrono::duration_cast<Duration>(frameStart - lastFrameStart).count();
        lastFrameStart = frameStart;

        if (!app.Update(deltaTime))
            break;

        const auto elapsed   = Clock::now() - frameStart;
        const auto remaining = kFrameBudget - elapsed;
        if (remaining.count() > 0.0f)
            std::this_thread::sleep_for(remaining);
    }
    return 0;
}
