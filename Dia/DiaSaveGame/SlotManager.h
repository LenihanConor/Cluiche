#pragma once

#include <stdint.h>

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

#include "DiaSaveGame/SaveConfig.h"
#include "DiaSaveGame/SlotInfo.h"

namespace Dia::SaveGame {

class SlotManager {
public:
    void Init(const SaveConfig& config);

    // Path construction: replaces {id} in slotPattern with slotId.AsChar(), prepends baseDirectory
    void BuildPath(Dia::Core::StringCRC slotId, char* outPath, uint32_t outPathLen) const;

    void EnumerateSlots(Dia::Core::Containers::DynamicArrayC<SlotInfo, 32>& out) const;
    bool SlotExists(Dia::Core::StringCRC slotId) const;
    void DeleteSlot(Dia::Core::StringCRC slotId) const;
    void RenameSlot(Dia::Core::StringCRC fromId, Dia::Core::StringCRC toId) const;

    bool IsMaxSlotsReached() const;

private:
    const SaveConfig* mConfig = nullptr;
};

} // namespace Dia::SaveGame
