#pragma once
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace Editor {

struct MemoryPluginEntry {
    char typeId[128];
    char instanceId[128];
};

class EditorMemory {
public:
    EditorMemory();

    bool Load(const char* path);      // Returns false if file missing/corrupt
    bool Save(const char* path) const;

    const Json::Value& GetLayoutTree() const;
    void SetLayoutTree(const Json::Value& tree);

    unsigned int GetPluginCount() const;
    const MemoryPluginEntry& GetPlugin(unsigned int index) const;
    void AddPlugin(const char* typeId, const char* instanceId);
    void ClearPlugins();

    const char* GetLastProject() const;
    void SetLastProject(const char* path);

private:
    Json::Value mLayoutTree;
    static const unsigned int kMaxMemoryPlugins = 16;
    Dia::Core::Containers::DynamicArrayC<MemoryPluginEntry, kMaxMemoryPlugins> mPlugins;
    char mLastProject[512];
};

}} // namespace Dia::Editor
