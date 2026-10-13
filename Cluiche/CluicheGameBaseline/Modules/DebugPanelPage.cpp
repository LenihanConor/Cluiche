#include "Modules/DebugPanelPage.h"

#ifdef DIA_DEBUG

#include <DiaUI/BoundMethod.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cstring>

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

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("onConsoleLine",
        Dia::UI::BoundMethod::MethodPtr(this, &DebugPanelPage::OnConsoleLine_JS)));
    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("onConsoleComplete",
        Dia::UI::BoundMethod::MethodPtr(this, &DebugPanelPage::OnConsoleComplete_JS)));
    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("onConsoleHistory",
        Dia::UI::BoundMethod::MethodPtr(this, &DebugPanelPage::OnConsoleHistory_JS)));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("onPaletteOpen",
        Dia::UI::BoundMethod::MethodPtr(this, &DebugPanelPage::OnPaletteOpen_JS)));
    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("onPaletteSearch",
        Dia::UI::BoundMethod::MethodPtr(this, &DebugPanelPage::OnPaletteSearch_JS)));
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

void DebugPanelPage::OnConsoleLine_JS(const Dia::UI::BoundMethodArgs& args)
{
    if (mCallbacks == nullptr)
        return;

    // (seq, part0[, part1, part2, part3])
    if (args.Size() < 2 || args.Size() > 5 || !(args.At(0).IsInteger() || args.At(0).IsDouble()))
    {
        DIA_LOG_WARNING("Debug", "DebugPanelPage: onConsoleLine needs (seq, part0..part3); got %u args",
                        args.Size());
        return;
    }

    const char* parts[4] = {};
    unsigned int partCount = 0;
    for (unsigned int i = 1; i < args.Size(); ++i)
    {
        if (!args.At(i).IsString())
        {
            DIA_LOG_WARNING("Debug", "DebugPanelPage: onConsoleLine part %u is not a string", i - 1);
            return;
        }
        parts[partCount++] = args.At(i).GetString().AsCStr();
    }

    const double seq = args.At(0).IsInteger() ? static_cast<double>(args.At(0).GetInteger())
                                              : args.At(0).GetDouble();
    mCallbacks->OnConsoleLine(seq < 0.0 ? 0u : static_cast<unsigned int>(seq), parts, partCount);
}

void DebugPanelPage::OnConsoleComplete_JS(const Dia::UI::BoundMethodArgs& args)
{
    if (mCallbacks == nullptr)
        return;

    const char* token = "";
    if (args.Size() >= 1 && args.At(0).IsString())
        token = args.At(0).GetString().AsCStr();

    mCallbacks->OnConsoleComplete(token);
}

void DebugPanelPage::OnConsoleHistory_JS(const Dia::UI::BoundMethodArgs& args)
{
    if (mCallbacks == nullptr)
        return;

    if (args.Size() < 1 || !args.At(0).IsString())
    {
        DIA_LOG_WARNING("Debug", "DebugPanelPage: onConsoleHistory needs (\"prev\"|\"next\")");
        return;
    }

    const char* direction = args.At(0).GetString().AsCStr();
    if (strcmp(direction, "prev") == 0)
        mCallbacks->OnConsoleHistory(true);
    else if (strcmp(direction, "next") == 0)
        mCallbacks->OnConsoleHistory(false);
    else
        DIA_LOG_WARNING("Debug", "DebugPanelPage: onConsoleHistory direction '%s' is not prev/next", direction);
}

void DebugPanelPage::OnPaletteOpen_JS(const Dia::UI::BoundMethodArgs& /*args*/)
{
    if (mCallbacks != nullptr)
        mCallbacks->OnPaletteOpen();
}

void DebugPanelPage::OnPaletteSearch_JS(const Dia::UI::BoundMethodArgs& args)
{
    if (mCallbacks == nullptr)
        return;

    // (query, mask) where mask is the string "1", "2" or "3".
    if (args.Size() < 2 || !args.At(0).IsString() || !args.At(1).IsString())
    {
        DIA_LOG_WARNING("Debug", "DebugPanelPage: onPaletteSearch needs (query, mask) strings; got %u args",
                        args.Size());
        return;
    }

    const char* maskText = args.At(1).GetString().AsCStr();
    int mask = 0;
    if (strcmp(maskText, "1") == 0)      mask = 1;
    else if (strcmp(maskText, "2") == 0) mask = 2;
    else if (strcmp(maskText, "3") == 0) mask = 3;
    else
    {
        DIA_LOG_WARNING("Debug", "DebugPanelPage: onPaletteSearch mask '%s' is not 1/2/3", maskText);
        return;
    }

    mCallbacks->OnPaletteSearch(args.At(0).GetString().AsCStr(), mask);
}

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
