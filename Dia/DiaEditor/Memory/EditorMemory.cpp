#include "EditorMemory.h"
#include <fstream>
#include <cstring>

namespace Dia { namespace Editor {

EditorMemory::EditorMemory()
    : mLayoutTree(Json::nullValue)
    , mPlugins()
{
    mLastProject[0] = '\0';
}

bool EditorMemory::Load(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return false;
    }

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(file, root))
    {
        return false;
    }

    if (root.get("version", 0).asInt() != 1)
    {
        return false;
    }

    mLayoutTree = root.get("layout", Json::nullValue);

    mPlugins.RemoveAll();
    const Json::Value& plugins = root.get("plugins", Json::nullValue);
    if (plugins.isArray())
    {
        for (Json::ArrayIndex i = 0; i < plugins.size() && i < kMaxMemoryPlugins; ++i)
        {
            const Json::Value& entry = plugins[i];
            MemoryPluginEntry plugin;
            strncpy_s(plugin.typeId, sizeof(plugin.typeId),
                entry.get("type", "").asCString(), _TRUNCATE);
            strncpy_s(plugin.instanceId, sizeof(plugin.instanceId),
                entry.get("instance_id", "").asCString(), _TRUNCATE);
            mPlugins.Add(plugin);
        }
    }

    strncpy_s(mLastProject, sizeof(mLastProject),
        root.get("last_project", "").asCString(), _TRUNCATE);

    return true;
}

bool EditorMemory::Save(const char* path) const
{
    Json::Value root;
    root["version"] = 1;

    if (!mLayoutTree.isNull())
    {
        root["layout"] = mLayoutTree;
    }

    Json::Value pluginsArray(Json::arrayValue);
    for (unsigned int i = 0; i < static_cast<unsigned int>(mPlugins.Size()); ++i)
    {
        const MemoryPluginEntry& entry = mPlugins[i];
        Json::Value pluginEntry;
        pluginEntry["type"] = entry.typeId;
        pluginEntry["instance_id"] = entry.instanceId;
        pluginsArray.append(pluginEntry);
    }
    root["plugins"] = pluginsArray;

    root["last_project"] = mLastProject;

    std::ofstream file(path);
    if (!file.is_open())
    {
        return false;
    }

    Json::StyledWriter writer;
    file << writer.write(root);
    return file.good();
}

const Json::Value& EditorMemory::GetLayoutTree() const
{
    return mLayoutTree;
}

void EditorMemory::SetLayoutTree(const Json::Value& tree)
{
    mLayoutTree = tree;
}

unsigned int EditorMemory::GetPluginCount() const
{
    return static_cast<unsigned int>(mPlugins.Size());
}

const MemoryPluginEntry& EditorMemory::GetPlugin(unsigned int index) const
{
    return mPlugins[index];
}

void EditorMemory::AddPlugin(const char* typeId, const char* instanceId)
{
    if (!mPlugins.IsFull())
    {
        MemoryPluginEntry entry;
        strncpy_s(entry.typeId, sizeof(entry.typeId), typeId, _TRUNCATE);
        strncpy_s(entry.instanceId, sizeof(entry.instanceId), instanceId, _TRUNCATE);
        mPlugins.Add(entry);
    }
}

void EditorMemory::ClearPlugins()
{
    mPlugins.RemoveAll();
}

const char* EditorMemory::GetLastProject() const
{
    return mLastProject;
}

void EditorMemory::SetLastProject(const char* path)
{
    strncpy_s(mLastProject, sizeof(mLastProject), path, _TRUNCATE);
}

}} // namespace Dia::Editor
