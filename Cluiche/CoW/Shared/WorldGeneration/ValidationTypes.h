// S-1 Reachability Validator: result, evidence and injection interfaces.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md
//
// Cells are indices (row-major, y * width + x), the same as Site::cell, not coordinates. Ids are stored as the
// StringCRC value (uint32_t) because StringCRC is not trivially copyable (Site precedent); the typed edge is
// ValidationResult::Find(StringCRC) and MeasurementKeys. Public structs expose no std containers: lists are
// EvidenceList (private std::vector behind a small API).
#pragma once

#include "TerrainTypes.h"

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaPathfinding/IPathCostProvider.h>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace CoW {
namespace WorldGeneration {

constexpr uint32_t kNoCellIndex = 0xFFFFFFFFu;
constexpr unsigned kMaxDiagnostics = 32;           // stored per guarantee; the rest only increment droppedDiagnostics
constexpr unsigned kMaxValidationProfiles = 16;
constexpr unsigned kMaxValidationGuarantees = 32;

// Small growable list used for evidence. Value semantics; no std type in the public signatures.
template <class T>
class EvidenceList
{
public:
    void Add(const T& v) { mItems.push_back(v); }
    void Clear() { mItems.clear(); }
    unsigned Size() const { return static_cast<unsigned>(mItems.size()); }
    bool IsEmpty() const { return mItems.empty(); }
    const T& operator[](unsigned i) const { return mItems[i]; }
    T& operator[](unsigned i) { return mItems[i]; }
    const T* begin() const { return mItems.data(); }
    const T* end() const { return mItems.data() + mItems.size(); }
    std::span<const T> Span() const { return { mItems.data(), mItems.size() }; }
    bool operator==(const EvidenceList& o) const { return mItems == o.mItems; }

private:
    std::vector<T> mItems;
};

struct Diagnostic
{
    char text[160] = {};
    bool operator==(const Diagnostic& o) const;
};

// A path that proves reachability: cells from 'from' to 'to' inclusive, consecutive cells are hex neighbours.
struct ProofPath
{
    uint32_t from = kNoCellIndex;
    uint32_t to = kNoCellIndex;
    uint32_t profileCrc = 0;                 // profile that found it
    EvidenceList<uint32_t> cells;

    bool operator==(const ProofPath&) const = default;
};

struct CellPair
{
    uint32_t a = kNoCellIndex;
    uint32_t b = kNoCellIndex;
    bool operator==(const CellPair&) const = default;
};

struct Measurement
{
    uint32_t keyCrc = 0;                     // MeasurementKeys::*().Value()
    uint32_t a = kNoCellIndex;               // cells the value refers to, kNoCellIndex if none
    uint32_t b = kNoCellIndex;
    int32_t  tag = -1;                       // extra discriminator (e.g. Apotheosis type for thronesOfType), -1 if none
    float    value = 0.0f;
    bool operator==(const Measurement&) const = default;
};

namespace MeasurementKeys
{
    Dia::Core::StringCRC PairDistance();           // "pairDistance": hex distance between a dynasty pair
    Dia::Core::StringCRC MinimumSpacing();         // "minimumSpacing"
    Dia::Core::StringCRC ViolationCount();         // "violationCount": layout checks (tag = LayoutCode)
}

struct GuaranteeResult
{
    uint32_t idCrc = 0;                      // guarantee id (StringCRC::Value())
    bool satisfied = false;
    EvidenceList<Diagnostic> diagnostics;    // empty if satisfied; failure details if not (at most kMaxDiagnostics)
    uint32_t droppedDiagnostics = 0;         // diagnostics beyond kMaxDiagnostics
    EvidenceList<ProofPath> proofPaths;
    EvidenceList<uint32_t> violatingCells;
    EvidenceList<CellPair> violatingPairs;
    EvidenceList<Measurement> measurements;

    // printf-style; truncated to Diagnostic::text, counted in droppedDiagnostics past kMaxDiagnostics.
    void AddDiagnostic(const char* format, ...);

    bool operator==(const GuaranteeResult&) const = default;
};

struct ValidationResult
{
    bool passed = false;                     // every guarantee satisfied (and a profile was supplied)
    EvidenceList<GuaranteeResult> guarantees; // one per injected guarantee, injection order
    EvidenceList<Diagnostic> diagnostics;    // validation-level problems (e.g. no navigation profile supplied)

    const GuaranteeResult* Find(const Dia::Core::StringCRC& id) const { return Find(id.Value()); }
    const GuaranteeResult* Find(uint32_t idCrc) const;   // nullptr if not present

    bool operator==(const ValidationResult&) const = default;
};

// Answers reachability under the injected profiles (any-profile semantics).
class INavigationQuery
{
public:
    virtual ~INavigationQuery() = default;
    virtual bool IsReachable(uint32_t from, uint32_t to) const = 0;
    // As IsReachable, and fills 'out' with the first successful path (profiles in injection order).
    virtual bool FindProofPath(uint32_t from, uint32_t to, ProofPath& out) const = 0;
    // Additive: reach the cheapest of 'targets' in one search per profile (ties: lowest cell index). 'out.to' is the
    // target reached. False if no target is reachable or 'targets' is empty.
    virtual bool FindProofPathToAny(uint32_t from, std::span<const uint32_t> targets, ProofPath& out) const = 0;
};

// A kind of actor's navigability. Injected; S-1 ships the placeholder, B2 ships body plans.
class INavigationProfile
{
public:
    virtual ~INavigationProfile() = default;
    virtual Dia::Core::StringCRC GetId() const = 0;
    // The provider is bound to 'world' and owned by the caller for one Validate() call. GetCost takes offset
    // (col,row) coordinates and returns IPathCostProvider::kImpassableCost for blocked edges.
    virtual std::unique_ptr<Dia::Pathfinding::IPathCostProvider> CreateCostProvider(const WorldStructure& world) const = 0;
};

struct ValidationContext
{
    const WorldStructure& world;
    const INavigationQuery& navigation;
};

// A rule every accepted world must satisfy. Stateless per call; must be deterministic.
class IWorldGuarantee
{
public:
    virtual ~IWorldGuarantee() = default;
    virtual Dia::Core::StringCRC GetId() const = 0;
    virtual GuaranteeResult Check(const ValidationContext& context) const = 0;
};

struct ValidationParams
{
    Dia::Core::Containers::DynamicArrayC<const INavigationProfile*, kMaxValidationProfiles> profiles;     // non-owning
    Dia::Core::Containers::DynamicArrayC<const IWorldGuarantee*, kMaxValidationGuarantees> guarantees;     // non-owning

    // False (unchanged) for null or when full.
    bool AddProfile(const INavigationProfile* profile);
    bool AddGuarantee(const IWorldGuarantee* guarantee);
};

} // namespace WorldGeneration
} // namespace CoW
