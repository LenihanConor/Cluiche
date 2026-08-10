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
};

} // namespace Dia::SaveGame
