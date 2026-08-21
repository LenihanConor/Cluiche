# Feature Spec: clip-completion-bus-adapter

**System:** DiaAnimation2D
**App:** Dia
**Status:** Approved

## Summary

`AnimClipPlayer` (Done) is pure poll today — `Play`/`Stop`/`Update`/`IsPlaying`/`GetNormalizedTime` — with no completion event of any kind, internal or external. Unlike every other feature in this effort, there's nothing existing to bridge; this feature has to add the detection logic itself before there's anything to observe or broadcast. It then adds `IAnimClipObserver` (new interface, same shape as the other systems' observers) and `AnimClipBusAdapter` forwarding to the shared `DiaMessageBus::Bus`.

**This feature surfaces a bigger gap than the others: `AnimClipPlayer` has no `IComponent` wrapper.** Every other per-entity adapter in this effort (`BehaviourTreeBusAdapter`) gets its owning entity's handle from an existing component attach point. No `AnimationComponent2D` (or equivalent) exists — whoever uses `AnimClipPlayer` today owns it as a plain member, with no established association to an entity. This feature does **not** create that component wrapper (that's a larger architecture decision, out of scope here per the engine/application escalation rule) — instead, entity addressing is left to the caller, same as the adapter constructor pattern, but with no guarantee an entity handle is even available.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../dia.md) |
| System | DiaAnimation2D — [diaanimation2d.md](diaanimation2d.md) (Done) |
| Depends on system | [diamessagebus.md](../diamessagebus/diamessagebus.md) — requires `core-bus` |

## Goals

- `AnimClipPlayer` can notify when a one-shot clip finishes, and (secondary) when a looping clip completes a cycle.
- Other systems (e.g. gameplay logic waiting on an attack animation to finish) can subscribe via the bus instead of polling `IsPlaying()`/`GetNormalizedTime()` every frame.
- No assumption is baked in that every `AnimClipPlayer` has an owning entity — the design must work whether or not one exists.

## Non-Goals

- Creating `AnimationComponent2D` or any `IComponent` wrapper for `AnimClipPlayer`. If that's wanted, it's a separate, larger feature — flag and confirm placement before starting, per the engine/application escalation rule.
- Firing completion events for every animation-adjacent thing (e.g. `PoseBlendStack`, `SpringChain`). Scoped to `AnimClipPlayer` only.

## Acceptance Criteria

- `AnimClipPlayer::Update(dt)` detects the transition from playing to finished for one-shot clips (i.e. `mIsPlaying` going `true→false` because normalized time reached 1.0, not because `Stop()` was called externally — those are different signals and must not be conflated).
- `AnimClipPlayer::Update(dt)` detects each loop wrap for looping clips (normalized time crossing 1.0 and resetting) as a secondary, separate event.
- `IAnimClipObserver` is a new interface: `OnClipFinished(const AnimClip& clip)` (one-shot completion), `OnClipLooped(const AnimClip& clip)` (loop wrap) — default no-op virtuals.
- `AnimClipPlayer` gains `Subscribe`/`Unsubscribe` for `IAnimClipObserver`, composing a subject the same shape as `EconomyObserverSubject`/`CalloutObserverSubject`.
- `AnimClipBusAdapter : IAnimClipObserver` forwards both events to the bus. Its constructor accepts an **optional** addressing target: if the caller supplies an entity handle, post to `{Bus::kEntityRouterId, handle.bits}`; if not, `Broadcast`. The adapter does not require an entity handle to exist.
- `ClipFinishedEvent`/`ClipLoopedEvent` are declared in a `.diagamemessages` file under `DiaAnimation2D` and generated via `dia codegen messages`.
- Calling `Stop()` explicitly does **not** fire `OnClipFinished` — only reaching the natural end of a one-shot clip does. This distinction must have a test.

## Design

### Completion detection

```cpp
// Inside AnimClipPlayer::Update, after advancing mCurrentTime:
bool wasPlaying = mIsPlaying;
// ... existing time-advance logic ...
if (mMode == PlaybackMode::kOneShot && wasPlaying && !mIsPlaying) {
    // reached the end naturally (not via Stop())
    mObservers.NotifyClipFinished(*mClip);
}
if (mMode == PlaybackMode::kLooping && /* wrapped this Update call */) {
    mObservers.NotifyClipLooped(*mClip);
}
```

The one-shot case needs care: `mIsPlaying` must only flip to `false` inside `Update` when the clip naturally ends, not when `Stop()` is called — `Stop()` already sets `mIsPlaying = false` directly today, so the "did we just finish vs. were we just stopped" distinction needs an explicit check (e.g. only fire when `Update` itself is what caused the flip, not any external call this same frame).

### Adapter — addressing is caller-supplied, not assumed

```cpp
class AnimClipBusAdapter : public Dia::Animation2D::IAnimClipObserver {
public:
    // owner is optional — pass an invalid/default Entity if none exists.
    AnimClipBusAdapter(Dia::MessageBus::Bus& bus, Dia::Entity::Entity owner = {})
        : mBus(bus), mOwner(owner) {}

    void OnClipFinished(const AnimClip& clip) override {
        ClipFinishedEvent e{ /* clip identity */ };
        if (mOwner.IsValid()) mBus.Post(e, { Bus::kEntityRouterId, mOwner.bits() });
        else                  mBus.Broadcast(e);
    }
    void OnClipLooped(const AnimClip& clip) override {
        ClipLoopedEvent e{ /* clip identity */ };
        if (mOwner.IsValid()) mBus.Post(e, { Bus::kEntityRouterId, mOwner.bits() });
        else                  mBus.Broadcast(e);
    }

private:
    Dia::MessageBus::Bus& mBus;
    Dia::Entity::Entity   mOwner;
};
```

`Broadcast` is the primary, intended delivery mode, not a fallback — see Resolved Design Decisions below. Entity-addressed delivery is available opportunistically to any caller that already holds both an `AnimClipPlayer` and an entity handle, with no component wrapper required to use it.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaAnimation2D/IAnimClipObserver.h` | New |
| `Dia/DiaAnimation2D/AnimClipPlayer.h` / `.cpp` | Modify — add completion/loop detection, `Subscribe`/`Unsubscribe`, compose observer subject |
| `Dia/DiaAnimation2D/AnimClipBusAdapter.h` / `.cpp` | New |
| `Dia/DiaAnimation2D/Messages/animation2d_messages.diagamemessages` | New — two event declarations |
| `Dia/DiaAnimation2D/DiaAnimation2D.vcxproj` | Add new files; add `DiaMessageBus` reference (adapter only) |
| `Tests/GoogleTests/Animation2D/ClipCompletionBusAdapterTests.cpp` | New — must cover the `Stop()`-vs-natural-finish distinction explicitly |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-004 | No STL in public APIs | `IAnimClipObserver` uses `AnimClip` value types; observer subject uses fixed-capacity `DynamicArrayC`. |
| Escalation rule (CLAUDE.md) | New engine implementation surfaced while working in an application context must stop and flag placement | This spec explicitly does not create `AnimationComponent2D` — resolved as out of scope, see Resolved Design Decisions. |

## Resolved Design Decisions

1. ~~**Does `AnimationComponent2D` (or equivalent) need to exist before this feature is genuinely useful for entity-addressed delivery?**~~ — **Resolved: no, and not in scope for this feature.** `Broadcast` is the primary, intended delivery mode, not a workaround — "an animation finished" is a legitimate thing to broadcast when no consumer has proven it needs to know *whose*. Unlike `BehaviourTreeComponent`, there is no existing per-entity listener mechanism for `AnimClipPlayer` proving a real need for entity-scoped delivery; building `AnimationComponent2D` now would be designing for a hypothetical, and it's materially bigger than this feature's scope (component registration, factory, lifecycle). Entity-addressed delivery stays available to any caller that already holds both an `AnimClipPlayer` and an entity handle — no component required — but building that component is deferred until a real consumer needs entity-scoped animation events.

## Open Design Questions

1. **Is loop-completion notification (`OnClipLooped`) actually wanted, or is it noise?** For a looping idle animation ticking many times a second, this could be a high-frequency broadcast for very little value. Recommend: implement the hook but leave it unsubscribed by default in any example wiring — let a real consumer justify turning it on.
2. **What identifies a clip in the event payload** — the `AnimClip*` pointer (unsafe to carry into a queued bus message, same class of risk as DiaEconomy's original raw-pointer issue) or a stable clip ID? `AnimClip` needs a stable identifier field checked before finalizing `ClipFinishedEvent`'s shape.

## Status

`Approved`
