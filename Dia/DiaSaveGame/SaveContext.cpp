#include "DiaSaveGame/SaveContext.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Core/Assert.h>

#include <cstring>

namespace Dia::SaveGame {

SaveContext::SaveContext()
    : mRoot(new Json::Value(Json::objectValue))
{
    StackEntry root;
    root.node    = mRoot;
    root.isArray = false;
    mStack.Add(root);
}

SaveContext::~SaveContext()
{
    delete mRoot;
}

Json::Value& SaveContext::Current()
{
    DIA_ASSERT(mStack.Size() > 0, "SaveContext stack underflow");
    return *mStack[mStack.Size() - 1].node;
}

Json::Value& SaveContext::Root()
{
    return *mRoot;
}

const Json::Value& SaveContext::Root() const
{
    return *mRoot;
}

Json::Value& SaveContext::CurrentNode()
{
    return Current();
}

void SaveContext::Write(Dia::Core::StringCRC key, int32_t value)
{
    Current()[key.AsChar()] = value;
}

void SaveContext::Write(Dia::Core::StringCRC key, int64_t value)
{
    Current()[key.AsChar()] = value;
}

void SaveContext::Write(Dia::Core::StringCRC key, float value)
{
    Current()[key.AsChar()] = value;
}

void SaveContext::Write(Dia::Core::StringCRC key, bool value)
{
    Current()[key.AsChar()] = value;
}

void SaveContext::Write(Dia::Core::StringCRC key, const char* value)
{
    DIA_ASSERT(value != nullptr, "SaveContext::Write null string");
    Current()[key.AsChar()] = value;
}

void SaveContext::BeginObject(Dia::Core::StringCRC key)
{
    DIA_ASSERT(!mStack.IsFull(), "SaveContext: nesting depth exceeded");
    Json::Value& parent = Current();
    parent[key.AsChar()] = Json::Value(Json::objectValue);

    StackEntry entry;
    entry.node    = &parent[key.AsChar()];
    entry.key     = key;
    entry.isArray = false;
    mStack.Add(entry);
}

void SaveContext::EndObject()
{
    DIA_ASSERT(mStack.Size() > 1, "SaveContext: EndObject without matching BeginObject");
    mStack.Remove();
}

void SaveContext::BeginArray(Dia::Core::StringCRC key)
{
    DIA_ASSERT(!mStack.IsFull(), "SaveContext: nesting depth exceeded");
    Json::Value& parent = Current();
    parent[key.AsChar()] = Json::Value(Json::arrayValue);

    StackEntry entry;
    entry.node    = &parent[key.AsChar()];
    entry.key     = key;
    entry.isArray = true;
    mStack.Add(entry);
}

void SaveContext::EndArray()
{
    DIA_ASSERT(mStack.Size() > 1, "SaveContext: EndArray without matching BeginArray");
    mStack.Remove();
}

bool SaveContext::Flush(char* outBuffer, unsigned int bufferSize) const
{
    DIA_ASSERT(outBuffer != nullptr, "SaveContext::Flush null buffer");
    Json::StyledWriter writer;
    std::string output = writer.write(*mRoot);
    if (output.size() + 1 > bufferSize)
        return false;
    memcpy(outBuffer, output.c_str(), output.size() + 1);
    return true;
}

} // namespace Dia::SaveGame
