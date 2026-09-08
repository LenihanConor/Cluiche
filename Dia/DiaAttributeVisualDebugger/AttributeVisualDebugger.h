////////////////////////////////////////////////////////////////////////////////
// Filename: AttributeVisualDebugger.h
// Description: IDebugDomain implementation for DiaAttribute.
//              Panel-only (no world drawers). Resolves the live selected entity,
//              reads its AttributeSetComponent, and emits every attribute's
//              resolved/base value, clamp range, and full modifier stack
//              (including live when_condition state).
//              Entire module is #ifdef DIA_DEBUG guarded.
// Feature spec: docs/specs/applications/dia/systems/diaattribute/visual-debugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>

#include <DiaAttribute/IAttributeObserver.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <DiaEntity/Entity.h>

#include <atomic>

namespace Dia::Debug     { class IDebugLayerRegistry; }
namespace Dia::Entity    { class Domain; }
namespace Dia::Attribute { class AttributeSet; class AttributeSetComponent; }

namespace Dia::AttributeVisualDebugger
{

////////////////////////////////////////////////////////////////////////////////
// AttributeVisualDebugger
//
// Panel-only IDebugDomain for per-entity attribute inspection.
//
// Selection resolution (see the feature spec's design decisions):
//   IDebugContext::GetSelectedEntityId() returns entity.GetIndex() + 1 and
//   therefore DISCARDS the handle generation — a full Dia::Entity::Entity cannot
//   be reconstructed from the raw id alone. Domain::GetAliveEntity(slotIndex)
//   exists for exactly this: it reconstructs the generation-correct handle for a
//   slot in O(1), and returns Entity::Invalid() for a dead slot (so a stale
//   selection id can never resolve to a recycled entity).
//
// This is the first panel-only domain in the codebase that needs the
// DebugLayerManager (its IDebugContext) for selection state. The manager is only
// ever handed to a domain through Register(), so Register/Unregister are
// overridden purely to capture/release that pointer — zero drawers are
// registered and HasWorldDrawers() stays false.
//
// Update path is deliberately dual:
//   - Push (AC-4): subscribed as an IAttributeObserver on the selected entity's
//     AttributeSet; OnAttributeChanged only sets mDirty, and the JSON tree is
//     rebuilt lazily on the next GetJSONState(). Non-dirty frames reuse the
//     cached tree, so this is genuinely push-driven rather than frame-driven.
//   - Bounded poll (AC-5): condition-driven value changes fire NO change event
//     (a documented DiaAttribute Feature 3 gap), so a push-only overlay would be
//     blind to exactly the bug class this domain exists to surface. Every
//     kConditionalPollIntervalFrames frames the conditional modifiers of the
//     selected entity ONLY are re-evaluated and diffed. Not a global always-on tick.
////////////////////////////////////////////////////////////////////////////////
class AttributeVisualDebugger : public Dia::VisualDebugger::IDebugDomain,
                                public Dia::Attribute::IAttributeObserver
{
public:
    // Cadence (in GetJSONState calls) of the conditional-modifier re-check. At the
    // panel's ~30-60fps refresh this is a couple of times a second — a bounded
    // backstop for the condition-driven-change gap, not a per-frame recompute.
    static const unsigned int kConditionalPollIntervalFrames = 30;

    // domain must outlive this object. Dia::Entity::Domain IS-A IEntityInspectable, and
    // this domain additionally needs Domain's own API (GetComponent<T>, GetAliveEntity),
    // so a single Domain& is taken rather than a redundant inspectable+domain pair.
    explicit AttributeVisualDebugger(Dia::Entity::Domain& domain);
    ~AttributeVisualDebugger() override;

    // ---- IDebugDomain: identity ----
    Dia::Core::StringCRC GetDomainId()     const override; ///< "Attribute"
    const char*          GetDisplayName()  const override; ///< "Attribute"
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override; ///< "AIBehavior"
    Dia::Core::RGBA      GetAccentColour() const override; ///< DebugGroupAccents::kAIBehavior

    bool HasWorldDrawers() const override { return false; }

    // ---- IDebugDomain: lifecycle (context capture only — no drawers) ----
    void Register(Dia::Debug::IDebugLayerRegistry& mgr)   override;
    void Unregister(Dia::Debug::IDebugLayerRegistry& mgr) override;

    // ---- IDebugDomain: panel bridge ----
    void GetJSONState(Json::Value& out) override;
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    // ---- Dia::Attribute::IAttributeObserver (push path, AC-4) ----
    void OnAttributeChanged(const Dia::Attribute::AttributeChangedEvent&) override;

    /// Test-support only — the number of times GetJSONState() actually rebuilt the
    /// attributes/modifiers JSON, as opposed to reusing the cached tree from a
    /// non-dirty frame. Proves the push-driven dirty-tracking is real rather than
    /// an unconditional per-frame recompute that merely happens to be correct.
    unsigned int GetRebuildCountForTesting() const { return mRebuildCount; }

private:
    /// Generation-correct resolution of IDebugContext::GetSelectedEntityId().
    /// Returns a default (invalid) Entity when nothing is selected, the captured
    /// layer manager is absent, or the id matches no live entity.
    Dia::Entity::Entity ResolveSelectedEntity() const;

    void RebuildJSONState(Dia::Entity::Entity entity, Dia::Attribute::AttributeSetComponent& comp);

    /// Subscribe/unsubscribe as the selection changes. Always marks dirty.
    /// `entity` is the (already-resolved) entity that owns `comp`, or Entity::Invalid()
    /// when comp is nullptr. Needed alongside comp so the PREVIOUSLY-observed component
    /// can be liveness-checked (via mObservedEntity) before it is dereferenced — see
    /// mObservedEntity's comment.
    void RebindObserver(Dia::Entity::Entity entity, Dia::Attribute::AttributeSetComponent* comp);

    /// Re-evaluates only the conditional modifiers of comp, diffs them against
    /// mConditionCache and refreshes the cache. Returns true if any flipped (AC-5).
    bool PollConditionalModifiersChanged(Dia::Attribute::AttributeSetComponent& comp);

    /// Rewrites mConditionCache to the set's current conditional-modifier states.
    /// Called after every rebuild so the cache always matches what the panel shows.
    void RefreshConditionalCache(const Dia::Attribute::AttributeSet& set);

    struct ConditionCacheEntry
    {
        Dia::Core::StringCRC attributeName;
        Dia::Core::StringCRC modifierName;
        bool                 lastTrue;
    };

    // Per-selected-entity, not global — a small fixed cap is ample.
    static const unsigned int kMaxTrackedConditionalModifiers = 64;

    Dia::Entity::Domain&           mDomain;
    Dia::Debug::IDebugLayerRegistry* mLayerManager = nullptr;

    // Currently-subscribed component, or nullptr. Non-owning.
    Dia::Attribute::AttributeSetComponent* mObservedComponent = nullptr;

    // The entity that owns mObservedComponent, or Entity::Invalid() when mObservedComponent
    // is nullptr. Carried alongside the component pointer purely so RebindObserver can
    // confirm — via mDomain.GetAliveEntity — that the PREVIOUS selection is still alive
    // (same generation) before dereferencing mObservedComponent. Without this, an entity
    // destroyed since the last GetJSONState() call leaves mObservedComponent dangling (or
    // pointing at a recycled, unrelated component), and unsubscribing through it would be a
    // use-after-free / wrong-object write.
    Dia::Entity::Entity mObservedEntity;

    bool         mDirty        = true; ///< start dirty so the first GetJSONState always builds
    Json::Value  mCachedState;         ///< reused on non-dirty, non-poll-triggered frames
    unsigned int mRebuildCount = 0;
    unsigned int mFrameCounter = 0;    ///< drives the bounded poll cadence (AC-5)

    // OnCommand arrives on the Render PU while GetJSONState runs on the Sim PU, so the
    // toggle flag is atomic (same reasoning as BlackboardVisualDebugger). The dirty flag
    // is deliberately NOT written by OnCommand — GetJSONState diffs the flag against
    // mLastEnabledSeen on the sim thread instead, keeping all mDirty writes single-threaded.
    std::atomic<bool> mInspectorEnabled{true};
    bool              mLastEnabledSeen = true;

    Dia::Core::Containers::DynamicArrayC<ConditionCacheEntry, kMaxTrackedConditionalModifiers> mConditionCache;
};

} // namespace Dia::AttributeVisualDebugger

#endif // DIA_DEBUG
