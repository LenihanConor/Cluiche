#include <DiaAttribute/ParameterFile.h>

#include <DiaCore/Core/Assert.h>
#include <DiaValueSchema/ValueSchemaLoader.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace Dia::Attribute
{
    namespace
    {
        using Dia::ValueSchema::SetText;
        using Dia::ValueSchema::ValueInfo;
        using Dia::ValueSchema::ValueKind;
        using Dia::ValueSchema::ValueType;
        using Dia::ValueSchema::ConstraintOp;

        constexpr uint32_t kMaxWarnings = 64;

        void Format(char* buf, size_t size, const char* fmt, va_list args) { vsnprintf(buf, size, fmt, args); }

        bool FailErr(ParamError& err, const char* key, const char* fmt, ...)
        {
            char buf[256];
            va_list args;
            va_start(args, fmt);
            Format(buf, sizeof(buf), fmt, args);
            va_end(args);
            SetText(err.key, key);
            SetText(err.otherKey, "");
            SetText(err.reason, buf);
            return false;
        }

        void AddWarning(ParamWarnings* warnings, const char* key, const char* fmt, ...)
        {
            if (warnings == nullptr) return;
            if (warnings->items.Size() >= kMaxWarnings) { ++warnings->dropped; return; }
            char buf[256];
            va_list args;
            va_start(args, fmt);
            Format(buf, sizeof(buf), fmt, args);
            va_end(args);
            ParamWarning w;
            SetText(w.key, key);
            SetText(w.reason, buf);
            warnings->items.Add(w);
        }

        bool Holds(float a, ConstraintOp op, float b)
        {
            switch (op)
            {
            case ConstraintOp::Lt: return a <  b;
            case ConstraintOp::Le: return a <= b;
            case ConstraintOp::Gt: return a >  b;
            case ConstraintOp::Ge: return a >= b;
            case ConstraintOp::Eq: return a == b;
            default:               return a != b;
            }
        }

        // -------------------------------------------------------------------
        // SchemaHash byte layout (FNV-1a 64: offset 14695981039346656037, prime 1099511628211).
        //
        // Primitives (all little-endian):
        //   u8   one byte
        //   u32  four bytes
        //   f64  the IEEE-754 bit pattern of a double, 8 bytes; -0.0 is normalised to +0.0
        //   str  u32 byte length, then the bytes (no terminator)
        //
        // Stream, in this order:
        //   for each sidecar entry with kind == parameter, in file order:
        //     u8  'P'
        //     str key
        //     u8  type          (ValueType: Float 0, Int 1, Bool 2, Enum 3, String 4, Ref 5, RefList 6, Map 7)
        //     u8  hasMin, f64 min   (min is 0 when hasMin is 0)
        //     u8  hasMax, f64 max   (max is 0 when hasMax is 0)
        //     u8  hasDefault, f64 defaultNumber, str defaultString   (zero / empty when absent)
        //     u8  mapValueType, str mapKeyRef                         (0 / empty unless type is Map)
        //     u8  hasMapDefault, u32 mapDefaultCount,
        //         then per map default, in loader (object) order: str mapKey, f64 number, str string
        //     u32 enumCount, then per enum value in file order: str id   (enum ids AND their order)
        //     u32 bandCount, then per band in loader order: str name, f64 lo, f64 hi
        //   then u8 'C', u32 constraintCount, then per constraint in file order: str a, u8 op, str b
        //     (op: Lt 0, Le 1, Gt 2, Ge 3, Eq 4, Ne 5)
        //
        // Excluded (presentation): label, description, hint, unit, ui, colour, enum labels/descriptions,
        // the sidecar `for`, bandSchemaVersion, and every non-parameter entry. The sidecar has no `keyEnum` field,
        // so `keyRef` (map key reference) is the only map key input.
        // -------------------------------------------------------------------
        class Fnv64
        {
        public:
            void Byte(uint8_t b) { mHash ^= b; mHash *= 1099511628211ull; }
            void U32(uint32_t v) { for (int i = 0; i < 4; ++i) Byte(static_cast<uint8_t>(v >> (8 * i))); }
            void F64(double d)
            {
                if (d == 0.0) d = 0.0;   // -0.0 -> +0.0
                uint64_t bits;
                memcpy(&bits, &d, sizeof(bits));
                for (int i = 0; i < 8; ++i) Byte(static_cast<uint8_t>(bits >> (8 * i)));
            }
            void Str(const char* s)
            {
                const uint32_t n = static_cast<uint32_t>(strlen(s));
                U32(n);
                for (uint32_t i = 0; i < n; ++i) Byte(static_cast<uint8_t>(s[i]));
            }
            uint64_t Value() const { return mHash; }
        private:
            uint64_t mHash = 14695981039346656037ull;
        };

        uint64_t ComputeSchemaHash(const Dia::ValueSchema::LoadResult& r)
        {
            Fnv64 h;
            for (unsigned int i = 0; i < r.mEntries.Size(); ++i)
            {
                const ValueInfo& e = r.mEntries[i];
                if (e.mKind != ValueKind::Parameter) continue;
                const bool isMap = e.mType == ValueType::Map;

                h.Byte('P');
                h.Str(e.mKey.AsCStr());
                h.Byte(static_cast<uint8_t>(e.mType));
                h.Byte(e.mHasMin ? 1 : 0); h.F64(e.mHasMin ? e.mMin : 0.0);
                h.Byte(e.mHasMax ? 1 : 0); h.F64(e.mHasMax ? e.mMax : 0.0);
                h.Byte(e.mHasDefault ? 1 : 0);
                h.F64(e.mHasDefault ? e.mDefaultNumber : 0.0);
                h.Str(e.mHasDefault ? e.mDefaultString.AsCStr() : "");
                h.Byte(isMap ? static_cast<uint8_t>(e.mMapValueType) : 0);
                h.Str(isMap ? e.mMapKeyRef.AsCStr() : "");
                const bool hasMapDefault = isMap && e.mHasMapDefault;
                h.Byte(hasMapDefault ? 1 : 0);
                const uint32_t mdCount = hasMapDefault ? e.mMapDefaults.Size() : 0;
                h.U32(mdCount);
                for (uint32_t d = 0; d < mdCount; ++d)
                {
                    h.Str(e.mMapDefaults[d].mMapKey.AsCStr());
                    h.F64(e.mMapDefaults[d].mNumber);
                    h.Str(e.mMapDefaults[d].mString.AsCStr());
                }
                h.U32(e.mEnumValues.Size());
                for (unsigned int v = 0; v < e.mEnumValues.Size(); ++v) h.Str(e.mEnumValues[v].mId.AsCStr());
                h.U32(e.mBands.Size());
                for (unsigned int b = 0; b < e.mBands.Size(); ++b)
                {
                    h.Str(e.mBands[b].mName.AsCStr());
                    h.F64(e.mBands[b].mLo);
                    h.F64(e.mBands[b].mHi);
                }
            }
            h.Byte('C');
            h.U32(r.mConstraints.Size());
            for (unsigned int c = 0; c < r.mConstraints.Size(); ++c)
            {
                h.Str(r.mConstraints[c].mA.AsCStr());
                h.Byte(static_cast<uint8_t>(r.mConstraints[c].mOp));
                h.Str(r.mConstraints[c].mB.AsCStr());
            }
            return h.Value();
        }

        // basedOn is a hex string; anything else (missing, non-string, unparsable) is stale.
        bool BasedOnMatches(const Json::Value& pack, uint64_t hash)
        {
            if (!pack.isMember("basedOn") || !pack["basedOn"].isString()) return false;
            const std::string s = pack["basedOn"].asString();
            if (s.empty()) return false;
            char* end = nullptr;
            const unsigned long long v = strtoull(s.c_str(), &end, 16);
            return end != nullptr && *end == '\0' && v == hash;
        }
    }

    ParameterFile::ParameterFile() = default;
    ParameterFile::~ParameterFile() = default;

    AttributeSet& ParameterFile::Values()
    {
        return mValues;
    }

    const AttributeSet& ParameterFile::Values() const
    {
        return mValues;
    }

    // -----------------------------------------------------------------------
    // Load (strict)
    // -----------------------------------------------------------------------
    bool ParameterFile::Load(const char* sidecarText, const char* sidecarName, const IMapKeySource* mapKeys, ParamError& err)
    {
        SetText(err.key, "");
        SetText(err.otherKey, "");
        SetText(err.reason, "");

        if (mLoaded)
            return FailErr(err, sidecarName != nullptr ? sidecarName : "", "ParameterFile is already loaded");

        const Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(sidecarText, sidecarName);
        if (!r.mOk)
        {
            // File-level failures (envelope) have no entry; their reason starts with the quoted offending
            // field ('format', 'version', ...), which is the key. Anything else names the sidecar.
            std::string key = r.mFailure.mEntry.AsCStr();
            if (key.empty())
            {
                const std::string reason = r.mFailure.mReason.AsCStr();
                const size_t close = reason.size() > 1 && reason[0] == '\'' ? reason.find('\'', 1) : std::string::npos;
                key = close != std::string::npos ? reason.substr(1, close - 1) : std::string(sidecarName != nullptr ? sidecarName : "sidecar");
            }
            SetText(err.key, key.c_str());
            SetText(err.reason, r.mFailure.mReason.AsCStr());
            return false;
        }

        AttributeSchema built;
        SchemaError se;
        if (!AttributeSchema::FromValueSchema(r, mapKeys, built, se))
        {
            SetText(err.key, se.key.AsCStr());
            SetText(err.reason, se.reason.AsCStr());
            return false;
        }

        mSchema = built;
        mValues.InitializeFromSchema(mSchema);
        SetText(mFor, r.mFor.AsCStr());

        // Map columns are contiguous per map entry, in key-source order.
        mMaps.RemoveAll();
        for (unsigned int i = 0; i < r.mEntries.Size(); ++i)
        {
            const ValueInfo& e = r.mEntries[i];
            if (e.mKind != ValueKind::Parameter || e.mType != ValueType::Map) continue;
            MapKeys keys;
            mapKeys->GetKeys(e.mMapKeyRef.AsCStr(), keys);   // validated by FromValueSchema
            MapInfo info;
            SetText(info.key, e.mKey.AsCStr());
            info.count = keys.count;
            info.firstIndex = 0;
            if (keys.count > 0)
            {
                char first[256];
                snprintf(first, sizeof(first), "%s.%s", e.mKey.AsCStr(), keys.names[0]);
                const AttributeDefinition* def = mSchema.FindAttribute(Dia::Core::StringCRC(first));
                DIA_ASSERT(def != nullptr, "ParameterFile::Load: map column missing from schema");
                info.firstIndex = static_cast<uint32_t>(def - &mSchema.GetAttributeByIndex(0));
            }
            if (mMaps.Capacity() <= mMaps.Size()) mMaps.Reserve(mMaps.Size() + 4);
            mMaps.Add(info);
        }

        mSchemaHash = ComputeSchemaHash(r);
        mMapKeys = mapKeys;
        SetText(mSidecarName, sidecarName != nullptr ? sidecarName : "");
        mLoaded = true;
        return true;
    }

    // -----------------------------------------------------------------------
    // Merge core
    // -----------------------------------------------------------------------
    bool ParameterFile::MergeValue(const char* key, uint32_t index, const Json::Value& v, LoadPolicy policy,
                                   float* candidate, ParamWarnings* warnings, ParamError& err)
    {
        const AttributeDefinition& def = mSchema.GetAttributeByIndex(index);
        const bool strict = policy == LoadPolicy::Strict;

        // A fault is fatal in Strict, a dropped value with a warning in Lenient.
        auto fault = [&](const char* fmt, auto... args) -> bool
        {
            if (strict) return FailErr(err, key, fmt, args...);
            AddWarning(warnings, key, fmt, args...);
            return true;
        };

        switch (def.kind)
        {
        case AttributeKind::Enum:
        {
            if (!v.isString()) return fault("expected an enum id string");
            const std::string id = v.asString();
            for (unsigned int i = 0; i < def.enumIdCount; ++i)
            {
                if (strcmp(mSchema.GetEnumId(index, i).AsCStr(), id.c_str()) == 0)
                {
                    candidate[index] = static_cast<float>(i);
                    return true;
                }
            }
            return fault("'%s' is not an id of this enum", id.c_str());
        }
        case AttributeKind::Bool:
        {
            if (v.isBool()) { candidate[index] = v.asBool() ? 1.0f : 0.0f; return true; }
            if (v.isNumeric())
            {
                const double d = v.asDouble();
                if (d == 0.0 || d == 1.0) { candidate[index] = static_cast<float>(d); return true; }
            }
            return fault("expected a boolean (true/false or 0/1)");
        }
        default:
        {
            if (!v.isNumeric()) return fault("expected a number");
            double d = v.asDouble();
            if (std::isnan(d)) return fault("value is not a number");
            if (def.kind == AttributeKind::Int && d != std::floor(d))
                return fault("int value must be a whole number (got %g)", d);
            const double mn = static_cast<double>(def.minimum_value);
            const double mx = static_cast<double>(def.maximum_value);
            if (d < mn || d > mx)
            {
                if (strict) return FailErr(err, key, "value %g is outside [%g, %g]", d, mn, mx);
                AddWarning(warnings, key, "value %g is outside [%g, %g]; clamped", d, mn, mx);
                d = d < mn ? mn : mx;
            }
            candidate[index] = static_cast<float>(d);
            return true;
        }
        }
    }

    bool ParameterFile::Merge(const Json::Value& obj, LoadPolicy policy, float* candidate, ParamWarnings* warnings, ParamError& err)
    {
        SetText(err.key, "");
        SetText(err.otherKey, "");
        SetText(err.reason, "");

        if (!obj.isObject()) return FailErr(err, "", "value set must be a JSON object");
        const bool strict = policy == LoadPolicy::Strict;
        const unsigned int count = mSchema.GetAttributeCount();

        const std::vector<std::string> names = obj.getMemberNames();
        for (const std::string& name : names)
        {
            if (!strict && (name == "format" || name == "version" || name == "basedOn")) continue;
            const Json::Value& v = obj[name];

            // scalar (or a flat "<map>.<mapKey>" column)
            const AttributeDefinition* def = mSchema.FindAttribute(Dia::Core::StringCRC(name.c_str()));
            if (def != nullptr && strcmp(def->fullKey.AsCStr(), name.c_str()) == 0)
            {
                const uint32_t index = static_cast<uint32_t>(def - &mSchema.GetAttributeByIndex(0));
                if (!MergeValue(name.c_str(), index, v, policy, candidate, warnings, err)) return false;
                continue;
            }

            // map: nested object keyed by map key
            const MapInfo* map = nullptr;
            for (unsigned int m = 0; m < mMaps.Size(); ++m)
            {
                if (strcmp(mMaps[m].key.AsCStr(), name.c_str()) == 0) { map = &mMaps[m]; break; }
            }
            if (map != nullptr)
            {
                if (!v.isObject())
                {
                    if (strict) return FailErr(err, name.c_str(), "a map value must be an object keyed by map key");
                    AddWarning(warnings, name.c_str(), "a map value must be an object keyed by map key; dropped");
                    continue;
                }
                const size_t prefix = strlen(map->key.AsCStr()) + 1;
                const std::vector<std::string> mapKeys = v.getMemberNames();
                for (const std::string& mk : mapKeys)
                {
                    const std::string full = name + "." + mk;
                    bool found = false;
                    for (uint32_t c = 0; c < map->count && !found; ++c)
                    {
                        const uint32_t index = map->firstIndex + c;
                        if (strcmp(mSchema.GetAttributeByIndex(index).fullKey.AsCStr() + prefix, mk.c_str()) == 0)
                        {
                            found = true;
                            if (!MergeValue(full.c_str(), index, v[mk], policy, candidate, warnings, err)) return false;
                        }
                    }
                    if (!found)
                    {
                        if (strict) return FailErr(err, full.c_str(), "'%s' is not a key of this map", mk.c_str());
                        AddWarning(warnings, full.c_str(), "'%s' is not a key of this map; dropped", mk.c_str());
                    }
                }
                continue;
            }

            if (strict) return FailErr(err, name.c_str(), "unknown parameter key");
            AddWarning(warnings, name.c_str(), "unknown parameter key; dropped");
        }

        SchemaError se;
        if (!mSchema.CheckConstraints(candidate, count, se))
        {
            FailErr(err, se.key.AsCStr(), "%s", se.reason.AsCStr());
            for (unsigned int c = 0; c < mSchema.GetConstraintCount(); ++c)
            {
                const ConstraintIdx& ci = mSchema.GetConstraintByIndex(c);
                if (!Holds(candidate[ci.a], ci.op, candidate[ci.b]))
                {
                    SetText(err.key, mSchema.GetAttributeByIndex(ci.a).fullKey.AsCStr());
                    SetText(err.otherKey, mSchema.GetAttributeByIndex(ci.b).fullKey.AsCStr());
                    break;
                }
            }
            return false;
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // ApplyPack (lenient, atomic)
    // -----------------------------------------------------------------------
    bool ParameterFile::ApplyPack(const Json::Value& pack, ParamWarnings& warn, ParamError& err)
    {
        SetText(err.key, "");
        SetText(err.otherKey, "");
        SetText(err.reason, "");

        if (!mLoaded) return FailErr(err, "", "ParameterFile is not loaded");
        if (!pack.isObject()) return FailErr(err, "pack", "a pack must be a JSON object");
        if (!pack.isMember("format") || !pack["format"].isString() || pack["format"].asString() != mFor.AsCStr())
            return FailErr(err, "format", "pack 'format' must be \"%s\"", mFor.AsCStr());
        if (!pack.isMember("version") || !pack["version"].isInt() || pack["version"].asInt() != 1)
            return FailErr(err, "version", "pack 'version' must be the integer 1");

        const unsigned int count = mSchema.GetAttributeCount();
        std::vector<float> candidate(count > 0 ? count : 1, 0.0f);
        for (unsigned int i = 0; i < count; ++i) candidate[i] = mValues.GetBaseValueByIndex(i);

        if (!BasedOnMatches(pack, mSchemaHash))
            AddWarning(&warn, "basedOn", "pack is stale: 'basedOn' is missing or does not match the current schema hash");

        if (!Merge(pack, LoadPolicy::Lenient, candidate.data(), &warn, err))
            return false;

        return mValues.ReplaceBaseValues(candidate.data(), count);
    }

    // -----------------------------------------------------------------------
    // Set (strict, atomic)
    // -----------------------------------------------------------------------
    bool ParameterFile::Set(Dia::Core::StringCRC key, const Json::Value& value, ParamError& err)
    {
        SetText(err.key, "");
        SetText(err.otherKey, "");
        SetText(err.reason, "");

        if (!mLoaded) return FailErr(err, key.AsChar(), "ParameterFile is not loaded");
        const AttributeDefinition* def = mSchema.FindAttribute(key);
        if (def == nullptr) return FailErr(err, key.AsChar(), "unknown parameter key");

        const unsigned int count = mSchema.GetAttributeCount();
        std::vector<float> candidate(count > 0 ? count : 1, 0.0f);
        for (unsigned int i = 0; i < count; ++i) candidate[i] = mValues.GetBaseValueByIndex(i);

        Json::Value obj(Json::objectValue);
        obj[def->fullKey.AsCStr()] = value;
        if (!Merge(obj, LoadPolicy::Strict, candidate.data(), nullptr, err))
            return false;

        return mValues.ReplaceBaseValues(candidate.data(), count);
    }

    // -----------------------------------------------------------------------
    // Reload (atomic): build the complete new state aside, then swap in place
    // -----------------------------------------------------------------------
    bool ParameterFile::Reload(const char* sidecarText, const Json::Value* const* packs, uint32_t packCount,
                               ParamWarnings& warn, ParamError& err)
    {
        SetText(err.key, "");
        SetText(err.otherKey, "");
        SetText(err.reason, "");

        if (packCount > 0 && packs == nullptr) return FailErr(err, "packs", "pack list is null");

        // 1. Build the whole new state in a local ParameterFile (fresh Load, then the packs in order).
        ParameterFile next;
        ParamWarnings local;
        if (!next.Load(sidecarText, mSidecarName.AsCStr()[0] != '\0' ? mSidecarName.AsCStr() : "sidecar", mMapKeys, err))
            return false;
        for (uint32_t i = 0; i < packCount; ++i)
        {
            if (packs[i] == nullptr) return FailErr(err, "packs", "pack %u is null", i);
            if (!next.ApplyPack(*packs[i], local, err)) return false;
        }

        // 2. Nothing below can fail. Warn about modifiers that lose their attribute (old schema still in place).
        if (mLoaded)
        {
            for (unsigned int i = 0; i < mSchema.GetAttributeCount(); ++i)
            {
                const AttributeDefinition& d = mSchema.GetAttributeByIndex(i);
                if (next.mSchema.FindAttribute(d.attribute_name) != nullptr) continue;
                const unsigned int n = mValues.GetModifierCountForAttribute(d.attribute_name);
                if (n > 0) AddWarning(&local, d.fullKey.AsCStr(), "attribute no longer exists; %u modifier(s) dropped", n);
            }
        }

        // 3. Swap. Everything an observer might read is in place before the values (and notifications) commit.
        const unsigned int count = next.mSchema.GetAttributeCount();
        std::vector<float> values(count > 0 ? count : 1, 0.0f);
        for (unsigned int i = 0; i < count; ++i) values[i] = next.mValues.GetBaseValueByIndex(i);

        SetText(mFor, next.mFor.AsCStr());
        mSchema = next.mSchema;
        mMaps.RemoveAll();
        if (next.mMaps.Size() > 0) mMaps.Reserve(next.mMaps.Size());
        for (unsigned int m = 0; m < next.mMaps.Size(); ++m) mMaps.Add(next.mMaps[m]);
        mSchemaHash = next.mSchemaHash;
        mMapKeys = next.mMapKeys;
        SetText(mSidecarName, next.mSidecarName.AsCStr());
        mLoaded = true;
        const bool swapped = mValues.ReplaceSchemaAndValues(mSchema, values.data(), count);
        DIA_ASSERT(swapped, "ParameterFile::Reload: value swap rejected a matching schema");
        (void)swapped;

        for (unsigned int i = 0; i < local.items.Size(); ++i)
        {
            if (warn.items.Size() >= kMaxWarnings) ++warn.dropped;
            else warn.items.Add(local.items[i]);
        }
        warn.dropped += local.dropped;
        return true;
    }
}
