#pragma once
#include <DiaEditor/Plugin/EditorPluginBase.h>

namespace Dia::PythonConsole {

class DiaPythonConsolePlugin : public Dia::Editor::EditorPluginBase
{
public:
    DiaPythonConsolePlugin();

    void OnPluginLoad() override;
    void OnPluginUnload() override;

    static const Dia::Core::StringCRC kUniqueId;
};

} // namespace Dia::PythonConsole
