#include <DiaAttribute/AttributeSet.h>

#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Core/Assert.h>

namespace Dia::Attribute {

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    AttributeSet::AttributeSet()
        : mSchemaName()
        , mSlots()
        , mModifierHandlePool()
    {}

    AttributeSet::AttributeSet(const AttributeSchema& schema)
        : AttributeSet()
    {
        InitializeFromSchema(schema);
    }

    AttributeSet::~AttributeSet()
    {}

    // -----------------------------------------------------------------------
    // CreateFromSchema — returns a prvalue constructed directly in the return
    // statement. This is a mandatory-copy-elision case (C++17): no copy/move
    // constructor is invoked (AttributeSet has neither — see header comment).
    // -----------------------------------------------------------------------
    AttributeSet AttributeSet::CreateFromSchema(const AttributeSchema& schema)
    {
        return AttributeSet(schema);
    }

    // -----------------------------------------------------------------------
    // InitializeFromSchema — populates a freshly default-constructed (unsized)
    // AttributeSet in place.
    // -----------------------------------------------------------------------
    void AttributeSet::InitializeFromSchema(const AttributeSchema& schema)
    {
        mSchemaName = schema.GetSchemaName();

        const unsigned int count = schema.GetAttributeCount();

        // HashTable::GetHashIndex divides by table size — SetSize(0, 0) would
        // divide by zero, so only size the table when there's at least one attribute.
        if (count > 0)
            mSlots.SetSize(count, count);

        for (unsigned int i = 0; i < count; ++i)
        {
            const AttributeDefinition& def = schema.GetAttributeByIndex(i);

            AttributeSlot slot;
            slot.base_value    = def.default_value;
            slot.minimum_value = def.minimum_value;
            slot.maximum_value = def.maximum_value;

            mSlots.Add(def.attribute_name, slot);
        }
    }

    // -----------------------------------------------------------------------
    // Resolution pipeline
    // -----------------------------------------------------------------------
    float AttributeSet::ResolveValue(const AttributeSlot& slot)
    {
        float addSum          = 0.0f;
        float multiplyProduct = 1.0f;
        bool  hasOverride     = false;
        float overrideValue   = 0.0f;

        for (unsigned int i = 0; i < slot.modifiers.Size(); ++i)
        {
            const AttributeModifier& mod = slot.modifiers[i].modifier;
            switch (mod.operation)
            {
            case ModifierOperation::Add:
                addSum += mod.value;
                break;
            case ModifierOperation::Multiply:
                multiplyProduct *= mod.value;
                break;
            case ModifierOperation::Override:
                hasOverride   = true;
                overrideValue = mod.value;
                break;
            }
        }

        float resolved = slot.base_value;
        resolved += addSum;
        resolved *= multiplyProduct;

        if (hasOverride)
            resolved = overrideValue;

        if (resolved < slot.minimum_value) resolved = slot.minimum_value;
        if (resolved > slot.maximum_value) resolved = slot.maximum_value;

        return resolved;
    }

    float AttributeSet::GetValue(Dia::Core::StringCRC attribute_name) const
    {
        const AttributeSlot* slot = mSlots.TryGetItemConst(attribute_name);
        DIA_ASSERT(slot != nullptr, "AttributeSet::GetValue: unknown attribute '%s'", attribute_name.AsChar());
        if (slot == nullptr)
            return 0.0f;

        return ResolveValue(*slot);
    }

    float AttributeSet::GetBaseValue(Dia::Core::StringCRC attribute_name) const
    {
        const AttributeSlot* slot = mSlots.TryGetItemConst(attribute_name);
        DIA_ASSERT(slot != nullptr, "AttributeSet::GetBaseValue: unknown attribute '%s'", attribute_name.AsChar());
        if (slot == nullptr)
            return 0.0f;

        return slot->base_value;
    }

    void AttributeSet::SetBaseValue(Dia::Core::StringCRC attribute_name, float value)
    {
        AttributeSlot* slot = mSlots.TryGetItem(attribute_name);
        DIA_ASSERT(slot != nullptr, "AttributeSet::SetBaseValue: unknown attribute '%s'", attribute_name.AsChar());
        if (slot == nullptr)
            return;

        slot->base_value = value;
    }

    // -----------------------------------------------------------------------
    // Modifier stack
    // -----------------------------------------------------------------------
    ModifierHandle AttributeSet::AddModifier(const AttributeModifier& modifier)
    {
        AttributeSlot* slot = mSlots.TryGetItem(modifier.attribute_name);
        if (slot == nullptr)
        {
            DIA_ASSERT(false, "AttributeSet::AddModifier: unknown attribute '%s'", modifier.attribute_name.AsChar());
            DIA_LOG_WARNING("Attribute", "AttributeSet::AddModifier: unknown attribute '%s' — modifier dropped", modifier.attribute_name.AsChar());
            return ModifierHandle::Invalid();
        }

        if (modifier.operation == ModifierOperation::Override)
        {
            for (unsigned int i = 0; i < slot->modifiers.Size(); ++i)
            {
                DIA_ASSERT(slot->modifiers[i].modifier.operation != ModifierOperation::Override,
                    "AttributeSet::AddModifier: attribute '%s' already has an active Override modifier",
                    modifier.attribute_name.AsChar());
            }
        }

        if (slot->modifiers.IsFull())
        {
            DIA_ASSERT(false, "AttributeSet::AddModifier: attribute '%s' modifier stack is full", modifier.attribute_name.AsChar());
            DIA_LOG_WARNING("Attribute", "AttributeSet::AddModifier: attribute '%s' modifier stack full — modifier dropped", modifier.attribute_name.AsChar());
            return ModifierHandle::Invalid();
        }

        // Mint the internal handle (payload = owning attribute_name), then translate
        // its index+generation into the public ModifierHandle tag type.
        Dia::Core::Handle<Dia::Core::StringCRC> internal = mModifierHandlePool.Allocate(modifier.attribute_name);
        ModifierHandle handle(internal.GetIndex(), internal.GetGeneration());

        ModifierEntry entry;
        entry.handle   = handle;
        entry.modifier = modifier;
        slot->modifiers.Add(entry);

        DIA_LOG_INFO("Attribute", "AttributeSet::AddModifier: '%s' added to attribute '%s'",
            modifier.modifier_name.AsChar(), modifier.attribute_name.AsChar());

        return handle;
    }

    void AttributeSet::RemoveModifier(ModifierHandle handle)
    {
        Dia::Core::Handle<Dia::Core::StringCRC> internal(handle.GetIndex(), handle.GetGeneration());

        if (!mModifierHandlePool.IsValid(internal))
        {
            DIA_ASSERT(false, "AttributeSet::RemoveModifier: handle is invalid or already removed");
            return;
        }

        const Dia::Core::StringCRC* ownerAttr = mModifierHandlePool.Get(internal);
        DIA_ASSERT(ownerAttr != nullptr, "AttributeSet::RemoveModifier: pool returned null for a valid handle");
        if (ownerAttr == nullptr)
            return;

        AttributeSlot* slot = mSlots.TryGetItem(*ownerAttr);
        DIA_ASSERT(slot != nullptr, "AttributeSet::RemoveModifier: owning attribute slot not found");
        if (slot == nullptr)
            return;

        for (unsigned int i = 0; i < slot->modifiers.Size(); ++i)
        {
            if (slot->modifiers[i].handle == handle)
            {
                slot->modifiers.RemoveAt(i);
                break;
            }
        }

        mModifierHandlePool.Free(internal);

        DIA_LOG_INFO("Attribute", "AttributeSet::RemoveModifier: modifier removed");
    }

    // -----------------------------------------------------------------------
    // Metadata
    // -----------------------------------------------------------------------
    Dia::Core::StringCRC AttributeSet::GetSchemaName() const
    {
        return mSchemaName;
    }

} // namespace Dia::Attribute
