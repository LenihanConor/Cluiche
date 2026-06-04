#pragma once
#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>
#include <diaentitytemplate/IComponent.h>

namespace Dia::Entity {

    // Type-erased base for per-component-type storage pools.
    // Concrete typed pools are ComponentPool<TComponent>.
    class IComponentPool {
    public:
        virtual ~IComponentPool() = default;

        virtual IComponent*       GetRaw(uint32_t entityIndex)       = 0;
        virtual const IComponent* GetRaw(uint32_t entityIndex) const = 0;

        // Allocate a new slot for an entity, default-constructing the component.
        // Returns nullptr if pool is full or entity already has this component.
        virtual IComponent* AllocateRaw(uint32_t entityIndex) = 0;

        // Calls OnDetach then frees the slot.
        virtual void     Destroy(uint32_t entityIndex)                 = 0;
        virtual bool     HasSlot(uint32_t entityIndex) const           = 0;
        virtual uint32_t GetSlotForEntity(uint32_t entityIndex) const  = 0; // kInvalidPoolSlot if none
        virtual void     SetSlotForEntity(uint32_t entityIndex, uint32_t slotIndex) = 0;
        virtual void     ClearSlotForEntity(uint32_t entityIndex)      = 0;

        virtual Dia::Core::StringCRC GetTypeId() const = 0;

        // Returns the number of component slots currently allocated.
        virtual uint32_t Size() const = 0;
    };

    inline constexpr uint32_t kInvalidPoolSlot = 0xFFFFFFFFu;

} // namespace Dia::Entity
