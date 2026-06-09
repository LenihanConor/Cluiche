#include "DiaPythonConsoleEditor/DiaPythonConsolePlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>

using namespace Dia::PythonConsole;

REGISTER_EDITOR_PLUGIN(DiaPythonConsolePlugin, "DiaPythonConsole")

const Dia::Core::StringCRC DiaPythonConsolePlugin::kUniqueId("python_console");

namespace Dia::PythonConsole {

DiaPythonConsolePlugin::DiaPythonConsolePlugin()
    : EditorPluginBase({
        "Python Console",
        "1.0.0",
        "Interactive Python REPL — execute dia_editor actions and run .py scripts",
        "dia://plugins/pythonconsole/index.html",
        Dia::Editor::LayoutMode::kDockable,
        nullptr,
        nullptr,
        false
    })
{}

void DiaPythonConsolePlugin::OnPluginLoad()
{
    DIA_LOG_INFO("Editor", "DiaPythonConsolePlugin: OnPluginLoad");
}

void DiaPythonConsolePlugin::OnPluginUnload()
{
    DIA_LOG_INFO("Editor", "DiaPythonConsolePlugin: OnPluginUnload");
}

} // namespace Dia::PythonConsole
