#include "Modules/DebugPanelPage.h"

#ifdef DIA_DEBUG

#include <DiaUI/BoundMethod.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace AppFlow {

DebugPanelPage::DebugPanelPage(ICallbacks* callbacks)
    : Dia::UI::Page()
    , mCallbacks(callbacks)
{}

void DebugPanelPage::InitializePage()
{
    Initialize(Dia::Core::FilePath("ui_common", "debug-panel.html"));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("onCommand",
        Dia::UI::BoundMethod::MethodPtr(this, &DebugPanelPage::OnCommand_JS)));
}

void DebugPanelPage::OnCommand_JS(const Dia::UI::BoundMethodArgs& args)
{
    if (mCallbacks == nullptr)
        return;

    if (args.Size() < 2)
    {
        DIA_LOG_WARNING("Debug", "DebugPanelPage: onCommand needs (domainId, cmd[, argsJson]); got %u args",
                        args.Size());
        return;
    }

    const Dia::UI::BoundMethodValue& domainArg = args.At(0);
    const Dia::UI::BoundMethodValue& cmdArg    = args.At(1);

    if (!domainArg.IsString() || !cmdArg.IsString())
    {
        DIA_LOG_WARNING("Debug", "DebugPanelPage: onCommand domainId and cmd must be strings");
        return;
    }

    const char* argsJson = "{}";
    if (args.Size() >= 3 && args.At(2).IsString())
        argsJson = args.At(2).GetString().AsCStr();

    mCallbacks->OnCommand(domainArg.GetString().AsCStr(),
                          cmdArg.GetString().AsCStr(),
                          argsJson);
}

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
