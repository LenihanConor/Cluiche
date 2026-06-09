#include "DiaPythonConsoleEditor/DiaPythonConsolePlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/EditorAPI/EditorActionRegistryService.h>
#include <DiaEditor/EditorAPI/EditorActionDescriptor.h>
#include <DiaEditor/EditorAPI/EditorActionRegistry.h>
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

    RegisterHandler(
        Dia::Core::StringCRC("python_console.run_file"),
        [](const Json::Value& params) -> Json::Value
        {
            if (!params.isMember("path") || params["path"].asString().empty())
            {
                return MakeErrorResponse("path parameter is required");
            }

            std::string path = params["path"].asString();
            std::string capturedStdout;
            std::string capturedStderr;

            Dia::Python::RedirectOutput(
                [&capturedStdout](const char* text) { capturedStdout += text; },
                [&capturedStderr](const char* text) { capturedStderr += text; }
            );

            int exitCode = Dia::Python::ExecuteScript(path.c_str());

            Dia::Python::RestoreOutput();

            Json::Value result;
            result["stdout"] = capturedStdout;
            result["stderr"] = capturedStderr;
            result["exitCode"] = exitCode;
            return MakeSuccessResponse(result);
        }
    );

    // Dual-register both actions in EditorActionRegistry so they are callable from
    // Python via dia_editor.python_console.execute / dia_editor.python_console.run_file
    // without going through the WebUIBridge. Handlers are identical to the WebUIBridge path above.
    if (GetServices() != nullptr)
    {
        Dia::Editor::EditorActionRegistryService* regSvc =
            GetServices()->GetService<Dia::Editor::EditorActionRegistryService>();
        if (regSvc != nullptr && regSvc->GetRegistry() != nullptr)
        {
            Dia::Editor::EditorActionRegistry* api = regSvc->GetRegistry();

            Dia::Editor::EditorActionDescriptor executeAction;
            executeAction.name           = Dia::Core::StringCRC("python_console.execute");
            executeAction.description    = "Execute a Python code string in the editor's embedded interpreter. Required param: code (string). Returns { stdout: string, stderr: string, returnValue: string }. Stdout/stderr are captured via DiaPython::RedirectOutput scoped to the execution. Errors (syntax errors, exceptions) appear in stderr rather than raising. Runs on main thread.";
            executeAction.category       = "python_console";
            executeAction.owner          = "DiaPythonConsolePlugin";
            executeAction.dispatchThread = Dia::Editor::DispatchThread::kMainThread;
            executeAction.handler        = [](const Json::Value& params) -> Json::Value
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
            };
            api->RegisterAction(executeAction);

            Dia::Editor::EditorActionDescriptor runFileAction;
            runFileAction.name           = Dia::Core::StringCRC("python_console.run_file");
            runFileAction.description    = "Execute a Python script file in the editor's embedded interpreter. Required param: path (string, absolute or relative to editor working directory). Returns { stdout: string, stderr: string, exitCode: int }. Exit code 0 indicates success; non-zero indicates an exception or error. Runs on main thread.";
            runFileAction.category       = "python_console";
            runFileAction.owner          = "DiaPythonConsolePlugin";
            runFileAction.dispatchThread = Dia::Editor::DispatchThread::kMainThread;
            runFileAction.handler        = [](const Json::Value& params) -> Json::Value
            {
                if (!params.isMember("path") || params["path"].asString().empty())
                {
                    return MakeErrorResponse("path parameter is required");
                }

                std::string path = params["path"].asString();
                std::string capturedStdout;
                std::string capturedStderr;

                Dia::Python::RedirectOutput(
                    [&capturedStdout](const char* text) { capturedStdout += text; },
                    [&capturedStderr](const char* text) { capturedStderr += text; }
                );

                int exitCode = Dia::Python::ExecuteScript(path.c_str());

                Dia::Python::RestoreOutput();

                Json::Value result;
                result["stdout"] = capturedStdout;
                result["stderr"] = capturedStderr;
                result["exitCode"] = exitCode;
                return MakeSuccessResponse(result);
            };
            api->RegisterAction(runFileAction);

            DIA_LOG_INFO("Editor", "DiaPythonConsolePlugin: Dual-registered execute/run_file actions in EditorActionRegistry");
        }
    }
}

void DiaPythonConsolePlugin::OnPluginUnload()
{
    DIA_LOG_INFO("Editor", "DiaPythonConsolePlugin: OnPluginUnload");
}

} // namespace Dia::PythonConsole
