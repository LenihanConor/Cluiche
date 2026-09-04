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

#include <DiaSaveGame/ISaveable.h>

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
    // ModifierSnapshot
    //
    // Read-only snapshot of one active modifier, for inspection (e.g. the visual
    // debugger). Not part of the resolution pipeline — a query surface only.
    // -----------------------------------------------------------------------
    struct ModifierSnapshot
    {
        Dia::Core::StringCRC modifier_name;
        ModifierOperation    operation;
        float                value;
        bool                 isConditional;           // when_condition was non-empty at AddModifier time
        bool                 conditionCurrentlyTrue;  // meaningful only if isConditional; true for unconditional modifiers
    };

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
    class AttributeSet : public Dia::SaveGame::ISaveable
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

        // Schema-invariant clamp range of the attribute at `index`. Returns 0.0f and asserts
        // if index is out of range. Same index-stability contract as GetValueByIndex.
        float GetMinimumValueByIndex(unsigned int index) const;
        float GetMaximumValueByIndex(unsigned int index) const;

        // -------------------------------------------------------------------
        // Modifier-stack inspection
        //
        // Query surface only — enumerates the modifiers currently registered against
        // one attribute, including each one's live condition state. Exists so an
        // out-of-module inspector (DiaAttributeVisualDebugger) can display the stack
        // without reaching into the private ModifierEntry / AttributeSlot types.
        //
        // Both methods assert (and return a benign default) on an unknown attribute or
        // an out-of-range index, matching every other index/name accessor in this class.
        // -------------------------------------------------------------------

        unsigned int     GetModifierCountForAttribute(Dia::Core::StringCRC attribute_name) const;
        ModifierSnapshot GetModifierSnapshotForAttribute(Dia::Core::StringCRC attribute_name, unsigned int index) const;

        // True if `attribute_name` is a known attribute on this set (i.e. present in the
        // schema this AttributeSet was created/initialized from). Used by Deserialize to
        // pre-check a saved modifier's attribute before ever calling AddModifier — see .cpp.
        bool HasAttribute(Dia::Core::StringCRC attribute_name) const;

        // -------------------------------------------------------------------
        // Dia::SaveGame::ISaveable
        //
        // Persists every attribute's base value and its full active modifier list
        // (modifier_name, attribute_name, operation, value, when_condition). A saved
        // modifier whose attribute_name no longer exists in the current schema is
        // dropped on load with a DIA_LOG_WARNING (schema drift between save and load
        // is expected data-compatibility handling, not a programmer error).
        //
        // ModifierHandle values are NOT preserved across save/load — Deserialize always
        // calls the public AddModifier, which mints fresh handles from this instance's
        // own HandlePool.
        // -------------------------------------------------------------------
        void     Serialize  (Dia::SaveGame::SaveContext& ctx) const override;
        void     Deserialize(Dia::SaveGame::LoadContext& ctx)       override;
        uint32_t GetVersion () const override { return 1; }

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
