#pragma once

#include <stdint.h>

namespace Dia::SaveGame {

class SaveContext;
class LoadContext;

class ISaveable {
public:
    virtual ~ISaveable() = default;

    virtual void        Serialize   (SaveContext& ctx) const = 0;
    virtual void        Deserialize (LoadContext& ctx)       = 0;
    virtual uint32_t    GetVersion  () const                 = 0;

    // Called on every participant (post-migration) before any Deserialize runs, so a load is
    // all-or-nothing. Check the data in ctx without touching live state; return false to abort
    // the load with LoadResultCode::DeserializeError. Default accepts everything.
    virtual bool        Validate    (LoadContext& ctx) const { (void)ctx; return true; }
};

} // namespace Dia::SaveGame
