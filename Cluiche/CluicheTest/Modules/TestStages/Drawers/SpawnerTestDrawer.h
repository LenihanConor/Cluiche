#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/Entity.h>
#include <DiaEntitySpawner/EntitySpawnerImpl.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Colour/RGBA.h>
#include <unordered_map>

namespace Dia::Debug  { class DebugLayerManager; }
namespace Dia::Core   { class IDebugDraw; }

namespace CluicheTest {

// Per-child visual state owned by the drawer.
struct SpawnerChildVisual
{
    Dia::Maths::Vector2D direction;            // unit vector, fixed at birth
    float                speed     = 0.0f;    // pixels/sec
    float                birthTime = 0.0f;    // absolute elapsed seconds at spawn
};

class SpawnerTestDrawer : public Dia::Debug::IVisualDebugger
{
public:
    SpawnerTestDrawer(
        Dia::EntitySpawner::EntitySpawnerImpl& impl,
        Dia::Entity::Entity                    rateEmitter,
        Dia::Entity::Entity                    burstEmitter,
        Dia::Entity::Entity                    capEmitter,
        Dia::Entity::Entity                    explicitEmitter,
        const float&                           elapsed,
        const Dia::Debug::DebugLayerManager&   mgr);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

    // Called by the stage's OnDespawn callback so we can evict dead children.
    void OnChildDespawned(Dia::Entity::Entity entity);

    // Called when a new child is born (forwarded from the module's spawn-count tracker).
    // childIndex is the sequential spawn index for this emitter, used to spread directions.
    void OnChildSpawned(Dia::Entity::Entity entity,
                        Dia::Entity::Entity emitter,
                        unsigned int        childIndex);

private:
    struct EmitterDesc {
        Dia::Entity::Entity      entity;
        Dia::Maths::Vector2D     screenPos;
        Dia::Core::RGBA          colour;
        const char*              label = nullptr;
    };

    void DrawEmitter(Dia::Core::IDebugDraw& draw, const EmitterDesc& desc);
    void DrawChildren(Dia::Core::IDebugDraw& draw, const EmitterDesc& desc);

    Dia::EntitySpawner::EntitySpawnerImpl& mImpl;
    EmitterDesc mEmitters[4];

    const float& mElapsed;

    // Child visuals keyed by entity index (stable over entity lifetime).
    std::unordered_map<unsigned int, SpawnerChildVisual> mChildVisuals;

    // Per-emitter spawn counter for direction spreading.
    std::unordered_map<unsigned int, unsigned int> mEmitterSpawnCount;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
