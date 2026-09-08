#pragma once

#ifdef DIA_DEBUG

#include <DiaUI/Page.h>

namespace Dia { namespace UI { class BoundMethodArgs; } }

namespace Cluiche { namespace AppFlow {

// C++ shell for debug-panel.html (DiaDebugPanel, SD-001).
//
// The page exposes a single bound method to JavaScript:
//
//   app.onCommand(domainId, cmd, argsJson)
//
// Three separate string arguments are used deliberately rather than one
// serialised JSON blob: Ultralight marshals every JS string through
// Dia::Core::Containers::String64, which truncates at 63 characters. A single
// {domainId, cmd, args} envelope routinely exceeds that; the three fields
// individually do not.
//
// State flows the other way via IUISystem::CallJSFunction("updateDomainState", ...)
// which DebugPanelPageModule drives once per frame.
class DebugPanelPage : public Dia::UI::Page
{
public:
    struct ICallbacks
    {
        virtual ~ICallbacks() = default;

        // domainId — IDebugDomain::GetDomainId() source string, or "__global__"
        //            for panel-wide commands (e.g. the global scale slider).
        // cmd      — "toggle", "setScale", ...
        // argsJson — compact JSON object, never null (defaults to "{}").
        virtual void OnCommand(const char* domainId,
                               const char* cmd,
                               const char* argsJson) = 0;
    };

    explicit DebugPanelPage(ICallbacks* callbacks);

    void InitializePage();

private:
    void OnCommand_JS(const Dia::UI::BoundMethodArgs& args);

    ICallbacks* mCallbacks;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
