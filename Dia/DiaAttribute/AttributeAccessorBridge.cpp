#include <DiaAttribute/AttributeAccessorBridge.h>

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::Attribute {

    void AttributeAccessorBridge::RegisterAccessors(Dia::Condition::ConditionRegistry& registry,
                                                    const AttributeSet& set,
                                                    Dia::Core::StringCRC slot_name)
    {
        const unsigned int count = set.GetAttributeCount();

        if (count > kMaxBridgedAttributesPerSet)
        {
            DIA_ASSERT(false,
                "AttributeAccessorBridge::RegisterAccessors: schema '%s' has %u attributes, exceeding kMaxBridgedAttributesPerSet (%u) — registering only the first %u",
                set.GetSchemaName().AsChar(), count, kMaxBridgedAttributesPerSet, kMaxBridgedAttributesPerSet);
            DIA_LOG_WARNING("Attribute",
                "AttributeAccessorBridge::RegisterAccessors: schema '%s' has %u attributes, exceeding kMaxBridgedAttributesPerSet (%u) — partial registration",
                set.GetSchemaName().AsChar(), count, kMaxBridgedAttributesPerSet);
        }

        const unsigned int registerCount = (count < kMaxBridgedAttributesPerSet) ? count : kMaxBridgedAttributesPerSet;

        for (unsigned int i = 0; i < registerCount; ++i)
        {
            Dia::Core::StringCRC attrName = set.GetAttributeNameByIndex(i);

            // Hard error, not a silent overwrite: ConditionRegistry has no unregister and no
            // way to report that RegisterFloat replaced an existing entry, so a collision here
            // would otherwise quietly repoint someone else's accessor. Assert-then-continue
            // matches AttributeSet::AddModifier's Override-collision handling; in Release the
            // subsequent RegisterFloat overwrites, which is ConditionRegistry's own behaviour.
            DIA_ASSERT(!registry.HasFloat(slot_name, attrName),
                "AttributeAccessorBridge::RegisterAccessors: '%s.%s' is already registered — re-registration is a hard error",
                slot_name.AsChar(), attrName.AsChar());

            registry.RegisterFloat(slot_name, attrName, kAccessorTable[i]);

            DIA_LOG_INFO("Attribute", "AttributeAccessorBridge::RegisterAccessors: bridged '%s.%s'",
                slot_name.AsChar(), attrName.AsChar());
        }
    }

} // namespace Dia::Attribute
