#pragma once

// ParamBinding<T> (DiaAttribute Parameter Files, Phase E).
//
// Copies the RESOLVED values (modifiers included) of an AttributeSet into a plain C++ struct T, so
// simulation hot loops read plain members. Header-only.
//
//   ParamBinding<TerrainRules> b;
//   b.Bind(StringCRC("sea.level"), &TerrainRules::seaLevel);          // float
//   b.Bind(StringCRC("sea.octaves"), &TerrainRules::octaves);         // int32_t (Int attribute)
//   b.BindIntFromFloat(StringCRC("sea.depth"), &TerrainRules::depth); // int32_t member on a FLOAT attribute (opt-in)
//   b.Bind(StringCRC("sea.enabled"), &TerrainRules::enabled);         // bool (0/1)
//   b.BindEnum(StringCRC("sea.mode"), &TerrainRules::mode);           // enum ordinal
//   b.BindMap(StringCRC("biome.wetness"), &TerrainRules::wetness);    // float[N] / int32_t[N] / bool[N] / E[N]
//   b.BindMap(StringCRC("biome.base"), &Rules::base, biomeCount);      // float[N] capacity, first biomeCount bound (run-time count)
//   BindingReport report;  b.Resolve(schema, report);                 // keys -> indices, once per schema
//   b.Snapshot(attributeSet, rules);                                  // no allocation, no hash lookup
//
// Contracts:
//  * Bind* stores a byte offset into T (captured once, at Bind time) and a key; Resolve turns each key into an
//    attribute INDEX once; Snapshot only calls AttributeSet::GetValueByIndex.
//  * Maps: a map entry `<key>` expands in the schema to attributes `<key>.<mapKey>` in key-source order. A bound
//    map covers all of them and element i of the destination array receives the i-th one. A bound count that
//    differs from the schema's expansion is reported in BindingReport::countMismatches and the map is skipped by
//    Snapshot. Maps are matched through the attribute's fullKey: a map key may itself contain dots.
//  * Kind check: Resolve reports (BindingReport::kindMismatches) a binding whose member kind does not match the
//    attribute kind: float<->Float, int32_t<->Int, bool<->Bool, BindEnum<->Enum, and the same per map column kind.
//    A mismatched binding is skipped by Snapshot. The one deliberate cross-kind form is BindIntFromFloat, which
//    binds an int32_t member to a FLOAT attribute: the value is rounded half away from zero and SATURATES to the
//    int32 range (NaN -> 0). Plain Bind(int32_t) against a Float attribute is a kind mismatch.
//  * Binding the same key twice is a programming error (DIA_ASSERT; the second binding is dropped).
//  * More than kMaxBindings bindings is a programming error (DIA_ASSERT; the extra binding is dropped).
//  * Snapshot skips bindings that Resolve could not resolve, and Snapshot before Resolve does nothing.
//  * T must be standard-layout for the offset arithmetic to be meaningful (static_assert).

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <DiaCore/Core/Assert.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String128.h>

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>

namespace Dia::Attribute {

    // Names reported by ParamBinding::Resolve. Each list holds up to kCapacity names; any overflow is counted
    // in the matching dropped counter (the report is still non-empty, so Resolve still returns false).
    struct BindingReport
    {
        static constexpr uint32_t kCapacity = 256;

        // Schema attributes (full key, `<key>.<mapKey>` for map columns) that no binding covers.
        Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String128, kCapacity> unboundParameters;
        // Bound keys that have no schema entry.
        Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String128, kCapacity> bindingsWithoutEntry;
        // Bound maps whose count differs from the schema's expansion ("<key>").
        Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String128, kCapacity> countMismatches;
        uint32_t droppedUnbound = 0;
        uint32_t droppedWithoutEntry = 0;
        uint32_t droppedMismatches = 0;
        // Bindings (maps: "<key>") whose member kind does not match the attribute kind.
        Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String128, kCapacity> kindMismatches;
        uint32_t droppedKindMismatches = 0;

        bool IsClean() const
        {
            return unboundParameters.Size() == 0 && bindingsWithoutEntry.Size() == 0 && countMismatches.Size() == 0
                && kindMismatches.Size() == 0
                && droppedUnbound == 0 && droppedWithoutEntry == 0 && droppedMismatches == 0 && droppedKindMismatches == 0;
        }
    };

    template<class T>
    class ParamBinding
    {
        static_assert(std::is_standard_layout_v<T>, "ParamBinding<T>: T must be standard-layout");

    public:
        // TerrainRules needs 99 rows today; this leaves generous headroom.
        static constexpr uint32_t kMaxBindings = 256;

        // ---- scalars ----------------------------------------------------------------------------------------
        void Bind(Dia::Core::StringCRC key, float T::* m)   { Add(key, Kind::Float, OffsetOf(m), 1, 0); }
        void Bind(Dia::Core::StringCRC key, int32_t T::* m) { Add(key, Kind::Int,   OffsetOf(m), 1, 0); }
        void Bind(Dia::Core::StringCRC key, bool T::* m)    { Add(key, Kind::Bool,  OffsetOf(m), 1, 0); }

        // Explicit opt-in: an int32_t member fed from a FLOAT attribute (e.g. a rule stored as a float in the sidecar
        // but read as a count). Rounds half away from zero, saturates to the int32 range, NaN -> 0. Against any
        // other attribute kind it is a kind mismatch. Plain Bind(int32_t) requires an Int attribute.
        void BindIntFromFloat(Dia::Core::StringCRC key, int32_t T::* m) { Add(key, Kind::IntFromFloat, OffsetOf(m), 1, 0); }

        // Stores the ordinal (the attribute's value) into an enum member of any underlying size (1, 2, 4, 8 bytes).
        template<class E>
        void BindEnum(Dia::Core::StringCRC key, E T::* m)
        {
            static_assert(std::is_enum_v<E>, "BindEnum: member must be an enum");
            Add(key, Kind::Enum, OffsetOf(m), 1, static_cast<uint8_t>(sizeof(E)));
        }

        // ---- maps: one array element per map key, key-source order -----------------------------------------
        template<size_t N> void BindMap(Dia::Core::StringCRC key, float   (T::* arr)[N]) { Add(key, Kind::FloatMap, OffsetOf(arr), N, 0); }
        template<size_t N> void BindMap(Dia::Core::StringCRC key, int32_t (T::* arr)[N]) { Add(key, Kind::IntMap,   OffsetOf(arr), N, 0); }
        template<size_t N> void BindMap(Dia::Core::StringCRC key, bool    (T::* arr)[N]) { Add(key, Kind::BoolMap,  OffsetOf(arr), N, 0); }
        template<class E, size_t N, class = std::enable_if_t<std::is_enum_v<E>>>
        void BindMap(Dia::Core::StringCRC key, E (T::* arr)[N]) { Add(key, Kind::EnumMap, OffsetOf(arr), N, static_cast<uint8_t>(sizeof(E))); }

        // First-element forms: `first` is the first of `count` contiguous members.
        void BindMap(Dia::Core::StringCRC key, float   T::* first, uint32_t count) { Add(key, Kind::FloatMap, OffsetOf(first), count, 0); }
        void BindMap(Dia::Core::StringCRC key, int32_t T::* first, uint32_t count) { Add(key, Kind::IntMap,   OffsetOf(first), count, 0); }
        void BindMap(Dia::Core::StringCRC key, bool    T::* first, uint32_t count) { Add(key, Kind::BoolMap,  OffsetOf(first), count, 0); }
        template<class E, class = std::enable_if_t<std::is_enum_v<E>>>
        void BindMap(Dia::Core::StringCRC key, E T::* first, uint32_t count) { Add(key, Kind::EnumMap, OffsetOf(first), count, static_cast<uint8_t>(sizeof(E))); }

        // Capacity forms: `arr` has room for N elements but only the first `count` are bound, for maps whose key count is
        // known at run time (e.g. one value per registry biome, up to a fixed capacity). Resolve requires the schema's
        // expansion to equal `count` (not N); Snapshot writes elements [0, count) and leaves the rest untouched.
        // count > N is a programming error (DIA_ASSERT; the binding is clamped to N).
        template<size_t N> void BindMap(Dia::Core::StringCRC key, float   (T::* arr)[N], uint32_t count) { Add(key, Kind::FloatMap, OffsetOf(arr), ClampCount(count, N), 0); }
        template<size_t N> void BindMap(Dia::Core::StringCRC key, int32_t (T::* arr)[N], uint32_t count) { Add(key, Kind::IntMap,   OffsetOf(arr), ClampCount(count, N), 0); }
        template<size_t N> void BindMap(Dia::Core::StringCRC key, bool    (T::* arr)[N], uint32_t count) { Add(key, Kind::BoolMap,  OffsetOf(arr), ClampCount(count, N), 0); }

        // Maps every key to its attribute index (once per schema) and checks coverage both ways. Returns true
        // when the report is clean. Safe to call again after a schema change (Reload): it re-resolves everything.
        bool Resolve(const AttributeSchema& schema, BindingReport& report)
        {
            report = BindingReport();
            const uint32_t n = schema.GetAttributeCount();

            Dia::Core::Containers::DynamicArray<uint8_t> covered(n > 0 ? n : 1);
            for (uint32_t i = 0; i < n; ++i) covered.Add(0);

            for (uint32_t b = 0; b < mEntries.Size(); ++b)
            {
                Entry& e = mEntries[b];
                e.resolved = false;
                e.index = 0;

                if (!IsMap(e.kind))
                {
                    bool found = false;
                    for (uint32_t i = 0; i < n && !found; ++i)
                    {
                        if (schema.GetAttributeByIndex(i).attribute_name == e.key)
                        {
                            e.index = i;
                            e.resolved = true;
                            covered[i] = 1;
                            found = true;
                            if (schema.GetAttributeByIndex(i).kind != ExpectedAttributeKind(e.kind))
                            {
                                e.resolved = false;
                                Push(report.kindMismatches, report.droppedKindMismatches, e.key.AsChar());
                            }
                        }
                    }
                    if (!found) Push(report.bindingsWithoutEntry, report.droppedWithoutEntry, e.key.AsChar());
                    continue;
                }

                // Map: the attributes whose fullKey is "<key>.<mapKey>".
                uint32_t first = 0, matches = 0;
                bool contiguous = true;
                for (uint32_t i = 0; i < n; ++i)
                {
                    if (!IsMapColumn(schema.GetAttributeByIndex(i).fullKey.AsCStr(), e.key)) continue;
                    if (matches == 0) first = i;
                    else if (i != first + matches) contiguous = false;
                    ++matches;
                    covered[i] = 1;
                }
                if (matches == 0)
                {
                    Push(report.bindingsWithoutEntry, report.droppedWithoutEntry, e.key.AsChar());
                    continue;
                }
                if (matches != e.count || !contiguous)
                {
                    Push(report.countMismatches, report.droppedMismatches, e.key.AsChar());
                    continue;
                }
                if (schema.GetAttributeByIndex(first).kind != ExpectedAttributeKind(e.kind))
                {
                    Push(report.kindMismatches, report.droppedKindMismatches, e.key.AsChar());
                    continue;
                }
                e.index = first;
                e.resolved = true;
            }

            for (uint32_t i = 0; i < n; ++i)
            {
                if (covered[i] == 0)
                    Push(report.unboundParameters, report.droppedUnbound, schema.GetAttributeByIndex(i).fullKey.AsCStr());
            }
            return report.IsClean();
        }

        // Reads RESOLVED values (modifiers included) by attribute index. No allocation, no hash lookup.
        void Snapshot(const AttributeSet& values, T& out) const
        {
            char* const base = reinterpret_cast<char*>(&out);
            for (uint32_t b = 0; b < mEntries.Size(); ++b)
            {
                const Entry& e = mEntries[b];
                if (!e.resolved) continue;
                char* const dst = base + e.offset;
                for (uint32_t k = 0; k < e.count; ++k)
                {
                    const float v = values.GetValueByIndex(e.index + k);
                    switch (e.kind)
                    {
                    case Kind::Float: case Kind::FloatMap: { const float x = v; std::memcpy(dst + k * sizeof(float), &x, sizeof(float)); break; }
                    case Kind::Int:   case Kind::IntMap:   case Kind::IntFromFloat: { const int32_t x = ToInt(v); std::memcpy(dst + k * sizeof(int32_t), &x, sizeof(int32_t)); break; }
                    case Kind::Bool:  case Kind::BoolMap:  { const bool x = (v != 0.0f); std::memcpy(dst + k * sizeof(bool), &x, sizeof(bool)); break; }
                    case Kind::Enum:  case Kind::EnumMap:  WriteOrdinal(dst + k * e.enumSize, e.enumSize, ToInt(v)); break;
                    }
                }
            }
        }

        uint32_t BindingCount() const { return mEntries.Size(); }

    private:
        enum class Kind : uint8_t { Float, Int, Bool, Enum, IntFromFloat, FloatMap, IntMap, BoolMap, EnumMap };

        struct Entry
        {
            Dia::Core::StringCRC key;
            Kind     kind     = Kind::Float;
            uint8_t  enumSize = 0;
            bool     resolved = false;
            uint32_t offset   = 0;   // byte offset of the member (maps: of the first element) inside T
            uint32_t count    = 1;   // elements (1 for scalars)
            uint32_t index    = 0;   // attribute index (maps: of the first column); valid when resolved
        };

        static bool IsMap(Kind k) { return k >= Kind::FloatMap; }

        static size_t ClampCount(uint32_t count, size_t capacity)
        {
            DIA_ASSERT(count <= capacity, "ParamBinding::BindMap: count %u exceeds the array capacity %u", count, static_cast<uint32_t>(capacity));
            return count <= capacity ? count : capacity;
        }

        static AttributeKind ExpectedAttributeKind(Kind k)
        {
            switch (k)
            {
            case Kind::Float: case Kind::IntFromFloat: case Kind::FloatMap: return AttributeKind::Float;
            case Kind::Int:   case Kind::IntMap:                            return AttributeKind::Int;
            case Kind::Bool:  case Kind::BoolMap:                           return AttributeKind::Bool;
            default:                                                        return AttributeKind::Enum;
            }
        }

        // Byte offset of a data member (or array member) inside T, computed against scratch storage so no T
        // is ever constructed and no null pointer is dereferenced.
        template<class M>
        static uint32_t OffsetOf(M T::* m)
        {
            alignas(T) static unsigned char scratch[sizeof(T)];
            T* const p = reinterpret_cast<T*>(scratch);
            const char* const member = reinterpret_cast<const char*>(&(p->*m));
            return static_cast<uint32_t>(member - reinterpret_cast<const char*>(scratch));
        }

        void Add(Dia::Core::StringCRC key, Kind kind, uint32_t offset, size_t count, uint8_t enumSize)
        {
            for (uint32_t i = 0; i < mEntries.Size(); ++i)
            {
                if (mEntries[i].key == key)
                {
                    DIA_ASSERT(false, "ParamBinding::Bind: key '%s' is already bound", key.AsChar());
                    return;
                }
            }
            if (mEntries.IsFull())
            {
                DIA_ASSERT(false, "ParamBinding::Bind: more than kMaxBindings (%u) bindings; raise kMaxBindings", kMaxBindings);
                return;
            }
            Entry e;
            e.key = key; e.kind = kind; e.enumSize = enumSize; e.offset = offset; e.count = static_cast<uint32_t>(count);
            mEntries.Add(e);
        }

        static void Push(Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String128, BindingReport::kCapacity>& list,
                         uint32_t& dropped, const char* text)
        {
            if (list.IsFull()) { ++dropped; return; }
            list.Add(Dia::Core::Containers::String128(text));
        }

        // True when fullKey is "<mapKey>.<something>" for the bound key; tries every '.' split because both the
        // bound key and the map key may contain dots.
        static bool IsMapColumn(const char* fullKey, Dia::Core::StringCRC key)
        {
            char prefix[256];
            for (size_t i = 0; fullKey[i] != '\0' && i < sizeof(prefix) - 1; ++i)
            {
                if (fullKey[i] != '.') continue;
                std::memcpy(prefix, fullKey, i);
                prefix[i] = '\0';
                if (Dia::Core::StringCRC(prefix) == key) return true;
            }
            return false;
        }

        // Half away from zero, saturating to the int32 range; NaN -> 0 (a bare cast would be undefined behaviour).
        static int32_t ToInt(float v)
        {
            const float r = v < 0.0f ? v - 0.5f : v + 0.5f;
            if (r != r) return 0;
            if (r >= 2147483648.0f) return INT32_MAX;
            if (r <= -2147483648.0f) return INT32_MIN;
            return static_cast<int32_t>(r);
        }

        static void WriteOrdinal(char* dst, uint8_t size, int32_t ordinal)
        {
            switch (size)
            {
            case 1: { const int8_t  x = static_cast<int8_t>(ordinal);  std::memcpy(dst, &x, 1); break; }
            case 2: { const int16_t x = static_cast<int16_t>(ordinal); std::memcpy(dst, &x, 2); break; }
            case 4: { const int32_t x = ordinal;                       std::memcpy(dst, &x, 4); break; }
            default: { const int64_t x = ordinal;                      std::memcpy(dst, &x, 8); break; }
            }
        }

        Dia::Core::Containers::DynamicArrayC<Entry, kMaxBindings> mEntries;
    };

} // namespace Dia::Attribute
