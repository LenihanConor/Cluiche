#include "DiaPythonConsoleEditor/DiaPythonConsolePlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaPython/ScriptExecution/Script.h>

#include <string>

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

    RegisterHandler(
        Dia::Core::StringCRC("python_console.execute"),
        [](const Json::Value& params) -> Json::Value
        {
            if (!params.isMember("code") || params["code"].asString().empty())
            {
                return MakeErrorResponse("code parameter is required");
            }

            std::string code = params["code"].asString();
            std::string capturedStdout;
            std::string capturedStderr;

            Dia::Python::RedirectOutput(
                [&capturedStdout](const char* text) { capturedStdout += text; },
                [&capturedStderr](const char* text) { capturedStderr += text; }
            );

            Dia::Python::ExecuteString(code.c_str());

            Dia::Python::RestoreOutput();

            Json::Value result;
            result["stdout"] = capturedStdout;
            result["stderr"] = capturedStderr;
            result["returnValue"] = "";
            return MakeSuccessResponse(result);
        }
    );
}

void DiaPythonConsolePlugin::OnPluginUnload()
{
    DIA_LOG_INFO("Editor", "DiaPythonConsolePlugin: OnPluginUnload");
}

} // namespace Dia::PythonConsole
