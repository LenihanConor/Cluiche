#pragma once

#include <stdint.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include "DiaSaveGame/SaveConfig.h"
#include "DiaSaveGame/SaveResult.h"
#include "DiaSaveGame/SlotInfo.h"
#include "DiaSaveGame/SlotManager.h"

namespace Dia::SaveGame {

class SaveRegistry;

class SaveManager {
public:
    static const unsigned int kMaxPathLen  = 512;
    static const unsigned int kBufferSize  = 65536;

    SaveManager() = default;

    void Init    (const SaveConfig& config, SaveRegistry& registry);
    void Shutdown();

    SaveResult Save(Dia::Core::StringCRC slotId);
    LoadResult Load(Dia::Core::StringCRC slotId);

    void DeleteSlot  (Dia::Core::StringCRC slotId);
    void RenameSlot  (Dia::Core::StringCRC fromId, Dia::Core::StringCRC toId);
    void EnumerateSlots(Dia::Core::Containers::DynamicArrayC<SlotInfo, 32>& out) const;

private:
    const SaveConfig*  mConfig   = nullptr;
    SaveRegistry*      mRegistry = nullptr;
    SlotManager        mSlotManager;
};

} // namespace Dia::SaveGame
