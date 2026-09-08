#include <DiaAttribute/AttributeSet.h>

#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Core/Assert.h>

#include <DiaCore/Json/external/json/json.h>

#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>

#include <cstring>
#include <utility>

namespace Dia::Attribute {

    namespace {

        // --- JSON keys (StringCRC's AsChar() is used as the jsoncpp member key) ---
        const Dia::Core::StringCRC kKeyBaseValues    ("baseValues");
        const Dia::Core::StringCRC kKeyModifiers     ("modifiers");
        const Dia::Core::StringCRC kKeyModifierName  ("modifierName");
        const Dia::Core::StringCRC kKeyAttributeName ("attributeName");
        const Dia::Core::StringCRC kKeyOperation     ("operation");
        const Dia::Core::StringCRC kKeyValue         ("value");
        const Dia::Core::StringCRC kKeyWhenCondition ("whenCondition");

    } // anonymous namespace

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
        // Reentrancy guard: a conditional modifier's when_condition can reference an
        // accessor (via AttributeAccessorBridge) that reads back through GetValueByIndex
        // into THIS SAME SLOT's resolution — e.g. attribute "health" gated on a condition
        // that itself reads "health". Without this guard that is unbounded recursion
        // (stack overflow), not a graceful failure. Detect it and break the cycle by
        // returning the un-resolved base value instead.
        //
        // Deliberately scoped per-SLOT (slot.resolving), not per-AttributeSet: a
        // conditional modifier on attribute A legitimately gating on a DIFFERENT
        // attribute B of the same set (AttributeAccessorBridge) causes a nested
        // ResolveValue call on B's slot while A's is still resolving — that is normal,
        // non-cyclic cross-attribute evaluation and must not trip this guard.
        if (slot.resolving)
        {
            DIA_ASSERT(false,
                "AttributeSet::ResolveValue: reentrant call detected — a conditional modifier's "
                "when_condition references an accessor that reads back into this same attribute's "
                "resolution, causing infinite recursion. Returning the un-resolved base value to break the cycle.");
            DIA_LOG_WARNING("Attribute",
                "AttributeSet::ResolveValue: reentrant resolution detected — returning base_value to break a self-referencing conditional-modifier cycle");
            return slot.base_value;
        }

        // RAII guard: guarantees slot.resolving is reset to false on every exit from this
        // function (including any early return added in future edits), rather than relying
        // on a manual reset placed before each return statement.
        struct ScopedResolveGuard
        {
            bool& flag;
            ~ScopedResolveGuard() { flag = false; }
        };
        slot.resolving = true;
        ScopedResolveGuard scopedGuard{ slot.resolving };

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

    // -----------------------------------------------------------------------
    // Index-based access (see header for the index-stability contract)
    // -----------------------------------------------------------------------
    unsigned int AttributeSet::GetAttributeCount() const
    {
        return mSlots.Size();
    }

    float AttributeSet::GetValueByIndex(unsigned int index) const
    {
        DIA_ASSERT(index < mSlots.Size(),
            "AttributeSet::GetValueByIndex: index %u out of range (attribute count %u)", index, mSlots.Size());
        if (index >= mSlots.Size())
            return 0.0f;

        // HashTable's payload array is append-order, so GetItemByIndexConst(index) is the
        // slot added by InitializeFromSchema for schema attribute `index`.
        return ResolveValue(mSlots.GetItemByIndexConst(index));
    }

    Dia::Core::StringCRC AttributeSet::GetAttributeNameByIndex(unsigned int index) const
    {
        DIA_ASSERT(index < mSlots.Size(),
            "AttributeSet::GetAttributeNameByIndex: index %u out of range (attribute count %u)", index, mSlots.Size());
        if (index >= mSlots.Size())
            return Dia::Core::StringCRC();

        // HashTable exposes GetItemByIndexConst for payloads but no GetKeyByIndex, so walk
        // the const iterator (which does expose GetKey) `index` steps. O(index) — acceptable
        // because callers iterate once at registration time over a small schema.
        auto it = mSlots.Begin();
        for (unsigned int i = 0; i < index; ++i)
            ++it;

        return it.GetKey();
    }

    float AttributeSet::GetMinimumValueByIndex(unsigned int index) const
    {
        DIA_ASSERT(index < mSlots.Size(),
            "AttributeSet::GetMinimumValueByIndex: index %u out of range (attribute count %u)", index, mSlots.Size());
        if (index >= mSlots.Size())
            return 0.0f;

        return mSlots.GetItemByIndexConst(index).minimum_value;
    }

    float AttributeSet::GetMaximumValueByIndex(unsigned int index) const
    {
        DIA_ASSERT(index < mSlots.Size(),
            "AttributeSet::GetMaximumValueByIndex: index %u out of range (attribute count %u)", index, mSlots.Size());
        if (index >= mSlots.Size())
            return 0.0f;

        return mSlots.GetItemByIndexConst(index).maximum_value;
    }

    bool AttributeSet::HasAttribute(Dia::Core::StringCRC attribute_name) const
    {
        return mSlots.ContainsKey(attribute_name);
    }

    // -----------------------------------------------------------------------
    // Modifier-stack inspection (see header for the query-surface contract)
    // -----------------------------------------------------------------------
    unsigned int AttributeSet::GetModifierCountForAttribute(Dia::Core::StringCRC attribute_name) const
    {
        const AttributeSlot* slot = mSlots.TryGetItemConst(attribute_name);
        DIA_ASSERT(slot != nullptr,
            "AttributeSet::GetModifierCountForAttribute: unknown attribute '%s'", attribute_name.AsChar());
        if (slot == nullptr)
            return 0;

        return slot->modifiers.Size();
    }

    ModifierSnapshot AttributeSet::GetModifierSnapshotForAttribute(Dia::Core::StringCRC attribute_name, unsigned int index) const
    {
        ModifierSnapshot snap{};

        const AttributeSlot* slot = mSlots.TryGetItemConst(attribute_name);
        DIA_ASSERT(slot != nullptr,
            "AttributeSet::GetModifierSnapshotForAttribute: unknown attribute '%s'", attribute_name.AsChar());
        if (slot == nullptr)
            return snap;

        DIA_ASSERT(index < slot->modifiers.Size(),
            "AttributeSet::GetModifierSnapshotForAttribute: index %u out of range (modifier count %u) for attribute '%s'",
            index, slot->modifiers.Size(), attribute_name.AsChar());
        if (index >= slot->modifiers.Size())
            return snap;

        const ModifierEntry& entry = slot->modifiers[index];

        snap.modifier_name = entry.modifier.modifier_name;
        snap.operation     = entry.modifier.operation;
        snap.value         = entry.modifier.value;
        snap.isConditional = (entry.parsed_condition != nullptr);

        // Mirrors ResolveValue's null-registry-defensive check — an unconditional modifier
        // is always "true" (it always contributes); a conditional one is evaluated live.
        snap.conditionCurrentlyTrue = snap.isConditional
            ? (mConditionRegistry != nullptr && entry.parsed_condition->Evaluate(*mConditionRegistry))
            : true;

        return snap;
    }

    // -----------------------------------------------------------------------
    // Dia::SaveGame::ISaveable
    // -----------------------------------------------------------------------
    void AttributeSet::Serialize(Dia::SaveGame::SaveContext& ctx) const
    {
        // 1. Base values — a plain named object, keyed by attribute name.
        ctx.BeginObject(kKeyBaseValues);
        for (unsigned int i = 0; i < GetAttributeCount(); ++i)
        {
            Dia::Core::StringCRC name = GetAttributeNameByIndex(i);
            ctx.Write(name, GetBaseValue(name));
        }
        ctx.EndObject();

        // 2. Full active modifier list, across every attribute slot.
        ctx.BeginArray(kKeyModifiers);
        for (auto it = mSlots.Begin(); it != mSlots.End(); ++it)
        {
            const AttributeSlot& slot = it.Value();
            for (unsigned int i = 0; i < slot.modifiers.Size(); ++i)
            {
                const AttributeModifier& modifier = slot.modifiers[i].modifier;

                Json::Value elem(Json::objectValue);
                elem[kKeyModifierName.AsChar()]  = modifier.modifier_name.AsChar();
                elem[kKeyAttributeName.AsChar()] = modifier.attribute_name.AsChar();
                elem[kKeyOperation.AsChar()]     = static_cast<Json::Int>(modifier.operation);
                elem[kKeyValue.AsChar()]         = modifier.value;
                elem[kKeyWhenCondition.AsChar()] = modifier.when_condition; // empty string if unconditional
                ctx.CurrentNode().append(elem);
            }
        }
        ctx.EndArray();
    }

    void AttributeSet::Deserialize(Dia::SaveGame::LoadContext& ctx)
    {
        // 1. Base values — only ever reads keys for attributes this (possibly-newer)
        //    schema still has; any saved base value for an attribute this schema has
        //    since dropped is simply never read (LoadContext::Read returns false).
        if (ctx.BeginObject(kKeyBaseValues))
        {
            for (unsigned int i = 0; i < GetAttributeCount(); ++i)
            {
                Dia::Core::StringCRC name = GetAttributeNameByIndex(i);
                float value = 0.0f;
                if (ctx.Read(name, value))
                    SetBaseValue(name, value);
            }
            ctx.EndObject();
        }

        // 2. Modifiers — reconstructed via the public AddModifier, so Core's own
        //    validation (full-stack rejection, conditional-modifier parsing/validation)
        //    applies exactly as it would to any other caller. Handles are freshly
        //    issued by THIS instance's HandlePool (AC-4) — never assumed stable.
        uint32_t modifierCount = 0;
        if (ctx.BeginArray(kKeyModifiers, modifierCount))
        {
            for (uint32_t i = 0; i < modifierCount; ++i)
            {
                ctx.SetArrayIndex(i);

                char    modifierNameBuf[64]  = {};
                char    attributeNameBuf[64] = {};
                int32_t operationVal         = 0;
                float   value                = 0.0f;
                char    whenConditionBuf[128] = {}; // matches AttributeModifier::when_condition's size

                ctx.Read(kKeyModifierName,  modifierNameBuf,  sizeof(modifierNameBuf));
                ctx.Read(kKeyAttributeName, attributeNameBuf, sizeof(attributeNameBuf));
                ctx.Read(kKeyOperation,     operationVal);
                ctx.Read(kKeyValue,         value);
                ctx.Read(kKeyWhenCondition, whenConditionBuf, sizeof(whenConditionBuf));

                // AC-2: the attribute this saved modifier targets may no longer exist in
                // the current schema. Check BEFORE calling AddModifier — AddModifier's own
                // DIA_ASSERT for an unknown attribute exists to catch a *programmer* bug,
                // not this legitimate save/schema-drift condition, so it must never be
                // reached from here.
                Dia::Core::StringCRC attrName(attributeNameBuf);
                if (!HasAttribute(attrName))
                {
                    DIA_LOG_WARNING("Attribute",
                        "AttributeSet::Deserialize: saved modifier '%s' references attribute '%s' which no longer exists in the current schema — dropped",
                        modifierNameBuf, attributeNameBuf);
                    continue;
                }

                // Same reasoning as the HasAttribute check above, for AddModifier's OTHER
                // programmer-bug assert: a save file can legitimately carry more than
                // kMaxModifiersPerAttribute modifiers for one attribute (corrupted, hand-edited,
                // or saved under a since-lowered cap). That is schema/data drift, not a
                // programmer error, so it must never reach AddModifier's internal
                // DIA_ASSERT(false, "...modifier stack is full") — pre-check and drop instead.
                if (GetModifierCountForAttribute(attrName) >= kMaxModifiersPerAttribute)
                {
                    DIA_LOG_WARNING("Attribute",
                        "AttributeSet::Deserialize: attribute '%s' modifier stack is already full (%u) — saved modifier '%s' dropped",
                        attributeNameBuf, kMaxModifiersPerAttribute, modifierNameBuf);
                    continue;
                }

                AttributeModifier modifier{};
                modifier.modifier_name  = Dia::Core::StringCRC(modifierNameBuf);
                modifier.attribute_name = attrName;
                modifier.operation      = static_cast<ModifierOperation>(operationVal);
                modifier.value          = value;
                strncpy_s(modifier.when_condition, sizeof(modifier.when_condition), whenConditionBuf, _TRUNCATE);

                AddModifier(modifier); // handle discarded — fresh handles are the contract (AC-4)
            }
            ctx.EndArray();
        }
    }

} // namespace Dia::Attribute
