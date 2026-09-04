#include <DiaAttribute/AttributeSet.h>

#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Core/Assert.h>

#include <DiaCore/Json/external/json/json.h>

#include <utility>

namespace Dia::Attribute {

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    AttributeSet::AttributeSet()
        : mSchemaName()
        , mSlots()
        , mOwningEntity()
        , mObserverSubject()
        , mModifierHandlePool()
    {}

    AttributeSet::AttributeSet(const AttributeSchema& schema)
        : AttributeSet()
    {
        InitializeFromSchema(schema);
    }

    AttributeSet::~AttributeSet()
    {
        // Release any heap-owned ConditionExpr instances still attached to outstanding
        // conditional modifiers. AttributeSet is non-copyable/non-movable (see header),
        // so there is no risk of a slot/entry being copied and later double-freed here.
        for (auto it = mSlots.Begin(); it != mSlots.End(); ++it)
        {
            AttributeSlot& slot = it.Value();
            for (unsigned int i = 0; i < slot.modifiers.Size(); ++i)
            {
                delete slot.modifiers[i].parsed_condition;
                slot.modifiers[i].parsed_condition = nullptr;
            }
        }
    }

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
    float AttributeSet::ResolveValue(const AttributeSlot& slot) const
    {
        float addSum          = 0.0f;
        float multiplyProduct = 1.0f;
        bool  hasOverride     = false;
        float overrideValue   = 0.0f;

        for (unsigned int i = 0; i < slot.modifiers.Size(); ++i)
        {
            const ModifierEntry& entry = slot.modifiers[i];

            // Conditional modifiers whose gating condition currently evaluates false
            // contribute nothing to this resolve — they remain registered, just inactive.
            if (entry.parsed_condition != nullptr)
            {
                DIA_ASSERT(mConditionRegistry != nullptr,
                    "AttributeSet::ResolveValue: conditional modifier present but no ConditionRegistry set");
                if (mConditionRegistry == nullptr || !entry.parsed_condition->Evaluate(*mConditionRegistry))
                    continue;
            }

            const AttributeModifier& mod = entry.modifier;
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

    void AttributeSet::NotifyIfChanged(Dia::Core::StringCRC attribute_name, const AttributeSlot& slot, float before, float after)
    {
        if (before == after)
            return;

        AttributeChangedEvent e{ mOwningEntity, attribute_name, before, after };
        mObserverSubject.NotifyAttributeChanged(e);

        const bool wasAtMax = (before == slot.maximum_value);
        const bool isAtMax  = (after  == slot.maximum_value);
        if (!wasAtMax && isAtMax)
            mObserverSubject.NotifyAttributeReachedMaximum(mOwningEntity, attribute_name);

        const bool wasAtMin = (before == slot.minimum_value);
        const bool isAtMin  = (after  == slot.minimum_value);
        if (!wasAtMin && isAtMin)
            mObserverSubject.NotifyAttributeReachedMinimum(mOwningEntity, attribute_name);
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

        const float before = ResolveValue(*slot);
        slot->base_value = value;
        const float after = ResolveValue(*slot);

        NotifyIfChanged(attribute_name, *slot, before, after);
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

        // Conditional modifiers: parse + validate the when_condition JSON eagerly, before
        // the modifier ever enters the array, so ResolveValue() never has to deal with an
        // unresolvable or malformed condition (AC-4 / AC-5 of the Conditional Modifiers feature).
        Dia::Condition::ConditionExpr* parsedCondition = nullptr;
        if (modifier.when_condition[0] != '\0')
        {
            // Note: these are data/content validation failures (bad or stale game data),
            // not programmer errors — logged via DIA_LOG_WARNING only, no DIA_ASSERT.
            if (mConditionRegistry == nullptr)
            {
                DIA_LOG_WARNING("Attribute", "AttributeSet::AddModifier: attribute '%s' modifier '%s' has a when_condition but SetConditionRegistry was never called — modifier dropped",
                    modifier.attribute_name.AsChar(), modifier.modifier_name.AsChar());
                return ModifierHandle::Invalid();
            }

            Json::Value conditionRoot;
            Json::Reader conditionReader;
            const bool parsedJson = conditionReader.parse(modifier.when_condition, conditionRoot);
            if (!parsedJson)
            {
                DIA_LOG_WARNING("Attribute", "AttributeSet::AddModifier: attribute '%s' modifier '%s' when_condition is not valid JSON — modifier dropped",
                    modifier.attribute_name.AsChar(), modifier.modifier_name.AsChar());
                return ModifierHandle::Invalid();
            }

            Dia::Core::Containers::DynamicArrayC<const char*, 32> loadErrors;
            Dia::Condition::ConditionExpr expr = Dia::Condition::ConditionExpr::LoadFromJson(conditionRoot, loadErrors);
            if (!expr.IsValid())
            {
                DIA_LOG_WARNING("Attribute", "AttributeSet::AddModifier: attribute '%s' modifier '%s' when_condition failed to parse — modifier dropped",
                    modifier.attribute_name.AsChar(), modifier.modifier_name.AsChar());
                return ModifierHandle::Invalid();
            }

            Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
            if (!expr.Validate(*mConditionRegistry, validateErrors))
            {
                DIA_LOG_WARNING("Attribute", "AttributeSet::AddModifier: attribute '%s' modifier '%s' when_condition references an accessor unresolvable in the ConditionRegistry — modifier dropped",
                    modifier.attribute_name.AsChar(), modifier.modifier_name.AsChar());
                return ModifierHandle::Invalid();
            }

            parsedCondition = new Dia::Condition::ConditionExpr(std::move(expr));
        }

        // Mint the internal handle (payload = owning attribute_name), then translate
        // its index+generation into the public ModifierHandle tag type.
        Dia::Core::Handle<Dia::Core::StringCRC> internal = mModifierHandlePool.Allocate(modifier.attribute_name);
        ModifierHandle handle(internal.GetIndex(), internal.GetGeneration());

        const float before = ResolveValue(*slot);

        ModifierEntry entry;
        entry.handle           = handle;
        entry.modifier         = modifier;
        entry.parsed_condition = parsedCondition;
        slot->modifiers.Add(entry);

        const float after = ResolveValue(*slot);
        NotifyIfChanged(modifier.attribute_name, *slot, before, after);

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

        // Value copy — ownerAttr is a pointer into pool storage that Free() below may invalidate.
        Dia::Core::StringCRC attrName = *ownerAttr;

        for (unsigned int i = 0; i < slot->modifiers.Size(); ++i)
        {
            if (slot->modifiers[i].handle == handle)
            {
                const float before = ResolveValue(*slot);

                delete slot->modifiers[i].parsed_condition;
                slot->modifiers[i].parsed_condition = nullptr;
                slot->modifiers.RemoveAt(i);

                const float after = ResolveValue(*slot);
                NotifyIfChanged(attrName, *slot, before, after);
                break;
            }
        }

        mModifierHandlePool.Free(internal);

        DIA_LOG_INFO("Attribute", "AttributeSet::RemoveModifier: modifier removed");
    }

    void AttributeSet::SetConditionRegistry(Dia::Condition::ConditionRegistry* registry)
    {
        mConditionRegistry = registry;
    }

    void AttributeSet::SetOwningEntity(Dia::Entity::Entity entity)
    {
        mOwningEntity = entity;
    }

    AttributeObserverSubject& AttributeSet::GetObserverSubject()
    {
        return mObserverSubject;
    }

    // -----------------------------------------------------------------------
    // Metadata
    // -----------------------------------------------------------------------
    Dia::Core::StringCRC AttributeSet::GetSchemaName() const
    {
        return mSchemaName;
    }

} // namespace Dia::Attribute
