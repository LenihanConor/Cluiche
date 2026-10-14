#include "DiaSaveGame/SlotManager.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <windows.h>

#include "DiaCore/Core/Assert.h"
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::SaveGame {

//------------------------------------------------------------------------------------
void SlotManager::Init(const SaveConfig& config)
{
    mConfig = &config;
}

//------------------------------------------------------------------------------------
bool SlotManager::IsValidSlotId(const char* id)
{
    if (id == nullptr || id[0] == '\0' || id[0] == '.')
    {
        return false;
    }

    const size_t len = strlen(id);
    if (len > kMaxSlotIdLen)
    {
        return false;
    }

    for (size_t i = 0; i < len; ++i)
    {
        const char c = id[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                        || c == '_' || c == '-' || c == '.';
        if (!ok || (c == '.' && id[i + 1] == '.'))
        {
            return false;
        }
    }

    // Reserved Windows device names, with or without an extension.
    char stem[8] = {};
    size_t stemLen = 0;
    while (id[stemLen] != '\0' && id[stemLen] != '.' && stemLen < sizeof(stem) - 1)
    {
        stem[stemLen] = static_cast<char>(toupper(static_cast<unsigned char>(id[stemLen])));
        ++stemLen;
    }
    if (id[stemLen] == '\0' || id[stemLen] == '.')
    {
        if (strcmp(stem, "CON") == 0 || strcmp(stem, "PRN") == 0 || strcmp(stem, "AUX") == 0 || strcmp(stem, "NUL") == 0)
        {
            return false;
        }
        if (stemLen == 4 && (strncmp(stem, "COM", 3) == 0 || strncmp(stem, "LPT", 3) == 0) && stem[3] >= '0' && stem[3] <= '9')
        {
            return false;
        }
    }
    return true;
}

//------------------------------------------------------------------------------------
void SlotManager::BuildPath(Dia::Core::StringCRC slotId, char* outPath, uint32_t outPathLen) const
{
    DIA_ASSERT(mConfig != nullptr, "SlotManager not initialised");
    DIA_ASSERT(outPath != nullptr, "outPath must not be null");
    DIA_ASSERT(outPathLen > 0, "outPathLen must be greater than zero");

    if (!IsValidSlotId(slotId.AsChar()))
    {
        DIA_LOG_WARNING("savegame", "BuildPath: invalid slot id '%s'", slotId.AsChar());
        outPath[0] = '\0';
        return;
    }

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

    // Split slotPattern around "{id}" so the search matches only slot files and the
    // slot id can be recovered from the file name.
    const char* pattern = mConfig->slotPattern;
    const char* placeholder = strstr(pattern, "{id}");
    const int prefixLen = placeholder ? static_cast<int>(placeholder - pattern) : 0;
    const char* suffix = placeholder ? placeholder + 4 : ".sav";
    const size_t suffixLen = strlen(suffix);

    char searchPath[512];
    snprintf(searchPath, sizeof(searchPath), "%s%.*s*%s", mConfig->baseDirectory, prefixLen, pattern, suffix);

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

        // Derive slot id: strip the slotPattern prefix and suffix so the id matches the one
        // passed to Save/BuildPath. FindFirstFile wildcards are loose (8.3 names, "*.sav*"),
        // so re-check the match and skip files that do not fit the pattern exactly.
        const char* fileName = findData.cFileName;
        const size_t nameLen = strlen(fileName);
        if (nameLen < static_cast<size_t>(prefixLen) + suffixLen
            || strncmp(fileName, pattern, prefixLen) != 0
            || strcmp(fileName + nameLen - suffixLen, suffix) != 0)
        {
            continue;
        }
        char idBuffer[256];
        const size_t idLen = nameLen - prefixLen - suffixLen;
        snprintf(idBuffer, sizeof(idBuffer), "%.*s", static_cast<int>(idLen), fileName + prefixLen);
        info.id = Dia::Core::StringCRC(idBuffer);

        // File last-write time (FILETIME is 100 ns ticks since 1601) -> Unix seconds.
        ULARGE_INTEGER ft;
        ft.LowPart  = findData.ftLastWriteTime.dwLowDateTime;
        ft.HighPart = findData.ftLastWriteTime.dwHighDateTime;
        const unsigned long long kUnixEpochTicks = 116444736000000000ull;
        info.timestamp = ft.QuadPart > kUnixEpochTicks ? (ft.QuadPart - kUnixEpochTicks) / 10000000ull : 0;

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
    if (path[0] == '\0')
    {
        return false;
    }
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

//------------------------------------------------------------------------------------
void SlotManager::DeleteSlot(Dia::Core::StringCRC slotId) const
{
    DIA_ASSERT(mConfig != nullptr, "SlotManager not initialised");

    char path[512];
    BuildPath(slotId, path, sizeof(path));
    if (path[0] == '\0')
    {
        return;
    }
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
    if (fromPath[0] == '\0' || toPath[0] == '\0')
    {
        return;
    }
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
