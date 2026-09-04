#pragma once

#include <cstdint>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Handle.h>
#include <DiaCore/Containers/HandlePool.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/HashTables/HashTable.h>

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeObserverSubject.h>

#include <DiaCondition/ConditionExpr.h>
#include <DiaCondition/ConditionRegistry.h>

#include <DiaEntity/Entity.h>

namespace Dia::Attribute {

    // -----------------------------------------------------------------------
    // ModifierOperation / AttributeModifier
    // -----------------------------------------------------------------------
    enum class ModifierOperation
    {
        Add,
        Multiply,
        Override
    };

    struct AttributeModifier
    {
        Dia::Core::StringCRC modifier_name;
        Dia::Core::StringCRC attribute_name;
        ModifierOperation    operation;
        float                value;
        char                 when_condition[128]; // empty = always-on. Core does NOT evaluate
                                                   // this field — carried only, evaluated by Feature 2.
    };

    // -----------------------------------------------------------------------
    // ModifierHandle
    // -----------------------------------------------------------------------
    class ModifierTag {};
    using ModifierHandle = Dia::Core::Handle<ModifierTag>;

    // -----------------------------------------------------------------------
    // AttributeSet
    //
    // Resolution pipeline (fixed):
    //   resolved  = base_value
    //   resolved += sum(Add modifiers)
    //   resolved *= product(Multiply modifiers)     // 1.0 if none
    //   if any Override modifier: resolved = override.value  (exclusive slot)
    //   resolved  = clamp(resolved, minimum_value, maximum_value)
    //
    // NOTE: AttributeSet owns a Dia::Core::HandlePool member, which is neither
    // copyable nor movable (see HandlePool.h). AttributeSet is therefore also
    // neither copyable nor movable. CreateFromSchema() relies on C++17 mandatory
    // copy elision (it returns a prvalue constructed directly in the return
    // statement) rather than NRVO. Callers that already own a default-constructed
    // AttributeSet (e.g. AttributeSetComponent) must use InitializeFromSchema()
    // to populate it in place instead of assigning a freshly-created instance.
    // -----------------------------------------------------------------------
    class AttributeSet
    {
    public:
        static const unsigned int kMaxModifiersPerAttribute    = 16;
        static const uint32_t     kMaxModifiersPerAttributeSet = 256;

        AttributeSet();
        ~AttributeSet();

        AttributeSet(const AttributeSet&)            = delete;
        AttributeSet& operator=(const AttributeSet&) = delete;
        AttributeSet(AttributeSet&&)                 = delete;
        AttributeSet& operator=(AttributeSet&&)      = delete;

        [[nodiscard]] static AttributeSet CreateFromSchema(const AttributeSchema& schema);

        // Populates this (assumed freshly default-constructed / not-yet-sized)
        // AttributeSet from a schema, in place. See class comment above.
        void InitializeFromSchema(const AttributeSchema& schema);

        float GetValue(Dia::Core::StringCRC attribute_name) const;
        float GetBaseValue(Dia::Core::StringCRC attribute_name) const;
        void  SetBaseValue(Dia::Core::StringCRC attribute_name, float value);

        [[nodiscard]] ModifierHandle AddModifier(const AttributeModifier& modifier);
        void RemoveModifier(ModifierHandle handle);

        // Registers the (non-owning) DiaCondition registry used to evaluate conditional
        // modifiers' when_condition expressions. Caller retains ownership and must keep
        // the registry alive for as long as any conditional modifier added after this
        // call remains on the AttributeSet. Not required for unconditional modifiers.
        void SetConditionRegistry(Dia::Condition::ConditionRegistry* registry);

        // Non-owning — used to stamp AttributeChangedEvent::entity / boundary event
        // entity params. Optional: if never called, those params carry a default
        // (invalid) Dia::Entity::Entity.
        void SetOwningEntity(Dia::Entity::Entity entity);

        // Callers Subscribe/Unsubscribe IAttributeObserver instances directly on the
        // returned reference.
        AttributeObserverSubject& GetObserverSubject();

        Dia::Core::StringCRC GetSchemaName() const;

        // -------------------------------------------------------------------
        // Index-based access
        //
        // Attribute slots are index-stable: InitializeFromSchema adds them in schema
        // index order and slots are never removed afterwards (only modifiers are), so
        // index i here always denotes schema.GetAttributeByIndex(i)'s attribute for the
        // lifetime of this AttributeSet.
        //
        // These exist so AttributeAccessorBridge can build a compile-time trampoline
        // table of non-capturing C function pointers (one per index) over an AttributeSet
        // whose attribute names are only known at runtime — see AttributeAccessorBridge.h.
        // -------------------------------------------------------------------

        unsigned int GetAttributeCount() const;

        // Fully resolved value (same pipeline as GetValue) of the attribute at `index`.
        // Returns 0.0f and asserts if index is out of range.
        float GetValueByIndex(unsigned int index) const;

        // Name of the attribute at `index`. Returns a default (zero) StringCRC and asserts
        // if index is out of range. O(index) — intended for one-off registration-time
        // iteration, not per-frame use.
        Dia::Core::StringCRC GetAttributeNameByIndex(unsigned int index) const;

    private:
        // Delegating constructor used by CreateFromSchema — constructs the
        // returned prvalue directly (guaranteed elision), never via copy/move.
        explicit AttributeSet(const AttributeSchema& schema);

        struct ModifierEntry
        {
            ModifierHandle                  handle;
            AttributeModifier               modifier;
            Dia::Condition::ConditionExpr*   parsed_condition; // heap-owned; nullptr if when_condition was empty
        };

        struct AttributeSlot
        {
            float base_value    = 0.0f;
            float minimum_value = 0.0f;
            float maximum_value = 0.0f;
            Dia::Core::Containers::DynamicArrayC<ModifierEntry, kMaxModifiersPerAttribute> modifiers;
        };

        float ResolveValue(const AttributeSlot& slot) const;

        // Diffs before/after the resolved value of a slot and fires OnAttributeChanged
        // plus the edge-triggered OnAttributeReachedMaximum/Minimum notifications as
        // appropriate. No-op if before == after. Called only on the success path of a
        // mutating method, after the mutation has already been applied to slot.
        void NotifyIfChanged(Dia::Core::StringCRC attribute_name, const AttributeSlot& slot, float before, float after);

        Dia::Core::StringCRC mSchemaName;
        Dia::Core::Containers::HashTable<Dia::Core::StringCRC, AttributeSlot> mSlots;

        // Non-owning — caller (SetConditionRegistry) retains lifetime.
        Dia::Condition::ConditionRegistry* mConditionRegistry = nullptr;

        // Non-owning — caller (SetOwningEntity) retains lifetime. Default-constructed
        // (invalid handle) when AttributeSet is used standalone without an owning entity.
        Dia::Entity::Entity mOwningEntity;

        AttributeObserverSubject mObserverSubject;

        // Mints ModifierHandle identity. Payload per-slot is the owning attribute_name,
        // so RemoveModifier(handle) can locate the AttributeSlot without a linear scan
        // of every attribute. Internally produces Handle<StringCRC> (matching the
        // payload template parameter); AddModifier/RemoveModifier translate between
        // that and the public ModifierHandle (Handle<ModifierTag>) by index+generation,
        // the same pattern Domain uses to translate Handle<EntitySlotData> <-> Entity.
        Dia::Core::HandlePool<Dia::Core::StringCRC, kMaxModifiersPerAttributeSet> mModifierHandlePool;
    };

} // namespace Dia::Attribute
