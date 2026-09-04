////////////////////////////////////////////////////////////////////////////////
// Filename: AttributeVisualDebugger.cpp
// Description: IDebugDomain implementation for DiaAttribute.
// Feature spec: docs/specs/applications/dia/systems/diaattribute/visual-debugger.md
////////////////////////////////////////////////////////////////////////////////
#include "AttributeVisualDebugger.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

#include <DiaAttribute/AttributeSet.h>
#include <DiaAttribute/AttributeSetComponent.h>

#include <DiaEntity/Domain.h>

namespace Dia::AttributeVisualDebugger
{

namespace
{
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kDrawerAttributeInspector("AttributeInspector");

    const char* OperationToString(Dia::Attribute::ModifierOperation op)
    {
        switch (op)
        {
        case Dia::Attribute::ModifierOperation::Add:      return "Add";
        case Dia::Attribute::ModifierOperation::Multiply: return "Multiply";
        case Dia::Attribute::ModifierOperation::Override: return "Override";
        }
        return "Unknown";
    }

    // StringCRC retains its source string in debug builds; fall back to an empty
    // label rather than emitting a null into the JSON tree.
    const char* SafeName(Dia::Core::StringCRC crc)
    {
        const char* str = crc.AsChar();
        return (str != nullptr && str[0] != '\0') ? str : "";
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AttributeVisualDebugger::AttributeVisualDebugger(Dia::Entity::Domain& domain)
    : mDomain(domain)
{
    mCachedState = Json::Value(Json::objectValue);
}

AttributeVisualDebugger::~AttributeVisualDebugger()
{
    // Never leave a dangling observer registration on a surviving AttributeSet.
    RebindObserver(nullptr);
}

// ---------------------------------------------------------------------------
// IDebugDomain: identity
// ---------------------------------------------------------------------------

Dia::Core::StringCRC AttributeVisualDebugger::GetDomainId() const
{
    return Dia::Core::StringCRC("Attribute");
}

const char* AttributeVisualDebugger::GetDisplayName() const
{
    return "Attribute";
}

const char* AttributeVisualDebugger::GetDescription() const
{
    // Must stay <= 80 characters.
    return "Selected entity attributes - values, ranges, and modifier stack";
}

Dia::Core::StringCRC AttributeVisualDebugger::GetGroup() const
{
    return Dia::Core::StringCRC("AIBehavior");
}

Dia::Core::RGBA AttributeVisualDebugger::GetAccentColour() const
{
    return Dia::VisualDebugger::DebugGroupAccents::kAIBehavior;
}

// ---------------------------------------------------------------------------
// IDebugDomain: lifecycle
//
// Panel-only domain: zero drawers are registered. Register/Unregister are
// overridden purely to capture the DebugLayerManager, which is the only channel
// through which a domain receives its IDebugContext (and therefore the live
// selected-entity id).
// ---------------------------------------------------------------------------

void AttributeVisualDebugger::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr)
        return; // already registered — idempotent, matching EntityDebugDomain

    mLayerManager = &mgr;
}

void AttributeVisualDebugger::Unregister(Dia::Debug::DebugLayerManager& /*mgr*/)
{
    // Selection is no longer observable — drop the current subscription so the
    // domain does not hold an observer registration it can no longer refresh.
    RebindObserver(nullptr);
    mLayerManager = nullptr;
}

// ---------------------------------------------------------------------------
// Selection resolution
// ---------------------------------------------------------------------------

Dia::Entity::Entity AttributeVisualDebugger::ResolveSelectedEntity() const
{
    if (mLayerManager == nullptr)
        return Dia::Entity::Entity::Invalid();

    // IDebugContext contract: 0 means "nothing selected", otherwise the value is
    // entity.GetIndex() + 1 (the generation is not carried).
    const uint32_t selectedId = mLayerManager->GetSelectedEntityId();
    if (selectedId == 0)
        return Dia::Entity::Entity::Invalid();

    // Reconstructs the generation from the live slot, so a stale id whose entity has
    // since been destroyed resolves to Invalid() rather than to a recycled entity.
    return mDomain.GetAliveEntity(selectedId - 1);
}

// ---------------------------------------------------------------------------
// IDebugDomain: panel bridge
// ---------------------------------------------------------------------------

void AttributeVisualDebugger::GetJSONState(Json::Value& out)
{
    // Fold the (cross-thread) toggle into the dirty flag on this thread only.
    const bool enabled = mInspectorEnabled.load();
    if (enabled != mLastEnabledSeen)
    {
        mLastEnabledSeen = enabled;
        mDirty           = true;
    }

    Json::Value drawers(Json::arrayValue);
    {
        Json::Value entry(Json::objectValue);
        entry["name"]    = "AttributeInspector";
        entry["enabled"] = enabled;
        drawers.append(entry);
    }

    const Dia::Entity::Entity selected = ResolveSelectedEntity();

    Dia::Attribute::AttributeSetComponent* comp =
        selected.IsValid() ? mDomain.GetComponent<Dia::Attribute::AttributeSetComponent>(selected)
                           : nullptr;

    if (comp != mObservedComponent)
        RebindObserver(comp); // selection changed — resubscribe and force a rebuild

    // AC-5: bounded re-check of the selected entity's conditional modifiers only.
    // Skipped entirely when there is nothing to show, so this is never a global tick.
    bool pollTriggered = false;
    if (comp != nullptr && enabled)
    {
        ++mFrameCounter;
        if (mFrameCounter >= kConditionalPollIntervalFrames)
        {
            mFrameCounter = 0;
            pollTriggered = PollConditionalModifiersChanged(*comp);
        }
    }

    // AC-4: the tree is rebuilt only on a push notification, a selection change, a
    // toggle, or a poll-detected condition flip — never merely because a frame passed.
    if (mDirty || pollTriggered)
    {
        if (comp != nullptr)
        {
            RebuildJSONState(selected, *comp);
        }
        else
        {
            mCachedState = Json::Value(Json::objectValue);
            mCachedState["hasSelection"]    = selected.IsValid();
            mCachedState["hasAttributeSet"] = false;
            mCachedState["attributes"]      = Json::Value(Json::arrayValue);
            mConditionCache.RemoveAll();
        }

        ++mRebuildCount;
        mDirty = false;
    }

    out["drawers"] = drawers;
    out["stats"]   = mCachedState;
}

void AttributeVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString())
            return;

        const Dia::Core::StringCRC drawerName(args["drawer"].asCString());
        if (drawerName == kDrawerAttributeInspector)
            mInspectorEnabled.store(!mInspectorEnabled.load());

        return;
    }

    // "setScale" and every other command — no-op, never crash.
}

// ---------------------------------------------------------------------------
// Dia::Attribute::IAttributeObserver — the entire push path (AC-4)
// ---------------------------------------------------------------------------

void AttributeVisualDebugger::OnAttributeChanged(const Dia::Attribute::AttributeChangedEvent&)
{
    // The event only says "something on the observed set changed". A full rebuild on
    // the next GetJSONState is simpler than an incremental patch and cheap for one
    // entity's worth of attributes.
    mDirty = true;
}

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------

void AttributeVisualDebugger::RebindObserver(Dia::Attribute::AttributeSetComponent* comp)
{
    if (mObservedComponent != nullptr)
        mObservedComponent->GetAttributeSet().GetObserverSubject().Unsubscribe(this);

    mObservedComponent = comp;

    if (mObservedComponent != nullptr)
        mObservedComponent->GetAttributeSet().GetObserverSubject().Subscribe(this);

    // The selection itself changed, so the cached tree is meaningless regardless of
    // whether any push or poll signal arrived.
    mConditionCache.RemoveAll();
    mDirty = true;
}

void AttributeVisualDebugger::RebuildJSONState(Dia::Entity::Entity                    entity,
                                               Dia::Attribute::AttributeSetComponent& comp)
{
    const Dia::Attribute::AttributeSet& set = comp.GetAttributeSet();

    mCachedState = Json::Value(Json::objectValue);
    mCachedState["hasSelection"]    = true;
    mCachedState["hasAttributeSet"] = true;
    mCachedState["entityIndex"]     = static_cast<Json::UInt>(entity.GetIndex());
    mCachedState["schemaName"]      = SafeName(set.GetSchemaName());
    mCachedState["attributeCount"]  = static_cast<Json::UInt>(set.GetAttributeCount());

    Json::Value attributes(Json::arrayValue);

    // Nothing is displayed while the drawer is toggled off, so skip the walk entirely.
    if (mLastEnabledSeen)
    {
        const unsigned int attributeCount = set.GetAttributeCount();
        for (unsigned int i = 0; i < attributeCount; ++i)
        {
            const Dia::Core::StringCRC name = set.GetAttributeNameByIndex(i);

            Json::Value attribute(Json::objectValue);
            attribute["name"]      = SafeName(name);
            attribute["value"]     = static_cast<double>(set.GetValueByIndex(i));
            attribute["baseValue"] = static_cast<double>(set.GetBaseValue(name));
            attribute["minimum"]   = static_cast<double>(set.GetMinimumValueByIndex(i));
            attribute["maximum"]   = static_cast<double>(set.GetMaximumValueByIndex(i));

            Json::Value modifiers(Json::arrayValue);
            const unsigned int modifierCount = set.GetModifierCountForAttribute(name);
            for (unsigned int m = 0; m < modifierCount; ++m)
            {
                const Dia::Attribute::ModifierSnapshot snap = set.GetModifierSnapshotForAttribute(name, m);

                Json::Value modifier(Json::objectValue);
                modifier["modifierName"]  = SafeName(snap.modifier_name);
                modifier["operation"]     = OperationToString(snap.operation);
                modifier["value"]         = static_cast<double>(snap.value);
                modifier["isConditional"] = snap.isConditional;
                modifier["conditionTrue"] = snap.conditionCurrentlyTrue;
                modifiers.append(modifier);
            }

            attribute["modifiers"] = modifiers;
            attributes.append(attribute);
        }
    }

    mCachedState["attributes"] = attributes;

    // Keep the poll baseline in lockstep with what the panel now shows, so the next
    // poll reports a flip only if the condition really changed after this rebuild.
    RefreshConditionalCache(set);
}

void AttributeVisualDebugger::RefreshConditionalCache(const Dia::Attribute::AttributeSet& set)
{
    mConditionCache.RemoveAll();

    const unsigned int attributeCount = set.GetAttributeCount();
    for (unsigned int i = 0; i < attributeCount; ++i)
    {
        const Dia::Core::StringCRC name = set.GetAttributeNameByIndex(i);

        const unsigned int modifierCount = set.GetModifierCountForAttribute(name);
        for (unsigned int m = 0; m < modifierCount; ++m)
        {
            const Dia::Attribute::ModifierSnapshot snap = set.GetModifierSnapshotForAttribute(name, m);
            if (!snap.isConditional)
                continue;

            if (mConditionCache.IsFull())
                return; // bounded tracking — see kMaxTrackedConditionalModifiers

            ConditionCacheEntry entry;
            entry.attributeName = name;
            entry.modifierName  = snap.modifier_name;
            entry.lastTrue      = snap.conditionCurrentlyTrue;
            mConditionCache.Add(entry);
        }
    }
}

bool AttributeVisualDebugger::PollConditionalModifiersChanged(Dia::Attribute::AttributeSetComponent& comp)
{
    const Dia::Attribute::AttributeSet& set = comp.GetAttributeSet();

    // Pass 1 — walk the conditional modifiers in the same stable order the cache was
    // built in, comparing as we go. A modifier added or removed since the last refresh
    // shows up as a name mismatch or a count mismatch, both of which count as a change.
    bool         changed = false;
    unsigned int index   = 0;

    const unsigned int attributeCount = set.GetAttributeCount();
    for (unsigned int i = 0; i < attributeCount && !changed; ++i)
    {
        const Dia::Core::StringCRC name = set.GetAttributeNameByIndex(i);

        const unsigned int modifierCount = set.GetModifierCountForAttribute(name);
        for (unsigned int m = 0; m < modifierCount; ++m)
        {
            const Dia::Attribute::ModifierSnapshot snap = set.GetModifierSnapshotForAttribute(name, m);
            if (!snap.isConditional)
                continue;

            if (index >= mConditionCache.Size())
            {
                changed = true;
                break;
            }

            const ConditionCacheEntry& cached = mConditionCache[index];
            if (cached.attributeName != name ||
                cached.modifierName  != snap.modifier_name ||
                cached.lastTrue      != snap.conditionCurrentlyTrue)
            {
                changed = true;
                break;
            }

            ++index;
        }
    }

    if (!changed && index != mConditionCache.Size())
        changed = true; // a conditional modifier was removed

    // Pass 2 — refresh the baseline either way, so a flip is reported exactly once.
    RefreshConditionalCache(set);

    return changed;
}

} // namespace Dia::AttributeVisualDebugger

#endif // DIA_DEBUG
