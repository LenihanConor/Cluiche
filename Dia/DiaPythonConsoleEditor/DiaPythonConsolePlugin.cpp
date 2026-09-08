#include "DiaPythonConsoleEditor/DiaPythonConsolePlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/EditorAPI/EditorActionRegistryService.h>
#include <DiaEditor/EditorAPI/EditorActionDescriptor.h>
#include <DiaEditor/EditorAPI/EditorActionRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaPython/DiaPython.h>

#include <string>

using namespace Dia::PythonConsole;

REGISTER_EDITOR_PLUGIN(DiaPythonConsolePlugin, "DiaPythonConsole")

const Dia::Core::StringCRC DiaPythonConsolePlugin::kUniqueId("python_console");

namespace
{
    struct OutputRedirectionGuard
    {
        ~OutputRedirectionGuard() { Dia::Python::RestoreOutput(); }
    };

    Json::Value HandleExecute(const Json::Value& params)
    {
        if (!params.isMember("code"))
            return Dia::Editor::EditorPluginBase::MakeErrorResponse("code parameter is required");

        std::string code = params["code"].asString();
        if (code.empty())
            return Dia::Editor::EditorPluginBase::MakeErrorResponse("code parameter is required");

        std::string capturedStdout;
        std::string capturedStderr;

        Dia::Python::RedirectOutput(
            [&capturedStdout](const char* text) { capturedStdout += text; },
            [&capturedStderr](const char* text) { capturedStderr += text; }
        );
        OutputRedirectionGuard guard;

        Dia::Python::ExecuteString(code.c_str());

        Json::Value result;
        result["stdout"] = capturedStdout;
        result["stderr"] = capturedStderr;
        result["returnValue"] = "";
        return Dia::Editor::EditorPluginBase::MakeSuccessResponse(result);
    }

    Json::Value HandleRunFile(const Json::Value& params)
    {
        if (!params.isMember("path"))
            return Dia::Editor::EditorPluginBase::MakeErrorResponse("path parameter is required");

        std::string path = params["path"].asString();
        if (path.empty())
            return Dia::Editor::EditorPluginBase::MakeErrorResponse("path parameter is required");

        std::string capturedStdout;
        std::string capturedStderr;

        Dia::Python::RedirectOutput(
            [&capturedStdout](const char* text) { capturedStdout += text; },
            [&capturedStderr](const char* text) { capturedStderr += text; }
        );
        OutputRedirectionGuard guard;

        int exitCode = Dia::Python::ExecuteScript(path.c_str());

        Json::Value result;
        result["stdout"] = capturedStdout;
        result["stderr"] = capturedStderr;
        result["exitCode"] = exitCode;
        return Dia::Editor::EditorPluginBase::MakeSuccessResponse(result);
    }
} // anonymous namespace

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

    if (!Dia::Python::IsInitialized())
    {
        bool ok = Dia::Python::Initialize("External/Python311/", "External/Python/", false);
        if (!ok)
            DIA_LOG_ERROR("Editor", "DiaPythonConsolePlugin: Failed to initialize Python interpreter");
    }

    RegisterHandler(Dia::Core::StringCRC("python_console.execute"), HandleExecute);
    RegisterHandler(Dia::Core::StringCRC("python_console.run_file"), HandleRunFile);

    if (GetServices() != nullptr)
    {
        Dia::Editor::EditorActionRegistryService* regSvc =
            GetServices()->GetService<Dia::Editor::EditorActionRegistryService>();
        if (regSvc != nullptr && regSvc->GetRegistry() != nullptr)
        {
            Dia::Editor::EditorActionRegistry* api = regSvc->GetRegistry();

            Dia::Editor::EditorActionDescriptor executeAction;
            executeAction.name           = Dia::Core::StringCRC("python_console.execute");
            executeAction.description    = "Execute a Python code string in the editor's embedded interpreter. Required param: code (string). Returns { stdout, stderr, returnValue }. Runs on main thread.";
            executeAction.category       = "python_console";
            executeAction.owner          = "DiaPythonConsolePlugin";
            executeAction.dispatchThread = Dia::Editor::DispatchThread::kMainThread;
            executeAction.handler        = HandleExecute;
            api->RegisterAction(executeAction);

            Dia::Editor::EditorActionDescriptor runFileAction;
            runFileAction.name           = Dia::Core::StringCRC("python_console.run_file");
            runFileAction.description    = "Execute a Python script file. Required param: path (string, absolute). Returns { stdout, stderr, exitCode }. Runs on main thread.";
            runFileAction.category       = "python_console";
            runFileAction.owner          = "DiaPythonConsolePlugin";
            runFileAction.dispatchThread = Dia::Editor::DispatchThread::kMainThread;
            runFileAction.handler        = HandleRunFile;
            api->RegisterAction(runFileAction);

            DIA_LOG_INFO("Editor", "DiaPythonConsolePlugin: Dual-registered execute/run_file actions in EditorActionRegistry");
        }
    }
}

void DiaPythonConsolePlugin::OnPluginUnload()
{
    DIA_LOG_INFO("Editor", "DiaPythonConsolePlugin: OnPluginUnload");

    if (GetServices() != nullptr)
    {
        Dia::Editor::EditorActionRegistryService* regSvc =
            GetServices()->GetService<Dia::Editor::EditorActionRegistryService>();
        if (regSvc != nullptr && regSvc->GetRegistry() != nullptr)
        {
            regSvc->GetRegistry()->DeregisterActionsForOwner(Dia::Core::StringCRC("DiaPythonConsolePlugin"));
        }
    }

    Dia::Python::Shutdown();
}

} // namespace Dia::PythonConsole
