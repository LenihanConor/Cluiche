#pragma once

#include <stdint.h>
#include <memory>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Architecture/Observer.h>

#include "DiaSaveGame/SaveConfig.h"
#include "DiaSaveGame/SaveResult.h"
#include "DiaSaveGame/SlotInfo.h"
#include "DiaSaveGame/SlotManager.h"

namespace Dia::SaveGame {

class SaveRegistry;

// Integer message codes fired via SaveManager's ObserverSubjects.
// Observers cast the int back to SaveEvent to identify the notification.
enum class SaveEvent : int {
    SaveStarted      = 0,
    SaveCompleted    = 1,
    LoadStarted      = 2,
    LoadCompleted    = 3,
    MigrationApplied = 4,
};

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

    // Observer subjects — callers AttachToObserver before calling Save/Load
    Dia::Core::ObserverSubject& OnSaveStarted()      { return mOnSaveStarted;      }
    Dia::Core::ObserverSubject& OnSaveCompleted()    { return mOnSaveCompleted;    }
    Dia::Core::ObserverSubject& OnLoadStarted()      { return mOnLoadStarted;      }
    Dia::Core::ObserverSubject& OnLoadCompleted()    { return mOnLoadCompleted;    }
    Dia::Core::ObserverSubject& OnMigrationApplied() { return mOnMigrationApplied; }

private:
    const SaveConfig*  mConfig   = nullptr;
    SaveRegistry*      mRegistry = nullptr;
    SlotManager        mSlotManager;

    Dia::Core::ObserverSubject mOnSaveStarted;
    Dia::Core::ObserverSubject mOnSaveCompleted;
    Dia::Core::ObserverSubject mOnLoadStarted;
    Dia::Core::ObserverSubject mOnLoadCompleted;
    Dia::Core::ObserverSubject mOnMigrationApplied;
};

} // namespace Dia::SaveGame
