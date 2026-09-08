#include "DiaSaveGame/SlotManager.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "DiaCore/Core/Assert.h"

namespace Dia::SaveGame {

//------------------------------------------------------------------------------------
void SlotManager::Init(const SaveConfig& config)
{
    mConfig = &config;
}

//------------------------------------------------------------------------------------
void SlotManager::BuildPath(Dia::Core::StringCRC slotId, char* outPath, uint32_t outPathLen) const
{
    DIA_ASSERT(mConfig != nullptr, "SlotManager not initialised");
    DIA_ASSERT(outPath != nullptr, "outPath must not be null");
    DIA_ASSERT(outPathLen > 0, "outPathLen must be greater than zero");

    // Locate {id} placeholder in slotPattern
    const char* pattern = mConfig->slotPattern;
    const char* placeholder = strstr(pattern, "{id}");

    if (placeholder != nullptr)
    {
        // Build: baseDirectory + pattern[0..placeholder) + slotId + pattern[placeholder+4..)
        const ptrdiff_t prefixLen = placeholder - pattern;
        snprintf(outPath, outPathLen, "%s%.*s%s%s",
            mConfig->baseDirectory,
            static_cast<int>(prefixLen), pattern,
            slotId.AsChar(),
            placeholder + 4); // skip past "{id}"
    }
    else
    {
        // No placeholder — just concatenate base and pattern
        snprintf(outPath, outPathLen, "%s%s", mConfig->baseDirectory, pattern);
    }
}

//------------------------------------------------------------------------------------
void SlotManager::EnumerateSlots(Dia::Core::Containers::DynamicArrayC<SlotInfo, 32>& out) const
{
    DIA_ASSERT(mConfig != nullptr, "SlotManager not initialised");

    char searchPath[512];
    snprintf(searchPath, sizeof(searchPath), "%s*.sav", mConfig->baseDirectory);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do
    {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
        }

        DIA_ASSERT(!out.IsFull(), "slot enumeration overflow");
        if (out.IsFull())
        {
            break;
        }

        SlotInfo info;

        // Build full path
        snprintf(info.path, sizeof(info.path), "%s%s", mConfig->baseDirectory, findData.cFileName);

        // Derive slot id: strip directory prefix and .sav extension from filename
        char idBuffer[256];
        snprintf(idBuffer, sizeof(idBuffer), "%s", findData.cFileName);
        const size_t nameLen = strlen(idBuffer);
        if (nameLen > 4 && strcmp(idBuffer + nameLen - 4, ".sav") == 0)
        {
            idBuffer[nameLen - 4] = '\0'; // strip ".sav"
        }
        info.id = Dia::Core::StringCRC(idBuffer);

        out.Add(info);

    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

//------------------------------------------------------------------------------------
bool SlotManager::SlotExists(Dia::Core::StringCRC slotId) const
{
    DIA_ASSERT(mConfig != nullptr, "SlotManager not initialised");

    char path[512];
    BuildPath(slotId, path, sizeof(path));
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

//------------------------------------------------------------------------------------
void SlotManager::DeleteSlot(Dia::Core::StringCRC slotId) const
{
    DIA_ASSERT(mConfig != nullptr, "SlotManager not initialised");

    char path[512];
    BuildPath(slotId, path, sizeof(path));
    DeleteFileA(path);
}

//------------------------------------------------------------------------------------
void SlotManager::RenameSlot(Dia::Core::StringCRC fromId, Dia::Core::StringCRC toId) const
{
    DIA_ASSERT(mConfig != nullptr, "SlotManager not initialised");

    char fromPath[512];
    char toPath[512];
    BuildPath(fromId, fromPath, sizeof(fromPath));
    BuildPath(toId, toPath, sizeof(toPath));
    MoveFileA(fromPath, toPath);
}

//------------------------------------------------------------------------------------
bool SlotManager::IsMaxSlotsReached() const
{
    DIA_ASSERT(mConfig != nullptr, "SlotManager not initialised");

    if (mConfig->maxSlots == 0)
    {
        return false;
    }

    Dia::Core::Containers::DynamicArrayC<SlotInfo, 32> slots;
    EnumerateSlots(slots);
    return slots.Size() >= mConfig->maxSlots;
}

} // namespace Dia::SaveGame
