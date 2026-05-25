#include <DiaEntity/ComponentRegistry.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::Entity {

    ComponentRegistry& ComponentRegistry::Get() {
        static ComponentRegistry instance;
        return instance;
    }

    bool ComponentRegistry::Register(const ComponentTypeDesc& desc) {
        if (Find(desc.typeId) != nullptr) {
            DIA_LOG_WARNING("DiaEntity", "ComponentRegistry: duplicate registration for '%s' — ignored", desc.debugName);
            return false;
        }
        if (mDescs.IsFull()) {
            DIA_ASSERT(false, "ComponentRegistry: registry full (capacity %u)", kMaxComponentTypesPerDomain);
            return false;
        }
        mDescs.Add(&desc);
        return true;
    }

    const ComponentTypeDesc* ComponentRegistry::Find(Dia::Core::StringCRC typeId) const {
        for (uint32_t i = 0; i < mDescs.Size(); ++i) {
            if (mDescs[i]->typeId == typeId) {
                return mDescs[i];
            }
        }
        return nullptr;
    }

    uint32_t ComponentRegistry::GetCount() const {
        return mDescs.Size();
    }

    const ComponentTypeDesc& ComponentRegistry::GetByIndex(uint32_t i) const {
        DIA_ASSERT(i < mDescs.Size(), "ComponentRegistry::GetByIndex out of range");
        return *mDescs[i];
    }

} // namespace Dia::Entity
