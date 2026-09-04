#pragma once

// <array>/<utility> are used ONLY to build a compile-time table of function pointers
// (std::integer_sequence pack expansion). No STL type is ever a parameter or return type
// of a public runtime API here, so this does not breach PD-004/AD-002 ("no STL containers
// in public runtime APIs") — kAccessorTable is a constexpr lookup table, not a container
// that crosses an API boundary.
#include <array>
#include <utility>

#include <DiaCore/CRC/StringCRC.h>

#include <DiaAttribute/AttributeSet.h>

#include <DiaCondition/ConditionRegistry.h>

namespace Dia::Attribute {

    // -----------------------------------------------------------------------
    // kMaxBridgedAttributesPerSet
    //
    // Hard cap on the number of attributes a single AttributeSet can expose through a
    // ConditionRegistry. This is not an arbitrary budget — it is the size of the
    // compile-time trampoline table below. Raising it costs one extra template
    // instantiation (and one function-pointer table slot) per added index.
    // -----------------------------------------------------------------------
    inline constexpr unsigned int kMaxBridgedAttributesPerSet = 64;

    // -----------------------------------------------------------------------
    // BridgedAttributeAccessor<Index>
    //
    // One instantiation per bridgeable attribute index. Each instantiation is a distinct,
    // non-capturing function with the exact signature Dia::Condition::FloatAccessorFn
    // (float(*)(void*)), with the attribute index baked in as a template argument.
    //
    // This template exists because of a hard constraint in DiaCondition:
    // ConditionRegistry::RegisterFloat takes a plain C function pointer and no per-
    // registration user parameter — the single `data` pointer is fixed at registry
    // construction and shared by every accessor on that instance. So there is nowhere to
    // stash "which attribute am I?" at registration time, and attribute names are only
    // known at runtime (they come from JSON schemas). Baking the index into the type is
    // the only way to get N distinguishable function pointers without a capture.
    //
    // PRECONDITION: `data` must point at the AttributeSet that was passed to
    // RegisterAccessors. See the note on AttributeAccessorBridge below.
    // -----------------------------------------------------------------------
    template <unsigned int Index>
    float BridgedAttributeAccessor(void* data)
    {
        return static_cast<const AttributeSet*>(data)->GetValueByIndex(Index);
    }

    // -----------------------------------------------------------------------
    // MakeAccessorTable / kAccessorTable
    //
    // Expands the index pack into one BridgedAttributeAccessor instantiation per slot.
    // Entirely constexpr — no runtime allocation, no runtime initialisation.
    // -----------------------------------------------------------------------
    template <unsigned int... Is>
    constexpr auto MakeAccessorTable(std::integer_sequence<unsigned int, Is...>)
    {
        return std::array<Dia::Condition::FloatAccessorFn, sizeof...(Is)>{ &BridgedAttributeAccessor<Is>... };
    }

    inline constexpr auto kAccessorTable =
        MakeAccessorTable(std::make_integer_sequence<unsigned int, kMaxBridgedAttributesPerSet>{});

    // -----------------------------------------------------------------------
    // AttributeAccessorBridge
    //
    // Exposes an entity's resolved attribute values to DiaRules / DiaUtilityAI, which read
    // gameplay state exclusively through DiaCondition's ConditionRegistry (accessors keyed
    // by (slot, field) StringCRC pairs).
    //
    // -------------------------------------------------------------------------------
    // PRECONDITION — READ THIS. It cannot be checked in code.
    //
    //   `registry` MUST have been constructed as
    //       Dia::Condition::ConditionRegistry registry(const_cast<AttributeSet*>(&set));
    //   i.e. its `data` pointer must be the very AttributeSet passed to RegisterAccessors.
    //
    // Every bridged accessor reinterprets the registry's single `data` pointer as an
    // AttributeSet. ConditionRegistry exposes no getter for `data`, so AttributeAccessorBridge
    // has no way to verify this and no way to assert on it. Get it wrong and every bridged
    // accessor silently reads through the wrong object — undefined behaviour that will not
    // announce itself.
    // -------------------------------------------------------------------------------
    // LIFETIME HAZARD — also unenforceable.
    //
    // The bridged AttributeSet must outlive every ConditionRegistry it was registered into.
    // ConditionRegistry has no unregister method, so there is nothing to call on entity
    // despawn: the registered accessors keep pointing at the registry's `data` pointer for
    // as long as the registry lives. A registry bound to a per-entity AttributeSet must
    // therefore be owned by (and die with) that entity. This is a known, accepted hazard —
    // see Open Design Question #1 in the accessor-bridge spec.
    // -------------------------------------------------------------------------------
    // -----------------------------------------------------------------------
    class AttributeAccessorBridge
    {
    public:
        // Registers one float accessor per attribute in `set`'s schema, under `slot_name`,
        // so each becomes resolvable as registry.GetFloat(slot_name, attribute_name) and
        // usable from DiaCondition JSON as "slot_name.attribute_name".
        //
        // Accessors always report the LIVE resolved value (base + modifier stack + clamp) at
        // query time, never a registration-time snapshot.
        //
        // At most kMaxBridgedAttributesPerSet attributes are registered. Exceeding that
        // asserts in Debug and logs a warning + registers only the first
        // kMaxBridgedAttributesPerSet in Release.
        //
        // Re-registering an already-registered (slot_name, attribute_name) pair is a hard
        // error (asserts in Debug) rather than a silent overwrite, because ConditionRegistry
        // offers no way to detect or undo a replacement after the fact.
        static void RegisterAccessors(Dia::Condition::ConditionRegistry& registry,
                                      const AttributeSet& set,
                                      Dia::Core::StringCRC slot_name);
    };

} // namespace Dia::Attribute
