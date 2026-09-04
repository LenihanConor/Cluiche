#pragma once

#include <cstdint>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Handle.h>
#include <DiaCore/Containers/HandlePool.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/HashTables/HashTable.h>

#include <DiaAttribute/AttributeSchema.h>

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

        Dia::Core::StringCRC GetSchemaName() const;

    private:
        // Delegating constructor used by CreateFromSchema — constructs the
        // returned prvalue directly (guaranteed elision), never via copy/move.
        explicit AttributeSet(const AttributeSchema& schema);

        struct ModifierEntry
        {
            ModifierHandle     handle;
            AttributeModifier  modifier;
        };

        struct AttributeSlot
        {
            float base_value    = 0.0f;
            float minimum_value = 0.0f;
            float maximum_value = 0.0f;
            Dia::Core::Containers::DynamicArrayC<ModifierEntry, kMaxModifiersPerAttribute> modifiers;
        };

        static float ResolveValue(const AttributeSlot& slot);

        Dia::Core::StringCRC mSchemaName;
        Dia::Core::Containers::HashTable<Dia::Core::StringCRC, AttributeSlot> mSlots;

        // Mints ModifierHandle identity. Payload per-slot is the owning attribute_name,
        // so RemoveModifier(handle) can locate the AttributeSlot without a linear scan
        // of every attribute. Internally produces Handle<StringCRC> (matching the
        // payload template parameter); AddModifier/RemoveModifier translate between
        // that and the public ModifierHandle (Handle<ModifierTag>) by index+generation,
        // the same pattern Domain uses to translate Handle<EntitySlotData> <-> Entity.
        Dia::Core::HandlePool<Dia::Core::StringCRC, kMaxModifiersPerAttributeSet> mModifierHandlePool;
    };

} // namespace Dia::Attribute
