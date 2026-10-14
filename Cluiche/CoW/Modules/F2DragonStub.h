#pragma once
#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>
#include <cstdint>

namespace CoW {

// F-2 check C6 test fixture: a minimal dragon record whose attribute floats come from the DiaAttribute
// band resolver (InitializeFromBands). Feature-local; real dragons arrive with B6 Dragon Lifecycle.
// Spec: docs/specs/applications/cow/systems/f2-core-runtime/f2-core-runtime.md (C6, Open Design Question 2)

enum class F2DragonAttr : uint8_t { Health = 0, Strength = 1, Agility = 2, Fertility = 3 };
constexpr int kF2DragonAttrCount = 4;

enum class F2Band : uint8_t { None = 0, Low, Baseline, Medium, High, Apex };

struct F2DragonRecord
{
    uint32_t dragonId = 0;
    uint32_t bandSchemaVersion = 1;
    F2Band   bands[kF2DragonAttrCount] = { F2Band::Baseline, F2Band::High, F2Band::Medium, F2Band::Low };
};

class F2DragonStub
{
public:
    F2DragonStub();
    F2DragonStub(const F2DragonStub&) = delete;
    F2DragonStub& operator=(const F2DragonStub&) = delete;

    // Loads a record: runs the resolver exactly once and caches the floats.
    void Load(const F2DragonRecord& record);

    // True when the live record's band_schema_version differs from the one the cached floats were
    // resolved at; that is the only trigger for ReloadIfVersionChanged.
    bool VersionChanged() const { return mLoaded && mRecord.bandSchemaVersion != mResolvedVersion; }
    // Explicit reload: re-resolves only if the version was bumped. Returns whether it resolved.
    bool ReloadIfVersionChanged();

    // The in-memory record. Editing its bands alone must NOT change the cached floats.
    F2DragonRecord& MutableRecord() { return mRecord; }
    const F2DragonRecord& GetRecord() const { return mRecord; }

    // Cached resolved float (a live read; never re-resolves, unless the fault hook below is on).
    float Read(F2DragonAttr attr);

    // Number of times the resolver (AttributeSet::InitializeFromBands) was invoked.
    uint32_t GetResolverInvocations() const { return mInvocations; }

    // Schema range [min, max] of `band` for `attr`; false if the schema has no such band.
    bool GetBandRange(F2DragonAttr attr, F2Band band, float& outMin, float& outMax) const;

    // Test hook: makes every Read re-resolve from the current record bands (a deliberately broken variant).
    void SetReResolveOnRead(bool enable) { mReResolveOnRead = enable; }

    static const char* AttrName(F2DragonAttr attr);
    static const char* BandName(F2Band band);

private:
    void Resolve();   // the single call site of InitializeFromBands

    Dia::Attribute::AttributeSchema mSchema;
    Dia::Attribute::AttributeSet    mSet;
    F2DragonRecord                  mRecord;
    uint32_t                        mResolvedVersion = 0;
    uint32_t                        mInvocations = 0;
    bool                            mLoaded = false;
    bool                            mReResolveOnRead = false;
};

} // namespace CoW
