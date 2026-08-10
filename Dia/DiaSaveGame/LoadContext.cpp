#include "DiaSaveGame/LoadContext.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Core/Assert.h>

#include <cstring>

namespace Dia::SaveGame {

LoadContext::LoadContext(const Json::Value& root)
    : mRoot(&root)
{
    StackEntry entry;
    entry.node       = mRoot;
    entry.arrayIndex = -1;
    mStack.Add(entry);
}

const Json::Value& LoadContext::Current() const
{
    DIA_ASSERT(mStack.Size() > 0, "LoadContext stack underflow");
    const StackEntry& top = mStack[mStack.Size() - 1];
    if (top.arrayIndex >= 0)
        return (*top.node)[static_cast<Json::ArrayIndex>(top.arrayIndex)];
    return *top.node;
}

const Json::Value& LoadContext::Root() const
{
    return *mRoot;
}

const Json::Value& LoadContext::CurrentNode() const
{
    return Current();
}

bool LoadContext::Read(Dia::Core::StringCRC key, int32_t& out) const
{
    const Json::Value& node = Current();
    if (!node.isMember(key.AsChar()) || !node[key.AsChar()].isIntegral())
        return false;
    out = node[key.AsChar()].asInt();
    return true;
}

bool LoadContext::Read(Dia::Core::StringCRC key, float& out) const
{
    const Json::Value& node = Current();
    if (!node.isMember(key.AsChar()) || !node[key.AsChar()].isNumeric())
        return false;
    out = node[key.AsChar()].asFloat();
    return true;
}

bool LoadContext::Read(Dia::Core::StringCRC key, bool& out) const
{
    const Json::Value& node = Current();
    if (!node.isMember(key.AsChar()) || !node[key.AsChar()].isBool())
        return false;
    out = node[key.AsChar()].asBool();
    return true;
}

bool LoadContext::Read(Dia::Core::StringCRC key, char* outBuffer, unsigned int bufferSize) const
{
    DIA_ASSERT(outBuffer != nullptr, "LoadContext::Read null buffer");
    const Json::Value& node = Current();
    if (!node.isMember(key.AsChar()) || !node[key.AsChar()].isString())
        return false;
    const char* str = node[key.AsChar()].asCString();
    unsigned int len = static_cast<unsigned int>(strlen(str));
    if (len + 1 > bufferSize)
        return false;
    memcpy(outBuffer, str, len + 1);
    return true;
}

bool LoadContext::BeginObject(Dia::Core::StringCRC key)
{
    DIA_ASSERT(!mStack.IsFull(), "LoadContext: nesting depth exceeded");
    const Json::Value& node = Current();
    if (!node.isMember(key.AsChar()) || !node[key.AsChar()].isObject())
        return false;

    StackEntry entry;
    entry.node       = &node[key.AsChar()];
    entry.arrayIndex = -1;
    mStack.Add(entry);
    return true;
}

void LoadContext::EndObject()
{
    DIA_ASSERT(mStack.Size() > 1, "LoadContext: EndObject without matching BeginObject");
    mStack.Remove();
}

bool LoadContext::BeginArray(Dia::Core::StringCRC key, uint32_t& countOut)
{
    DIA_ASSERT(!mStack.IsFull(), "LoadContext: nesting depth exceeded");
    const Json::Value& node = Current();
    if (!node.isMember(key.AsChar()) || !node[key.AsChar()].isArray())
        return false;

    countOut = node[key.AsChar()].size();

    StackEntry entry;
    entry.node       = &node[key.AsChar()];
    entry.arrayIndex = -1;
    mStack.Add(entry);
    return true;
}

void LoadContext::EndArray()
{
    DIA_ASSERT(mStack.Size() > 1, "LoadContext: EndArray without matching BeginArray");
    mStack.Remove();
}

void LoadContext::SetArrayIndex(uint32_t index)
{
    DIA_ASSERT(mStack.Size() > 0, "LoadContext: SetArrayIndex with empty stack");
    StackEntry& top = mStack[mStack.Size() - 1];
    DIA_ASSERT(top.node->isArray(), "LoadContext: SetArrayIndex on non-array node");
    DIA_ASSERT(index < top.node->size(), "LoadContext: SetArrayIndex out of bounds");
    top.arrayIndex = static_cast<int32_t>(index);
}

} // namespace Dia::SaveGame
