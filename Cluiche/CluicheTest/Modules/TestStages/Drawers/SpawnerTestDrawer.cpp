#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/SpawnerTestDrawer.h"

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaEntitySpawner/SpawnEmitterComponent.h>
#include <imgui.h>

#include <cmath>
#include <cstdio>

static constexpr float kPi = 3.14159265f;

// Emitter screen positions (world-space quadrants, centred around origin).
static constexpr float kQuadrantSpread = 220.0f;
static const Dia::Maths::Vector2D kTopLeft    (-kQuadrantSpread,  kQuadrantSpread);
static const Dia::Maths::Vector2D kTopRight   ( kQuadrantSpread,  kQuadrantSpread);
static const Dia::Maths::Vector2D kBottomLeft (-kQuadrantSpread, -kQuadrantSpread);
static const Dia::Maths::Vector2D kBottomRight( kQuadrantSpread, -kQuadrantSpread);

static constexpr float kEmitterRadius = 18.0f;
static constexpr float kChildRadius   =  7.0f;

// Colours
static const Dia::Core::RGBA kGreen   ( 80, 200,  80, 255);
static const Dia::Core::RGBA kYellow  (230, 200,  40, 255);
static const Dia::Core::RGBA kOrange  (230, 130,  30, 255);
static const Dia::Core::RGBA kRed     (220,  50,  50, 255);
static const Dia::Core::RGBA kDimWhite(200, 200, 200, 120);

// Lerp a colour toward transparent based on a 0..1 lifetime fraction.
static Dia::Core::RGBA FadeColour(Dia::Core::RGBA c, float lifetimeFrac)
{
    float alpha = 1.0f - lifetimeFrac * 0.7f;
    if (alpha < 0.0f) alpha = 0.0f;
    return Dia::Core::RGBA(c.R(), c.G(), c.B(),
        static_cast<unsigned char>(c.A() * alpha));
}

namespace CluicheTest {

SpawnerTestDrawer::SpawnerTestDrawer(
    Dia::EntitySpawner::EntitySpawnerImpl& impl,
    Dia::Entity::Entity                    rateEmitter,
    Dia::Entity::Entity                    burstEmitter,
    Dia::Entity::Entity                    capEmitter,
    Dia::Entity::Entity                    explicitEmitter,
    const float&                           elapsed,
    const Dia::Debug::DebugLayerManager&   /*mgr*/)
    : mImpl(impl)
    , mElapsed(elapsed)
{
    mEmitters[0] = { rateEmitter,     kTopLeft,     kGreen,  "Rate"     };
    mEmitters[1] = { burstEmitter,    kTopRight,    kYellow, "Burst"    };
    mEmitters[2] = { capEmitter,      kBottomLeft,  kOrange, "Cap=3"    };
    mEmitters[3] = { explicitEmitter, kBottomRight, kRed,    "Explicit" };
}

Dia::Core::StringCRC SpawnerTestDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("spawner.overview");
}

void SpawnerTestDrawer::OnChildDespawned(Dia::Entity::Entity entity)
{
    mChildVisuals.erase(entity.GetIndex());
}

void SpawnerTestDrawer::OnChildSpawned(Dia::Entity::Entity entity,
                                       Dia::Entity::Entity emitter,
                                       unsigned int        childIndex)
{
    // Early out if already registered (re-notification guard).
    if (mChildVisuals.count(entity.GetIndex()) != 0)
        return;

    unsigned int emitterIdx = emitter.GetIndex();
    (void)childIndex; // we use our own per-emitter counter
    unsigned int spawnCount = mEmitterSpawnCount[emitterIdx]++;

    // Choose speed based on which emitter this is.
    float speed = 55.0f;
    if (emitter == mEmitters[1].entity)      // burst — fast initial velocity
        speed = 110.0f;
    else if (emitter == mEmitters[2].entity) // cap — medium
        speed = 70.0f;
    else if (emitter == mEmitters[3].entity) // explicit — slow orbit baked as drift
        speed = 25.0f;

    // Spread direction evenly in a full circle using spawn index.
    float angleStep = (2.0f * kPi) / 8.0f;   // 8 slots covers small burst counts well
    float angle = angleStep * static_cast<float>(spawnCount);

    // Burst emitter fans 5 evenly across full circle.
    if (emitter == mEmitters[1].entity)
        angle = (2.0f * kPi / 5.0f) * static_cast<float>(spawnCount % 5);

    // Explicit emitter: two children orbit in opposing directions.
    if (emitter == mEmitters[3].entity)
        angle = (spawnCount % 2 == 0) ? 0.0f : kPi;

    SpawnerChildVisual v;
    v.direction = Dia::Maths::Vector2D(std::cos(angle), std::sin(angle));
    v.speed     = speed;
    v.birthTime = mElapsed;

    // try_emplace: no-op if already registered, so re-notification is safe.
    mChildVisuals.try_emplace(entity.GetIndex(), v);
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void SpawnerTestDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    for (const auto& desc : mEmitters)
    {
        DrawEmitter(draw, desc);
        DrawChildren(draw, desc);
    }

    // DespawnAll marker: at ~t=8s draw a brief flash ring on every emitter.
    if (mElapsed >= 8.0f && mElapsed < 8.5f)
    {
        float frac = (mElapsed - 8.0f) / 0.5f;   // 0..1
        float expandR = kEmitterRadius + frac * 60.0f;
        unsigned char alpha = static_cast<unsigned char>(255 * (1.0f - frac));
        Dia::Core::RGBA flash(255, 255, 255, alpha);
        for (const auto& desc : mEmitters)
            draw.RequestDraw(desc.screenPos, expandR, flash);
    }
}

void SpawnerTestDrawer::DrawEmitter(Dia::Core::IDebugDraw& draw, const EmitterDesc& desc)
{
    // Pulsing emitter ring — pulses once per second.
    float pulse = 0.5f + 0.5f * std::sin(mElapsed * 2.0f * kPi);
    float r = kEmitterRadius + pulse * 4.0f;
    draw.RequestDraw(desc.screenPos, r, desc.colour,
        Dia::Core::RGBA(desc.colour.R(), desc.colour.G(), desc.colour.B(), 60));

    // Label below.
    Dia::Maths::Vector2D labelPos(desc.screenPos.X() - 20.0f, desc.screenPos.Y() - kEmitterRadius - 14.0f);
    draw.RequestDrawText(labelPos, desc.label, 11.0f, desc.colour);

    // Live child count above.
    const Dia::EntitySpawner::EmitterState& state = mImpl.GetOrCreateEmitterState(desc.entity);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u", state.children.Size());
    Dia::Maths::Vector2D countPos(desc.screenPos.X() - 4.0f, desc.screenPos.Y() + kEmitterRadius + 6.0f);
    draw.RequestDrawText(countPos, buf, 11.0f, kDimWhite);
}

void SpawnerTestDrawer::DrawChildren(Dia::Core::IDebugDraw& draw, const EmitterDesc& desc)
{
    const Dia::EntitySpawner::EmitterState& state = mImpl.GetOrCreateEmitterState(desc.entity);

    for (unsigned int i = 0; i < state.children.Size(); ++i)
    {
        Dia::Entity::Entity child = state.children[i];
        auto it = mChildVisuals.find(child.GetIndex());
        if (it == mChildVisuals.end())
            continue;

        const SpawnerChildVisual& v = it->second;
        float age = mElapsed - v.birthTime;
        if (age < 0.0f) age = 0.0f;

        Dia::Maths::Vector2D pos;

        if (desc.entity == mEmitters[1].entity)
        {
            // Burst: fast outward then decelerate and drift back slightly.
            float t = age;
            float decel = v.speed * t - 0.5f * 40.0f * t * t;
            if (decel < 0.0f) decel = 0.0f;
            pos = desc.screenPos + v.direction * decel;
        }
        else if (desc.entity == mEmitters[2].entity)
        {
            // Cap: children spiral outward in a gentle arc.
            float dist = v.speed * age;
            float arc  = age * 1.2f;   // rotation over time
            float dx = v.direction.X() * std::cos(arc) - v.direction.Y() * std::sin(arc);
            float dy = v.direction.X() * std::sin(arc) + v.direction.Y() * std::cos(arc);
            pos = desc.screenPos + Dia::Maths::Vector2D(dx, dy) * dist;
        }
        else if (desc.entity == mEmitters[3].entity)
        {
            // Explicit: slow circular orbit around the emitter.
            float orbitR   = 40.0f;
            float orbitDir = (child.GetIndex() % 2 == 0) ? 1.0f : -1.0f;
            float angle    = orbitDir * age * 1.5f;
            pos = desc.screenPos + Dia::Maths::Vector2D(
                std::cos(angle) * orbitR,
                std::sin(angle) * orbitR);
        }
        else
        {
            // Rate: linear drift outward, fading toward lifetime end.
            pos = desc.screenPos + v.direction * (v.speed * age);
        }

        // Compute lifetime fraction for alpha fade (rate emitter has 3s lifetime).
        float lifetimeFrac = 0.0f;
        if (desc.entity == mEmitters[0].entity)
            lifetimeFrac = age / 3.0f;
        else if (desc.entity == mEmitters[1].entity)
            lifetimeFrac = age / 5.0f;

        Dia::Core::RGBA col = FadeColour(desc.colour, lifetimeFrac);
        draw.RequestDraw(pos, kChildRadius, col,
            Dia::Core::RGBA(col.R(), col.G(), col.B(), col.A() / 3));

        // Line from emitter to child.
        draw.RequestDraw(desc.screenPos, pos,
            Dia::Core::RGBA(col.R(), col.G(), col.B(), col.A() / 4));
    }
}

void SpawnerTestDrawer::DrawImGui()
{
    ImGui::Text("Rate emitter:     steady stream, 3s lifetime, fades");
    ImGui::Text("Burst emitter:    5 children explode out then drift back");
    ImGui::Text("Cap emitter:      spiral, FIFO pops oldest when cap=3 hit");
    ImGui::Text("Explicit emitter: 2 children orbit until DespawnAll at t=8s");
}

} // namespace CluicheTest

#endif // DIA_DEBUG
