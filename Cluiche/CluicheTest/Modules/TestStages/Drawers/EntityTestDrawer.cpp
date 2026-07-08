#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/EntityTestDrawer.h"
#include "Modules/TestStages/Entity/TransformComponent.h"
#include "Modules/TestStages/Entity/VisualTestRenderComponent.h"
#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <imgui.h>

namespace CluicheTest {

static Dia::Graphics::RGBA ColourFromUint32(uint32_t rgba)
{
    return Dia::Graphics::RGBA(
        static_cast<uint8_t>((rgba >> 24) & 0xFF),
        static_cast<uint8_t>((rgba >> 16) & 0xFF),
        static_cast<uint8_t>((rgba >>  8) & 0xFF),
        static_cast<uint8_t>((rgba      ) & 0xFF));
}

EntityTestDrawer::EntityTestDrawer(
    Dia::Entity::Domain&              domain,
    Dia::Entity::Entity               parent,
    Dia::Entity::Entity               childA,
    Dia::Entity::Entity               childB,
    Dia::Entity::Entity               childC,
    Dia::Entity::Entity               queryEntities[4],
    Dia::Entity::Entity               doomed,
    const bool&                       doomedDestroyed,
    const Dia::Debug::DebugLayerManager& mgr)
    : mDomain(domain)
    , mParent(parent)
    , mChildA(childA)
    , mChildB(childB)
    , mChildC(childC)
    , mDoomed(doomed)
    , mDoomedDestroyed(doomedDestroyed)
    , mManager(mgr)
    , mDrawer(mgr)
{
    mQueryEntities[0] = queryEntities[0];
    mQueryEntities[1] = queryEntities[1];
    mQueryEntities[2] = queryEntities[2];
    mQueryEntities[3] = queryEntities[3];
}

Dia::Core::StringCRC EntityTestDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("entity.shapes");
}

void EntityTestDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    auto submitEntity = [&](Dia::Entity::Entity e)
    {
        if (!mDomain.IsAlive(e)) return;
        auto* tc  = mDomain.GetComponent<TransformComponent>(e);
        auto* vrc = mDomain.GetComponent<VisualTestRenderComponent>(e);
        if (!tc || !vrc) return;

        Dia::Geometry2D::Circle circle(vrc->radius, Dia::Maths::Vector2D(tc->x, tc->y));
        mDrawer.SubmitCircle(circle, ColourFromUint32(vrc->colour));
    };

    submitEntity(mParent);
    submitEntity(mChildA);
    submitEntity(mChildB);
    submitEntity(mChildC);

    for (int i = 0; i < 4; ++i)
        submitEntity(mQueryEntities[i]);

    if (!mDoomedDestroyed)
        submitEntity(mDoomed);

    mDrawer.Draw(draw);
}

void EntityTestDrawer::DrawImGui()
{
    // Entity inspection and hierarchy are now provided by EntityVisualDebuggerModule.
}

} // namespace CluicheTest

#endif // DIA_DEBUG
